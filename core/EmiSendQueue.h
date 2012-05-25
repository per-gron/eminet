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
#include "EmiPacketHeader.h"
#include "EmiCongestionControl.h"

#include <arpa/inet.h>
#include <deque>
#include <map>
#include <set>
#include <algorithm>
#include <cmath>

class EmiConnTime;

template<class SockDelegate, class ConnDelegate>
class EmiConn;

template<class SockDelegate, class ConnDelegate>
class EmiSendQueue {
    typedef typename SockDelegate::Binding   Binding;
    typedef typename Binding::Error          Error;
    typedef typename Binding::PersistentData PersistentData;
    typedef EmiMessage<Binding>              EM;
    
    typedef std::deque<EM *> SendQueueDeque;
    typedef typename SendQueueDeque::iterator SendQueueDequeIter;
    typedef std::map<EmiChannelQualifier, EmiSequenceNumber> SendQueueAcksMap;
    typedef typename SendQueueAcksMap::iterator SendQueueAcksMapIter;
    typedef std::set<EmiChannelQualifier> SendQueueAcksSet;
    typedef EmiConn<SockDelegate, ConnDelegate> EC;
    
    EC& _conn;
    
    EmiPacketSequenceNumber _packetSequenceNumber;
    EmiPacketSequenceNumber _rttResponseSequenceNumber;
    EmiTimeInterval _rttResponseRegisterTime;
    SendQueueDeque _queue;
    size_t _queueSize;
    SendQueueAcksMap _acks;
    // This set is intended to ensure that only one ack is sent per channel per tick
    SendQueueAcksSet _acksSentInThisTick;
    size_t _bufLength;
    uint8_t *_buf;
    bool _enqueueHeartbeat;
    bool _enqueuePacketAck; // This helps to make sure that we only send one packet ACK per tick
    EmiPacketSequenceNumber _enqueuedNak;
    size_t _bytesSentSinceLastTick;
    
private:
    // Private copy constructor and assignment operator
    inline EmiSendQueue(const EmiSendQueue& other);
    inline EmiSendQueue& operator=(const EmiSendQueue& other);
    
    void clearQueue(SendQueueDequeIter begin, SendQueueDequeIter end) {
        SendQueueDequeIter iter(begin);
        while (iter != end) {
            EM *msg = *iter;
            _queueSize -= msg->approximateSize();
            msg->release();
            ++iter;
        }
        
        // Optimize for common special case
        if (_queue.begin() == begin && _queue.end() == end) {
            _queue.clear();
        }
        else {
            _queue.erase(begin, end);
        }
    }
    
    void clearQueue() {
        clearQueue(_queue.begin(), _queue.end());
    }
    
    void sendDatagram(EmiCongestionControl& congestionControl,
                      const uint8_t *buf, size_t bufSize,
                      bool packetHasSequenceNumber) {
        congestionControl.onDataSent(_packetSequenceNumber, bufSize);
        
        if (packetHasSequenceNumber) {
            _packetSequenceNumber = (_packetSequenceNumber+1) & EMI_PACKET_SEQUENCE_NUMBER_MASK;
        }
        
        _conn.sendDatagram(buf, bufSize);
    }
    
    void sendMessageInSeparatePacket(EmiCongestionControl& congestionControl, const EM *msg) {
        const uint8_t *data = Binding::extractData(msg->data);
        size_t dataLen = Binding::extractLength(msg->data);
        
        __block EmiCongestionControl& cc(congestionControl);
        
        EmiMessage<Binding>::template writeControlPacketWithData<128>(msg->flags, data, dataLen, msg->sequenceNumber, ^(uint8_t *packetBuf, size_t size) {
            // Actually send the packet
            sendDatagram(cc, packetBuf, size, /*packetHasSequenceNumber:*/false);
        });
    }
    
