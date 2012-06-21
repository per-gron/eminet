//
//  EmiMessageHeader.cc
//  rock
//
//  Created by Per Eckerdal on 2012-05-10.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#include "EmiMessageHeader.h"

#include "EmiNetUtil.h"

bool EmiMessageHeader::parse(const uint8_t *buf, size_t bufSize, EmiMessageHeader& header) {
    if (bufSize < EMI_MESSAGE_HEADER_MIN_LENGTH) return false;
    
    uint8_t connByte = buf[0];
    uint16_t length = ntohs(*((uint16_t *)(buf+2)));
    
    bool prxFlag = connByte & EMI_PRX_FLAG;
    bool rstFlag = connByte & EMI_RST_FLAG;
    bool ackFlag = connByte & EMI_ACK_FLAG;
    bool synFlag = connByte & EMI_SYN_FLAG;
    
    // If the message has RST, SYN and ACK flags, it's a close
    // connection ack message, not a normal message with ack
    bool messageHasAckData = ackFlag && !(rstFlag && synFlag) && !prxFlag;
    
    bool messageHasSequenceNumber = (length || (synFlag && !prxFlag));
    
    size_t lengthOffset = (length || synFlag) ? EMI_HEADER_SEQUENCE_NUMBER_LENGTH : 0;
    size_t headerLength = EMI_MESSAGE_HEADER_MIN_LENGTH + lengthOffset + (messageHasAckData ? EMI_HEADER_SEQUENCE_NUMBER_LENGTH : 0);
    
    if (headerLength > bufSize) return false;
    
    header.flags = connByte;
    header.channelQualifier = buf[1];
    if (messageHasSequenceNumber) {
        header.sequenceNumber = EmiNetUtil::read24(buf+EMI_MESSAGE_HEADER_MIN_LENGTH);
    }
    else {
        header.sequenceNumber = -1;
    }
    header.headerLength = headerLength;
    header.length = length;
    if (messageHasAckData) {
        header.ack = EmiNetUtil::read24(buf+EMI_MESSAGE_HEADER_MIN_LENGTH+lengthOffset);
    }
    else {
        header.ack = -1;
    }
    
    return true;
}

bool EmiMessageHeader::parseNextMessage(const uint8_t *buf, size_t bufSize,
                                        size_t *offset,
                                        size_t *dataOffset,
                                        EmiMessageHeader *header) {
    if (*offset + EMI_MESSAGE_HEADER_MIN_LENGTH <= bufSize) {
        if (!EmiMessageHeader::parse(buf+*offset, 
                                     bufSize-*offset,
                                     *header)) {
            return false;
        }
        
        *dataOffset = *offset+header->headerLength;
        if (*dataOffset + header->length > bufSize) {
            return false;
        }
        
        *offset += header->headerLength+header->length;
        if (header->flags & EMI_SACK_FLAG) return false;
        
        return true;
    }
    else {
        return *offset == bufSize;
    }
}
