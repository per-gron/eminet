//
//  EmiSendQueue.h
//  roshambo
//
//  Created by Per Eckerdal on 2012-02-16.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#ifndef emilir_EmiSendQueue_h
#define emilir_EmiSendQueue_h

#include "EmiMessage.h"
#include "EmiNetUtil.h"

#include <arpa/inet.h>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <cmath>

template<class SockDelegate, class ConnDelegate>
class EmiConn;

typedef void (^SendSynRstAckPacketCallback)(uint8_t *buf, size_t size);

template<class SockDelegate, class ConnDelegate>
class EmiSendQueue {
    typedef typename SockDelegate::SendData SendData;
    
    typedef std::vector<EmiMessage<SockDelegate> *> EmiSendQueueVector;
    typedef typename std::vector<EmiMessage<SockDelegate> *>::iterator EmiSendQueueVectorIter;
    typedef std::map<EmiChannelQualifier, EmiSequenceNumber> EmiSendQueueAcksMap;
    typedef std::set<EmiChannelQualifier> EmiSendQueueAcksSet;
    typedef EmiConn<SockDelegate, ConnDelegate> EC;
    
    EC& _conn;
    
    EmiSendQueueVector _queue;
    size_t _queueSize;
    EmiSendQueueAcksMap _acks;
    size_t _bufLength;
    uint8_t *_buf;
    bool _enqueueHeartbeat;
    
private:
    // Private copy constructor and assignment operator
    inline EmiSendQueue(const EmiSendQueue& other);
    inline EmiSendQueue& operator=(const EmiSendQueue& other);
    
    void clearQueue() {
        EmiSendQueueVectorIter iter = _queue.begin();
        EmiSendQueueVectorIter end = _queue.end();
        while (iter != end) {
            (*iter)->release();
            ++iter;
        }
        _queue.clear();
        _queueSize = 0;
    }
    void fillTimestamps(void *data, EmiTimeInterval now) {
        uint16_t *buf = (uint16_t *)data;
        
        buf[0] = htons(floor(_conn.getCurrentTime(now)*1000));
        if (_conn.hasReceivedTime()) {
            buf[1] = htons(_conn.largestReceivedTime());
            buf[2] = htons(floor((now - _conn.gotLargestReceivedTimeAt())*1000));
        }
        else {
            buf[1] = htons(0);
            buf[2] = htons(0);
        }
    }
    
    static size_t writeMsg(uint8_t *buf,
                           size_t bufSize,
                           size_t offset,
                           bool hasAck,
                           EmiSequenceNumber ack,
                           int32_t channelQualifier,
                           EmiSequenceNumber sequenceNumber,
                           const SendData& data,
                           EmiFlags flags) {
        const size_t msgLength = SockDelegate::extractLength(data);
        size_t pos = offset;
        
        flags |= (hasAck ? EMI_ACK_FLAG : 0); // SYN/RST/ACK flags
        
        if (0 == flags && 0 == msgLength) {
            return 0;
        }
        
        *((uint8_t*)  (buf+pos)) = flags; pos += 1;
        *((uint8_t*)  (buf+pos)) = std::max(0, channelQualifier); pos += 1; // Channel qualifier. cq == -1 means SYN/RST message
        *((uint16_t*) (buf+pos)) = htons(msgLength); pos += 2;
        if (0 != msgLength || flags & EMI_SYN_FLAG) {
            *((uint16_t*) (buf+pos)) = htons(sequenceNumber); pos += 2;
        }
        if (0 != msgLength) {
            *((uint8_t*) (buf+pos)) = 0; pos += 1; // Split id
        }
        if (hasAck) {
            *((uint16_t*) (buf+pos)) = htons(ack); pos += 2;
        }
        if (msgLength) {
            if (pos+msgLength > bufSize) {
                // The data doesn't fit in the buffer
                return 0;
            }
            memcpy(buf+pos, SockDelegate::extractData(data), msgLength); pos += msgLength;
        }
        
        return pos-offset;
    }
    
public:
    
    EmiSendQueue(EC& conn) :
    _conn(conn),
    _enqueueHeartbeat(false),
    _queueSize(0) {
        _bufLength = conn.getEmiSock().config.mtu;
        _buf = (uint8_t *)malloc(_bufLength);
    }
    virtual ~EmiSendQueue() {
        clearQueue();
        
        _enqueueHeartbeat = false;
        
        if (NULL != _buf) {
            _bufLength = 0;
            free(_buf);
            _buf = NULL;
        }
    }
    