    void fillPacketHeaderData(EmiTimeInterval now,
                              EmiCongestionControl& congestionControl,
                              EmiConnTime& connTime,
                              EmiPacketHeader& packetHeader) {
        packetHeader.flags |= EMI_SEQUENCE_NUMBER_PACKET_FLAG;
        packetHeader.sequenceNumber = _packetSequenceNumber;
        
        if (_enqueuePacketAck) {
            _enqueuePacketAck = false;
            
            EmiPacketSequenceNumber ack = congestionControl.ack();
            if (-1 != ack) {
                packetHeader.flags |= EMI_ACK_PACKET_FLAG;
                packetHeader.ack = ack;
            }
        }
        
        if (-1 != _enqueuedNak) {
            packetHeader.flags |= EMI_NAK_PACKET_FLAG;
            packetHeader.nak = _enqueuedNak;
        }
        
        // Note that we only send RTT requests if a packet would be sent anyways.
        // This ensures that RTT data is sent only once per heartbeat if no data
        // is being transmitted.
        if (connTime.rttRequest(now, packetHeader.sequenceNumber)) {
            packetHeader.flags |= EMI_RTT_REQUEST_PACKET_FLAG;
            
            // Since we already have code that makes sure to send RTT requests
            // reasonably often, we piggyback on that to easily decide when to
            // calculate and send data arrival rate info: Whenever we send
            // RTT requests.
            packetHeader.flags |= EMI_ARRIVAL_RATE_PACKET_FLAG;
            packetHeader.arrivalRate = congestionControl.dataArrivalRate();
            
            // Ditto with link capacity; sending it whenever we send RTT
            // requests relieves us from having to cook up some separate logic
            // as to when to send that data.
            packetHeader.flags |= EMI_LINK_CAPACITY_PACKET_FLAG;
            packetHeader.linkCapacity = congestionControl.linkCapacity();
        }
        
        // Fill the packet header with a RTT response if we've been requested to do so
        if (-1 != _rttResponseSequenceNumber) {
            packetHeader.flags |= EMI_RTT_RESPONSE_PACKET_FLAG;
            packetHeader.rttResponse = _rttResponseSequenceNumber;
            
            EmiTimeInterval delay = (now-_rttResponseRegisterTime)*1000;
            if (delay < 0) delay = 0;
            if (delay > EMI_PACKET_HEADER_MAX_RESPONSE_DELAY) delay = EMI_PACKET_HEADER_MAX_RESPONSE_DELAY;
            
            packetHeader.rttResponseDelay = (uint8_t) std::floor(delay);
            
            _rttResponseSequenceNumber = -1;
            _rttResponseRegisterTime = 0;
        }
    }
    
    // Returns true if a packet was sent
    bool flush(EmiCongestionControl& congestionControl,
               EmiConnTime& connTime,
               EmiTimeInterval now) {
        if (_queue.empty() && _acks.empty()) {
            return false;
        }
        
        size_t allowedSize = std::min(_bufLength,
                                      congestionControl.tickAllowance() - _bytesSentSinceLastTick);
        
        EmiPacketHeader packetHeader;
        fillPacketHeaderData(now, congestionControl, connTime, packetHeader);
        size_t packetHeaderLength;
        EmiPacketHeader::write(_buf, _bufLength, packetHeader, &packetHeaderLength);
        
        size_t pos = packetHeaderLength;
        
        /// Send the enqueued messages
        SendQueueAcksMapIter noAck = _acks.end();
        SendQueueDequeIter   iter  = _queue.begin(); // Note: iter is used below this loop
        SendQueueDequeIter   end   = _queue.end();
        while (iter != end) {
            EM *msg = *iter;
            // We need to increment iter here, because we might break
            // out of the loop early, and there is code later on that
            // relies on iter being after the last message that was
            // saved to the packet to be sent.
            ++iter;
            
            SendQueueAcksMapIter curAck;
            if (0 != _acksSentInThisTick.count(msg->channelQualifier)) {
                // Only send an ack for a particular channel once per packet
                curAck = noAck;
            }
            else {
                curAck = _acks.find(msg->channelQualifier);
            }
            
            bool hasAck = curAck != noAck;
            
            size_t msgSize = EM::writeMsg(_buf, /* buf */
                                          _bufLength, /* bufSize */
                                          pos, /* offset */
                                          hasAck, /* hasAck */
                                          hasAck && (*curAck).second, /* ack */
                                          msg->channelQualifier,
                                          msg->sequenceNumber,
                                          Binding::extractData(msg->data),
                                          Binding::extractLength(msg->data),
                                          msg->flags);
            
            if (pos+msgSize > allowedSize) {
                // The message got too big.
                break;
            }
            
            // Do the actual side effects to save the packet data.
            //
            // We need to do this after the potential break above.
            //
            // Up until now, this loop iteration did not perform
            // any side effects: saving the message to the buffer
            // does not count since we did not actually increment
            // pos. Code below will assume that that data is undefined
            // garbage unless we increment pos.
            pos += msgSize;
            _acksSentInThisTick.insert(msg->channelQualifier);
            _acks.erase(msg->channelQualifier);
        }
        
        /// Send ACK messages without data for the acks that are
        /// enqueued but was not sent along with actual data.
        SendQueueAcksMapIter ackIter = _acks.begin();
        SendQueueAcksMapIter ackEnd = _acks.end();
        while (ackIter != ackEnd) {
            EmiChannelQualifier cq = (*ackIter).first;
            
            if (0 == _acksSentInThisTick.count(cq)) {
                EmiSequenceNumber sn = (*ackIter).second;
                
                size_t msgSize = EM::writeMsg(_buf, /* buf */
                                              _bufLength, /* bufSize */
                                              pos, /* offset */
                                              true, /* hasAck */
                                              sn, /* ack */
                                              cq, /* channelQualifier */
                                              0, /* sequenceNumber */
                                              NULL, /* data */
                                              0, /* dataLength */
                                              0 /* flags */);
                
                if (pos+msgSize > allowedSize) {
                    // The message got too big.
                    break;
                }
                
                // Do the actual side effects. Like the previous loop,
                // we need to do all lasting side effects after the
                // potential break above.
                pos += msgSize;
                _acksSentInThisTick.insert(cq);
                _acks.erase(cq);
            }
            
            ++ackIter;
        }
        
        if (packetHeaderLength != pos) {
            ASSERT(pos <= _bufLength);
            
            sendDatagram(congestionControl, _buf, pos, /*packetHasSequenceNumber:*/true);
            _bytesSentSinceLastTick += pos;
            
            clearQueue(_queue.begin(), iter);
            
            // Return true to signify that a packet was sent
            return true;
        }
        else {
            // Return false to signify that no packet was sent
            return false;
        }
    }
    
public:
    
