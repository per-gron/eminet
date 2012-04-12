//
//  EmiMessageHeader.h
//  roshambo
//
//  Created by Per Eckerdal on 2012-02-16.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#ifndef emilir_EmiMessageHeader_h
#define emilir_EmiMessageHeader_h

#include "EmiTypes.h"

#include <stdint.h>

struct EmiMessageHeader;

typedef bool (^EmiParseMessageBlock)(EmiMessageHeader *header, size_t dataOffset);

// A message header in a computation friendly format (the actual wire
// format is more condensed)
struct EmiMessageHeader {
    EmiFlags flags;
    EmiChannelQualifier channelQualifier;
    // This is int32_t and not EmiChannelQualifier because it has to be capable of
    // holding -1, which means that the header had no sequence number
    int32_t sequenceNumber;
    // This is int32_t and not EmiSplitId because it has to be capable of
    // holding -1, which means that the header had no split id
    int32_t splitId;
    size_t headerLength;
    // length is 0 if the message has no content
    size_t length;
    // This is int32_t and not EmiSequenceNumber because it has to be capable of
    // holding -1, which means that the header had no ack
    int32_t ack;
    
    // Returns true if the parse was successful
    static bool parseMessageHeader(const uint8_t *buf, size_t bufSize, EmiMessageHeader *header) {
        if (bufSize < EMI_HEADER_LENGTH) return false;
        
        uint8_t connByte = buf[0];
        uint16_t length = ntohs(*((uint16_t *)(buf+2)));
        
        bool rstFlag = connByte & EMI_RST_FLAG;
        bool ackFlag = connByte & EMI_ACK_FLAG;
        bool synFlag = connByte & EMI_SYN_FLAG;
        
        // If the message has RST, SYN and ACK flags, it's a close
        // connection ack message, not a normal message with ack
        bool messageHasAckData = ackFlag && !(rstFlag && synFlag);
        
        NSUInteger lengthOffset = length ? 3 : (synFlag ? 2 : 0);
        NSUInteger headerLength = EMI_HEADER_LENGTH + lengthOffset + (messageHasAckData ? 2 : 0);
        
        if (headerLength > bufSize) return false;
        
        header->flags = connByte;
        header->channelQualifier = buf[1];
        header->sequenceNumber = (length || synFlag) ? ntohs(*((uint16_t *)(buf+4))) : -1;
        header->splitId = length ? buf[6] : -1;
        header->headerLength = headerLength;
        header->length = length;
        header->ack = messageHasAckData ? ntohs(*((uint16_t *)(buf+4+lengthOffset))) : -1;
        
        return true;
    }
    
    // Returns true if the entire parse was successful
    static bool parseMessages(const uint8_t *buf, size_t bufSize, EmiParseMessageBlock block) {
        size_t offset = 0;
        
        while (offset + EMI_HEADER_LENGTH <= bufSize) {
            EmiMessageHeader header;
            if (!EmiMessageHeader::parseMessageHeader(buf+offset, 
                                                      bufSize-offset,
                                                      &header)) {
                return false;
            }
            
            size_t dataOffset = offset+header.headerLength;
            if (dataOffset + header.length > bufSize) {
                return false;
            }
            
            offset += header.headerLength+header.length;
            if (header.flags & EMI_SACK_FLAG) return false;
            
            if (!block(&header, dataOffset)) {
                return false;
            }
        }
        
        return offset == bufSize;
    }
};

#endif
