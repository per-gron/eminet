//
//  EmiMessage.h
//  roshambo
//
//  Created by Per Eckerdal on 2012-04-09.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#ifndef emilir_EmiMessage_h
#define emilir_EmiMessage_h

#include "EmiTypes.h"
#include "EmiConnTime.h"
#include "EmiNetUtil.h"
#include "EmiPacketHeader.h"

#include <cmath>
#include <algorithm>

// A message, as it is represented in the sender side of the pipeline
template<class Binding>
class EmiMessage {
private:
    typedef typename Binding::PersistentData PersistentData;
    
    // Private copy constructor and assignment operator
    inline EmiMessage(const EmiMessage& other);
    inline EmiMessage& operator=(const EmiMessage& other);
    
    size_t _refCount;
    
    inline void commonInit() {
        _refCount = 1;
        registrationTime = 0;
        channelQualifier = EMI_CHANNEL_QUALIFIER_DEFAULT;
        sequenceNumber = 0;
        flags = 0;
        priority = EMI_PRIORITY_DEFAULT;
    }
    
    // The caller is responsible for releasing the returned object
    static EmiMessage *makeSynAndOrRstMessage(EmiMessageFlags flags, EmiSequenceNumber sequenceNumber,
                                              const uint8_t *data, size_t dataLen) {
        EmiMessage *msg;
        if (data) {
            msg = new EmiMessage(Binding::makePersistentData(data, dataLen));
        }
        else {
            msg = new EmiMessage;
        }
        msg->priority = EMI_PRIORITY_HIGH;
        msg->channelQualifier = -1; // Special SYN/RST message channel. SenderBuffer requires this to be an integer
        msg->sequenceNumber = sequenceNumber;
        msg->flags = flags;
        return msg;
    }
    
public:
    
    // The caller is responsible for releasing the returned object
    static EmiMessage *makeSynMessage(EmiSequenceNumber sequenceNumber, const uint8_t *data, size_t dataLen) {
        return makeSynAndOrRstMessage(EMI_SYN_FLAG, sequenceNumber, data, dataLen);
    }
    
    // The caller is responsible for releasing the returned object
    static EmiMessage *makeSynRstMessage(EmiSequenceNumber sequenceNumber) {
        return makeSynAndOrRstMessage(EMI_SYN_FLAG | EMI_RST_FLAG, sequenceNumber, /*data:*/NULL, /*dataLen:*/0);
    }
    
    // The caller is responsible for releasing the returned object
    static EmiMessage *makeRstMessage(EmiSequenceNumber sequenceNumber) {
        return makeSynAndOrRstMessage(EMI_RST_FLAG, sequenceNumber, /*data:*/NULL, /*dataLen:*/0);
    }
    
    // The caller is responsible for releasing the returned object
    static EmiMessage *makePrxAckMessage(EmiSequenceNumber sequenceNumber, const sockaddr_storage& inboundAddr) {
        const size_t ipLen = EmiNetUtil::ipLength(inboundAddr);
        static const size_t portLen = sizeof(uint16_t);
        const size_t endpointLen = ipLen+portLen;
        
        uint8_t buf[96];
        ASSERT(sizeof(buf) >= endpointLen);
        
        // The IP address and port number are in network byte order
        
        /// Save the endpoint in buf
        EmiNetUtil::extractIp(inboundAddr, buf, sizeof(buf));
        uint16_t port = EmiNetUtil::addrPortN(inboundAddr);
        memcpy(buf+ipLen, &port, portLen);
        
        return makeSynAndOrRstMessage(EMI_PRX_FLAG | EMI_ACK_FLAG, sequenceNumber, buf, endpointLen);
    }
    
    // The caller is responsible for releasing the returned object
    static EmiMessage *makeDataMessage(EmiChannelQualifier cq,
                                       EmiSequenceNumber sequenceNumber,
                                       const PersistentData& data,
                                       EmiPriority priority) {
        EmiMessage *msg = new EmiMessage(data);
        msg->channelQualifier = cq;
        msg->sequenceNumber = sequenceNumber;
        msg->priority = priority;
        
        return msg;
    }
    
    explicit EmiMessage(PersistentData data_) : data(data_) {
        commonInit();
    }
    
    EmiMessage() : data() {
        commonInit();
    }
    
    ~EmiMessage() {
        Binding::releasePersistentData(data);
    }
    
    inline void retain() {
        _refCount++;
    }
    
    inline void release() {
        _refCount--;
        if (0 == _refCount) delete this;
    }
    
    // Returns an upper bound of the size of this message as encoded
    // on the wire. Note that EmiSendQueue relies on this method to
    // always return the same value given the same message.
    size_t approximateSize() const {
        // + 2 for the possibility of adding ACK data to the message
        // + 3 for the sequence number and split id
        return EMI_MESSAGE_HEADER_MIN_LENGTH + Binding::extractLength(data) + 2 + 3;
    }
    