    EmiSendQueue(EC& conn) :
    _conn(conn),
    _packetSequenceNumber(arc4random() & EMI_PACKET_SEQUENCE_NUMBER_MASK),
    _rttResponseSequenceNumber(-1),
    _rttResponseRegisterTime(0),
    _enqueueHeartbeat(false),
    _enqueuePacketAck(false),
    _enqueuedNak(-1),
    _bytesSentSinceLastTick(0),
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
    
    void enqueueNak(EmiPacketSequenceNumber nak) {
        _enqueuedNak = nak;
    }
    
    // Returns the number of bytes sent
    size_t sendHeartbeat(EmiCongestionControl& congestionControl,
                         EmiConnTime& connTime,
                         EmiTimeInterval now) {
        EmiPacketHeader ph;
        fillPacketHeaderData(now, congestionControl, connTime, ph);
        
        uint8_t buf[32];
        size_t packetLength;
        EmiPacketHeader::write(buf, sizeof(buf), ph, &packetLength);
        
        if (_conn.isOpen()) {
            sendDatagram(congestionControl, buf, packetLength, /*packetHasSequenceNumber:*/true);
        }
        
        return packetLength;
    }
    
    // Returns true if something has been sent since the last tick
    bool tick(EmiCongestionControl& congestionControl,
              EmiConnTime& connTime,
              EmiTimeInterval now) {
        _enqueuePacketAck = true;
        
        _acksSentInThisTick.clear();
        
        // Send packets until we can't send any more packets,
        // either because of congestion control, or because there
        // is nothing more to send.
        while (flush(congestionControl, connTime, now));
        
        if (0 == _bytesSentSinceLastTick && _enqueueHeartbeat) {
            // Send heartbeat
            size_t heartbeatSize = sendHeartbeat(congestionControl, connTime, now);
            _bytesSentSinceLastTick += heartbeatSize;
            _enqueueHeartbeat = false;
        }
        
        bool somethingHasBeenSentInThisTick = (0 != _bytesSentSinceLastTick);
        _bytesSentSinceLastTick = 0;
        
        return somethingHasBeenSentInThisTick;
    }
    
    // Returns true if at least 1 ack is now enqueued
    bool enqueueAck(EmiChannelQualifier channelQualifier, EmiSequenceNumber sequenceNumber) {
        SendQueueAcksMapIter ackCur = _acks.find(channelQualifier);
        SendQueueAcksMapIter ackEnd = _acks.end();
        
        if (ackCur == ackEnd) {
            _acks[channelQualifier] = sequenceNumber;
        }
        else {
            _acks[channelQualifier] = EmiNetUtil::cyclicMax16((*ackCur).second, sequenceNumber);
        }
        
        return !_acks.empty();
    }
    
    inline EmiPacketSequenceNumber lastSentSequenceNumber() const {
        return _packetSequenceNumber;
    }
    
    void enqueueRttResponse(EmiPacketSequenceNumber sequenceNumber, EmiTimeInterval now) {
        _rttResponseSequenceNumber = sequenceNumber;
        _rttResponseRegisterTime = now;
    }
    
    bool enqueueMessage(EM *msg,
                        EmiCongestionControl& congestionControl,
                        EmiConnTime& connTime,
                        EmiTimeInterval now,
                        Error& err) {
        if (msg->flags & (EMI_PRX_FLAG | EMI_RST_FLAG | EMI_SYN_FLAG)) {
            // This is a control message, one that cannot be bundled with
            // other messages. We might just as well send it right away.
            //
            // Control messages are not congestion controlled.
            sendMessageInSeparatePacket(congestionControl, msg);
        }
        else {
            // Only EMI_PRIORITY_HIGH messages are implemented
            ASSERT(EMI_PRIORITY_HIGH == msg->priority);
            
            size_t msgSize = msg->approximateSize();
            
            // _bufLength is the MTU of the EmiSocket.
            // mss is short for maximum segment size.
            size_t mss = _bufLength - EMI_PACKET_HEADER_MAX_LENGTH - EMI_UDP_HEADER_SIZE;
            if (_queueSize + msgSize >= mss) {
                flush(congestionControl, connTime, now);
            }
            
            msg->retain();
            _queue.push_back(msg);
            _queueSize += msgSize;
        }
        
        return true;
    }
};

#endif
