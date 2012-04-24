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

template<class SockDelegate, class ConnDelegate>
class EmiSendQueue {
    typedef typename SockDelegate::Binding   Binding;
    typedef typename Binding::PersistentData PersistentData;
    typedef EmiMessage<Binding>              EM;
    
    typedef std::vector<EM *> SendQueueVector;
    typedef typename std::vector<EM *>::iterator SendQueueVectorIter;
    typedef std::map<EmiChannelQualifier, EmiSequenceNumber> SendQueueAcksMap;
    typedef std::set<EmiChannelQualifier> SendQueueAcksSet;
    typedef EmiConn<SockDelegate, ConnDelegate> EC;
    
    EC& _conn;
    
    SendQueueVector _queue;
    size_t _queueSize;
    SendQueueAcksMap _acks;
    size_t _bufLength;
    uint8_t *_buf;
    bool _enqueueHeartbeat;
    
private:
    // Private copy constructor and assignment operator
    inline EmiSendQueue(const EmiSendQueue& other);
    inline EmiSendQueue& operator=(const EmiSendQueue& other);
    
    void clearQueue() {
        SendQueueVectorIter iter = _queue.begin();
        SendQueueVectorIter end = _queue.end();
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
    
    void sendMessageInSeparatePacket(const EM *msg) {
        size_t tlen = EMI_TIMESTAMP_LENGTH;
        memset(_buf, 0, tlen);
        
        // Propagate the actual packet data
        size_t plen;
        plen = EM::writeMsg(_buf, /* buf */
                            _bufLength, /* bufSize */
                            tlen, /* offset */
                            false, /* hasAck */
                            0, /* ack */
                            msg->channelQualifier,
                            msg->sequenceNumber,
                            msg->data,
                            msg->flags);
        
        _conn.sendDatagram(_buf, tlen+plen);
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
    
    // Returns true if something was sent
    bool flush(EmiTimeInterval now) {
        bool sentPacket = false;
        
        if (!_queue.empty() || !_acks.empty()) {
            SendQueueAcksSet acksInThisPacket;
            size_t pos = 0;
            
            fillTimestamps(_buf, now); pos += EMI_TIMESTAMP_LENGTH;
            
            SendQueueAcksMap::iterator ackIter = _acks.begin();
            SendQueueAcksMap::iterator ackEnd = _acks.end();
            
            SendQueueVectorIter iter = _queue.begin();
            SendQueueVectorIter end = _queue.end();
            while (iter != end) {
                EM *msg = *iter;
                
                SendQueueAcksMap::iterator cur;
                if (0 != acksInThisPacket.count(msg->channelQualifier)) {
                    // Only send an ack for a particular channel once per packet
                    cur = ackEnd;
                }
                else {
                    cur = _acks.find(msg->channelQualifier);
                }
                acksInThisPacket.insert(msg->channelQualifier);
                
                bool hasAck = cur != ackEnd;
                pos += EM::writeMsg(_buf, /* buf */
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
                    
                    pos += EM::writeMsg(_buf, /* buf */
                                        _bufLength, /* bufSize */
                                        pos, /* offset */
                                        true, /* hasAck */
                                        sn, /* ack */
                                        cq, /* channelQualifier */
                                        0, /* sequenceNumber */
                                        PersistentData(), /* data */
                                        0 /* flags */);
                }
                
                ++ackIter;
            }
            
            if (EMI_TIMESTAMP_LENGTH != pos) {
                ASSERT(pos <= _bufLength);
                
                _conn.sendDatagram(_buf, pos);
                sentPacket = true;
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
        SendQueueAcksMap::iterator ackCur = _acks.find(channelQualifier);
        SendQueueAcksMap::iterator ackEnd = _acks.end();
        
        if (ackCur == ackEnd) {
            _acks[channelQualifier] = sequenceNumber;
        }
        else {
            _acks[channelQualifier] = emiCyclicMax16((*ackCur).second, sequenceNumber);
        }
        
        return !_acks.empty();
    }
    
    void enqueueMessage(EM *msg, EmiTimeInterval now) {
        if (msg->flags & (EMI_PRX_FLAG | EMI_RST_FLAG | EMI_SYN_FLAG)) {
            // This is a control message, one that cannot be bundled with
            // other messages. We might just as well send it right away.
            sendMessageInSeparatePacket(msg);
        }
        else {
            if (EMI_PRIORITY_HIGH != msg->priority) {
                // Only EMI_PRIORITY_HIGH messages are implemented
                Binding::panic();
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
    }
};

#endif
