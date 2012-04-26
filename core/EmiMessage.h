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

#include <cmath>
#include <algorithm>

// A message, as it is represented in the sender side of the pipeline
template<class Binding>
class EmiMessage {
private:
    typedef void (^SendSynRstAckPacketCallback)(uint8_t *buf, size_t size);
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
    static EmiMessage *makeSynAndOrRstMessage(EmiFlags flags, EmiSequenceNumber sequenceNumber) {
        EmiMessage *msg = new EmiMessage;
        msg->priority = EMI_PRIORITY_HIGH;
        msg->channelQualifier = -1; // Special SYN/RST message channel. SenderBuffer requires this to be an integer
        msg->sequenceNumber = sequenceNumber;
        msg->flags = flags;
        return msg;
    }
    
public:
    
    // The caller is responsible for releasing the returned object
    static EmiMessage *makeSynMessage(EmiSequenceNumber sequenceNumber) {
        return makeSynAndOrRstMessage(EMI_SYN_FLAG, sequenceNumber);
    }
    
    // The caller is responsible for releasing the returned object
    static EmiMessage *makeSynRstMessage(EmiSequenceNumber sequenceNumber) {
        return makeSynAndOrRstMessage(EMI_SYN_FLAG | EMI_RST_FLAG, sequenceNumber);
    }
    
    // The caller is responsible for releasing the returned object
    static EmiMessage *makeRstMessage(EmiSequenceNumber sequenceNumber) {
        return makeSynAndOrRstMessage(EMI_RST_FLAG, sequenceNumber);
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
    
    size_t approximateSize() const {
        // + 2 for the possibility of adding ACK data to the message
        // + 3 for the sequence number and split id
        return EMI_HEADER_LENGTH + Binding::extractLength(data) + 2 + 3;
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
    EmiFlags flags;
    EmiPriority priority;
    const PersistentData data;
    
    static size_t writeMsg(uint8_t *buf,
                           size_t bufSize,
                           size_t offset,
                           bool hasAck,
                           EmiSequenceNumber ack,
                           int32_t channelQualifier,
                           EmiSequenceNumber sequenceNumber,
                           const uint8_t *data,
                           size_t dataLength,
                           EmiFlags flags) {
        size_t pos = offset;
        
        flags |= (hasAck ? EMI_ACK_FLAG : 0); // SYN/RST/ACK flags
        
        if (0 == flags && 0 == dataLength) {
            return 0;
        }
        
        *((uint8_t*)  (buf+pos)) = flags; pos += 1;
        *((uint8_t*)  (buf+pos)) = std::max(0, channelQualifier); pos += 1; // Channel qualifier. cq == -1 means SYN/RST message
        *((uint16_t*) (buf+pos)) = htons(dataLength); pos += 2;
        if (0 != dataLength || flags & EMI_SYN_FLAG) {
            *((uint16_t*) (buf+pos)) = htons(sequenceNumber); pos += 2;
        }
        if (0 != dataLength) {
            *((uint8_t*) (buf+pos)) = 0; pos += 1; // Split id
        }
        if (hasAck) {
            *((uint16_t*) (buf+pos)) = htons(ack); pos += 2;
        }
        if (dataLength) {
            if (pos+dataLength > bufSize) {
                // The data doesn't fit in the buffer
                return 0;
            }
            memcpy(buf+pos, data, dataLength); pos += dataLength;
        }
        
        return pos-offset;
    }
    
    template<int BUF_SIZE>
    static void writeControlPacketWithData(EmiFlags flags, const uint8_t *data, size_t dataLength,
                                           SendSynRstAckPacketCallback callback) {
        uint8_t buf[BUF_SIZE];
        
        // Zero out the timestamp
        size_t tlen = EMI_TIMESTAMP_LENGTH;
        memset(buf, 0, tlen);
        
        // Propagate the actual packet data
        size_t plen;
        plen = writeMsg(buf, /* buf */
                        BUF_SIZE, /* bufSize */
                        tlen, /* offset */
                        false, /* hasAck */
                        0, /* ack */
                        -1, /* channelQualifier */
                        0, /* sequenceNumber */
                        data,
                        dataLength,
                        flags);
        
        ASSERT(plen);
        
        callback(buf, tlen+plen);
    }
    
    static void writeControlPacket(EmiFlags flags, SendSynRstAckPacketCallback callback) {
        // BUF_SIZE=96 ought to be plenty
        writeControlPacketWithData<96>(flags, NULL, 0, callback);
    }
    
    static void fillTimestamps(EmiConnTime& connTime, void *data, EmiTimeInterval now) {
        uint16_t *buf = (uint16_t *)data;
        
        buf[0] = htons(floor(connTime.getCurrentTime(now)*1000));
        if (connTime.hasReceivedTime()) {
            buf[1] = htons(connTime.getLargestReceivedTime());
            buf[2] = htons(floor((now - connTime.gotLargestReceivedTimeAt())*1000));
        }
        else {
            buf[1] = htons(0);
            buf[2] = htons(0);
        }
    }
};

#endif