    void enqueueHeartbeat() {
        _enqueueHeartbeat = true;
    }
    void sendHeartbeat(bool isResponse, EmiTimeInterval now) {
        if (_conn.isOpen()) {
            const size_t bufLen = EMI_TIMESTAMP_LENGTH+1;
            uint8_t buf[bufLen];
            fillTimestamps(buf, now);
            buf[EMI_TIMESTAMP_LENGTH] = isResponse ? 1 : 0;
            
            _conn.sendDatagram(buf, bufLen);
        }
    }
    static void sendSynRstAckPacket(SendSynRstAckPacketCallback callback) {
        const int BUF_SIZE = 80; // 80 ought to be plenty
        uint8_t buf[BUF_SIZE];
        
        // Zero out the timestamp
        size_t tlen = EMI_TIMESTAMP_LENGTH;
        memset(buf, 0, tlen);
        
        // Propagate the actual packet data
        size_t plen;
        plen = EmiSendQueue::writeMsg(buf, /* buf */
                                      BUF_SIZE, /* bufSize */
                                      tlen, /* offset */
                                      false, /* hasAck */
                                      0, /* ack */
                                      -1, /* channelQualifier */
                                      0, /* sequenceNumber */
                                      SendData(), /* data */
                                      EMI_SYN_FLAG | EMI_RST_FLAG | EMI_ACK_FLAG);
        
        callback(buf, tlen+plen);
    }
    // Returns true if something was sent
    bool flush(EmiTimeInterval now) {
        bool sentPacket = false;
        
        if (!_queue.empty() || !_acks.empty()) {
            EmiSendQueueAcksSet acksInThisPacket;
            size_t pos = 0;
            
            fillTimestamps(_buf, now); pos += EMI_TIMESTAMP_LENGTH;
            
            EmiSendQueueAcksMap::iterator ackIter = _acks.begin();
            EmiSendQueueAcksMap::iterator ackEnd = _acks.end();
            
            EmiSendQueueVectorIter iter = _queue.begin();
            EmiSendQueueVectorIter end = _queue.end();
            while (iter != end) {
                EmiMessage<SockDelegate> *msg = *iter;
                
                EmiSendQueueAcksMap::iterator cur;
                if (0 != acksInThisPacket.count(msg->channelQualifier)) {
                    // Only send an ack for a particular channel once per packet
                    cur = ackEnd;
                }
                else {
                    cur = _acks.find(msg->channelQualifier);
                }
                acksInThisPacket.insert(msg->channelQualifier);
                
                bool hasAck = cur != ackEnd;
                pos += EmiSendQueue::writeMsg(_buf, /* buf */
                                              _bufLength, /* bufSize */
                                              pos, /* offset */
                                              hasAck, /* hasAck */
                                              hasAck && (*cur).second, /* ack */
                                              msg->channelQualifier,
                                              msg->sequenceNumber,
                                              msg->data,
                                              msg->flags);
                
                ++iter;
            }
            
            while (ackIter != ackEnd) {
                EmiChannelQualifier cq = (*ackIter).first;
                
                if (0 == acksInThisPacket.count(cq)) {
                    EmiSequenceNumber sn = (*ackIter).second;
                    
                    pos += EmiSendQueue::writeMsg(_buf, /* buf */
                                                  _bufLength, /* bufSize */
                                                  pos, /* offset */
                                                  true, /* hasAck */
                                                  sn, /* ack */
                                                  cq, /* channelQualifier */
                                                  0, /* sequenceNumber */
                                                  SendData(), /* data */
                                                  0 /* flags */);
                }
                
                ++ackIter;
            }
            
            if (EMI_TIMESTAMP_LENGTH != pos) {
                if (pos <= _bufLength) { // Just to be sure, this should always be true
                    _conn.sendDatagram(_buf, pos);
                    sentPacket = true;
                }
            }
        }
        
        if (!sentPacket && _enqueueHeartbeat) {
            // Send heartbeat response
            sendHeartbeat(true, now);
            sentPacket = true;
        }
        
        clearQueue();
        _enqueueHeartbeat = false;
        
        return sentPacket;
    }
    
    // Returns true if at least 1 ack is now enqueued
    bool enqueueAck(EmiChannelQualifier channelQualifier, EmiSequenceNumber sequenceNumber) {
        EmiSendQueueAcksMap::iterator ackCur = _acks.find(channelQualifier);
        EmiSendQueueAcksMap::iterator ackEnd = _acks.end();
        
        if (ackCur == ackEnd) {
            _acks[channelQualifier] = sequenceNumber;
        }
        else {
            _acks[channelQualifier] = emiCyclicMax16((*ackCur).second, sequenceNumber);
        }
        
        return !_acks.empty();
    }
    
    void enqueueMessage(EmiMessage<SockDelegate> *msg, EmiTimeInterval now) {
        if (EMI_PRIORITY_HIGH != msg->priority) {
            // Only EMI_PRIORITY_HIGH messages are implemented
            SockDelegate::panic();
            return;
        }
        
        size_t msgSize = msg->approximateSize();
        
        if (_queueSize + msgSize >= _bufLength) { // _bufLength is the MTU of the EmiSocket
            flush(now);
        }
        
        msg->retain();
        _queue.push_back(msg);
        _queueSize += msgSize;
    }
};

#endif