    // THIS FIELD IS INTENDED TO BE USED ONLY BY EmiSenderBuffer!
    // Modifying this field outside of that class will break invariants
    // and can result in behaviour ranging from mild inefficiencies and
    // stalled message streams to hard crashes.
    EmiTimeInterval registrationTime;
    // This is int32_t and not EmiChannelQualifier because it has to be capable of
    // holding -1, the special SYN/RST message channel as used by EmiSenderBuffer
    int32_t channelQualifier;
    EmiSequenceNumber sequenceNumber;
    EmiMessageFlags flags;
    EmiPriority priority;
    const PersistentData data;
    
    // Returns 0 if buffer was not big enough to accomodate the message
    static size_t writeMsg(uint8_t *buf,
                           size_t bufSize,
                           size_t offset,
                           bool hasAck,
                           EmiSequenceNumber ack,
                           int32_t channelQualifier,
                           EmiSequenceNumber sequenceNumber,
                           const uint8_t *data,
                           size_t dataLength,
                           EmiMessageFlags flags) {
        // TODO The way this code is written makes the method rather fragile.
        // It's easy to make small mistakes that lead to potential buffer
        // overflow bugs. It should probably be rewritten in a clearer way.
        
        size_t pos = offset;
        
        flags |= (hasAck ? EMI_ACK_FLAG : 0); // SYN/RST/ACK flags
        
        // Quick and dirty way to validate parameters
        ASSERT(0 != flags || 0 != dataLength);
        
        size_t sequenceNumberFieldSize =
            ((0 != dataLength ||
              ((flags & EMI_SYN_FLAG) && !(flags & EMI_PRX_FLAG))) ? EMI_HEADER_SEQUENCE_NUMBER_LENGTH : 0);
        size_t ackSize = (hasAck ? EMI_HEADER_SEQUENCE_NUMBER_LENGTH : 0);
        
        if (bufSize-pos <= (EMI_MESSAGE_HEADER_MIN_LENGTH +
                            sequenceNumberFieldSize +
                            ackSize +
                            dataLength)) {
            // Buffer not big enough
            return 0;
        }
        
        *((uint8_t*)  (buf+pos)) = flags; pos += 1;
        *((uint8_t*)  (buf+pos)) = std::max(0, channelQualifier); pos += 1; // channelQualifier == -1 means SYN/RST message
        *((uint16_t*) (buf+pos)) = htons(dataLength); pos += 2;
        if (sequenceNumberFieldSize) {
            EmiNetUtil::write24(buf+pos, sequenceNumber); pos += sequenceNumberFieldSize;
        }
        if (ackSize) {
            EmiNetUtil::write24(buf+pos, ack); pos += ackSize;
        }
        if (dataLength) {
            memcpy(buf+pos, data, dataLength); pos += dataLength;
        }
        
        // If this assert fails, we have a buffer overflow.
        // I wish I had put this here in the first place :/
        ASSERT(pos < bufSize);
        
        return pos-offset;
    }
    
    // Returns the size of the packet, or 0 if the buffer was not large enough
    static size_t writeControlPacketWithData(EmiMessageFlags flags,
                                             uint8_t *buf, size_t bufSize,
                                             const uint8_t *data, size_t dataLength,
                                             EmiSequenceNumber sequenceNumber) {
        // Zero out the packet header
        size_t tlen;
        if (!EmiPacketHeader::writeEmpty(buf, bufSize, &tlen)) {
            return 0;
        }
        
        // Propagate the actual packet data
        size_t plen;
        plen = writeMsg(buf, /* buf */
                        bufSize, /* bufSize */
                        tlen, /* offset */
                        false, /* hasAck */
                        0, /* ack */
                        -1, /* channelQualifier */
                        sequenceNumber,
                        data,
                        dataLength,
                        flags);
        
        if (0 == plen) {
            return 0;
        }
        
        return tlen+plen;
    }
    
    // Returns the size of the packet, or 0 if the buffer was not large enough
    static size_t writeControlPacketWithData(EmiMessageFlags flags,
                                             uint8_t *buf, size_t bufSize,
                                             const uint8_t *data, size_t dataLength) {
        return writeControlPacketWithData(flags, buf, bufSize, data, dataLength, 0);
    }
    
    // Returns the size of the packet, or 0 if the buffer was not large enough
    static size_t writeControlPacket(EmiMessageFlags flags,
                                     uint8_t *buf, size_t bufSize,
                                     EmiSequenceNumber sequenceNumber) {
        return writeControlPacketWithData(flags, buf, bufSize, NULL, 0, sequenceNumber);
    }
    
    // Returns the size of the packet, or 0 if the buffer was not large enough
    static size_t writeControlPacket(EmiMessageFlags flags,
                                     uint8_t *buf, size_t bufSize) {
        return writeControlPacketWithData(flags, buf, bufSize, NULL, 0);
    }
};

#endif
