//
//  EmiPacketHeader.cc
//  rock
//
//  Created by Per Eckerdal on 2012-05-10.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#include "EmiPacketHeader.h"

#include "EmiNetUtil.h"

#include <cstring>
#include <algorithm>

inline static void extractFlagsAndSize(EmiPacketFlags flags,
                                       EmiPacketExtraFlags extraFlags,
                                       bool *hasSequenceNumber,
                                       bool *hasAck,
                                       bool *hasNak,
                                       bool *hasLinkCapacity,
                                       bool *hasArrivalRate, 
                                       bool *hasRttRequest,
                                       bool *hasRttResponse,
                                       size_t *fillerSizePtr, // Can be NULL
                                       size_t *expectedSize) {
    size_t fillerSize = 0;
    
    *hasSequenceNumber = !!(flags & EMI_SEQUENCE_NUMBER_PACKET_FLAG);
    *hasAck            = !!(flags & EMI_ACK_PACKET_FLAG);
    *hasNak            = !!(flags & EMI_NAK_PACKET_FLAG);
    *hasLinkCapacity   = !!(flags & EMI_LINK_CAPACITY_PACKET_FLAG);
    *hasArrivalRate    = !!(flags & EMI_ARRIVAL_RATE_PACKET_FLAG);
    *hasRttRequest     = !!(flags & EMI_RTT_REQUEST_PACKET_FLAG);
    *hasRttResponse    = !!(flags & EMI_RTT_RESPONSE_PACKET_FLAG);
    bool hasExtraFlags = !!(flags & EMI_EXTRA_FLAGS_PACKET_FLAG);
    
    // 1 for the flags byte
    *expectedSize = sizeof(EmiPacketFlags);
    
    if (hasExtraFlags) {
        *expectedSize += sizeof(EmiPacketExtraFlags);
        
        if (flags & EMI_1_BYTE_FILLER_EXTRA_PACKET_FLAG) {
            fillerSize = 1;
        }
        else if (flags & EMI_2_BYTE_FILLER_EXTRA_PACKET_FLAG) {
            fillerSize = 2;
        }
        else {
            fillerSize = 0;
        }
        
        *expectedSize += fillerSize;
    }
    
    if (fillerSizePtr) {
        *fillerSizePtr = fillerSize;
    }
    
    *expectedSize += (*hasSequenceNumber ? EMI_PACKET_SEQUENCE_NUMBER_LENGTH : 0);
    *expectedSize += (*hasAck            ? EMI_PACKET_SEQUENCE_NUMBER_LENGTH : 0);
    *expectedSize += (*hasNak            ? EMI_PACKET_SEQUENCE_NUMBER_LENGTH : 0);
    *expectedSize += (*hasLinkCapacity   ? sizeof(float) : 0);
    *expectedSize += (*hasArrivalRate    ? sizeof(float) : 0);
    *expectedSize += (*hasRttResponse    ? EMI_PACKET_SEQUENCE_NUMBER_LENGTH+sizeof(uint8_t) : 0);
}

EmiPacketHeader::EmiPacketHeader() :
flags(0),
sequenceNumber(0),
ack(0),
nak(0),
linkCapacity(0),
arrivalRate(0),
rttResponse(0),
rttResponseDelay(0) {}

EmiPacketHeader::~EmiPacketHeader() {}

bool EmiPacketHeader::parse(const uint8_t *buf, size_t bufSize, EmiPacketHeader *header, size_t *headerLength) {
    if (0 >= bufSize) {
        return false;
    }
    
    EmiPacketFlags flags = buf[0];
    
    EmiPacketExtraFlags extraFlags = (EmiPacketExtraFlags) 0;
    if (bufSize >= 1) {
        extraFlags = (EmiPacketExtraFlags) buf[1];
    }
    
    bool hasSequenceNumber, hasAck, hasNak, hasLinkCapacity;
    bool hasArrivalRate, hasRttRequest, hasRttResponse;
    size_t expectedSize, fillerSize;
    extractFlagsAndSize(flags,
                        extraFlags,
                        &hasSequenceNumber,
                        &hasAck,
                        &hasNak,
                        &hasLinkCapacity,
                        &hasArrivalRate, 
                        &hasRttRequest,
                        &hasRttResponse,
                        &fillerSize,
                        &expectedSize);
    
    if (2 == fillerSize) {
        // A 2 byte filler size means that the packet contains a 16 bit
        // unsigned integer which specifies padding
        if (4 > bufSize) {
            return false;
        }
        uint16_t twoByteFillerSize = ntohs(*((uint16_t *)(buf+2)));
        fillerSize += twoByteFillerSize;
        expectedSize += twoByteFillerSize;
    }
    
    if (bufSize < expectedSize) {
        return false;
    }
    
    header->flags = flags;
    header->sequenceNumber = 0;
    header->ack = 0;
    header->nak = 0;
    header->linkCapacity = 0.0f;
    header->arrivalRate = 0.0f;
    header->rttResponse = 0;
    header->rttResponseDelay = 0;
    
    const uint8_t *bufCur = buf+sizeof(header->flags);
    
    if (flags & EMI_EXTRA_FLAGS_PACKET_FLAG) {
        bufCur += sizeof(EmiPacketExtraFlags);
        bufCur += fillerSize;
    }
    
    if (hasSequenceNumber) {
        header->sequenceNumber = EmiNetUtil::read24(bufCur);
        bufCur += EMI_PACKET_SEQUENCE_NUMBER_LENGTH;
    }
    
    if (hasAck) {
        header->ack = EmiNetUtil::read24(bufCur);
        bufCur += EMI_PACKET_SEQUENCE_NUMBER_LENGTH;
    }
    
    if (hasNak) {
        header->nak = EmiNetUtil::read24(bufCur);
        bufCur += EMI_PACKET_SEQUENCE_NUMBER_LENGTH;
    }
    
    if (hasLinkCapacity) {
        uint32_t linkCapacityInt = ntohl(*reinterpret_cast<const uint32_t *>(bufCur));
        header->linkCapacity = *reinterpret_cast<float *>(&linkCapacityInt);
        bufCur += sizeof(header->linkCapacity);
    }
    
    if (hasArrivalRate) {
        uint32_t arrivalRateInt = ntohl(*reinterpret_cast<const uint32_t *>(bufCur));
        header->arrivalRate = *reinterpret_cast<float *>(&arrivalRateInt);
        bufCur += sizeof(header->arrivalRate);
    }
    
    if (hasRttResponse) {
        header->rttResponse = EmiNetUtil::read24(bufCur);
        bufCur += EMI_PACKET_SEQUENCE_NUMBER_LENGTH;
        
        header->rttResponseDelay = *bufCur;
        bufCur += sizeof(header->rttResponseDelay);
    }
    
    if (headerLength) {
        *headerLength = expectedSize;
    }
    
    return true;
}

bool EmiPacketHeader::write(uint8_t *buf, size_t bufSize, const EmiPacketHeader& header, size_t *headerLength) {
    if (0 >= bufSize) {
        return false;
    }
    
    bool hasSequenceNumber, hasAck, hasNak, hasLinkCapacity;
    bool hasArrivalRate, hasRttRequest, hasRttResponse;
    size_t expectedSize;
    extractFlagsAndSize(header.flags,
                        (EmiPacketExtraFlags)0,
                        &hasSequenceNumber,
                        &hasAck,
                        &hasNak,
                        &hasLinkCapacity,
                        &hasArrivalRate, 
                        &hasRttRequest,
                        &hasRttResponse,
                        /*fillerSize:*/NULL,
                        &expectedSize);
    
    if (bufSize < expectedSize) {
        return false;
    }
    
    memset(buf, 0, expectedSize);
    buf[0] = header.flags;
    
    uint8_t *bufCur = buf+sizeof(EmiPacketFlags);
    
    if (hasSequenceNumber) {
        EmiNetUtil::write24(bufCur, header.sequenceNumber);
        bufCur += EMI_PACKET_SEQUENCE_NUMBER_LENGTH;
    }
    
    if (hasAck) {
        EmiNetUtil::write24(bufCur, header.ack);
        bufCur += EMI_PACKET_SEQUENCE_NUMBER_LENGTH;
    }
    
    if (hasNak) {
        EmiNetUtil::write24(bufCur, header.nak);
        bufCur += EMI_PACKET_SEQUENCE_NUMBER_LENGTH;
    }
    
    if (hasLinkCapacity) {
        *((int32_t *)bufCur) = htonl(*reinterpret_cast<const uint32_t *>(&header.linkCapacity));
        bufCur += sizeof(header.linkCapacity);
    }
    
    if (hasArrivalRate) {
        *((int32_t *)bufCur) = htonl(*reinterpret_cast<const uint32_t *>(&header.arrivalRate));
        bufCur += sizeof(header.arrivalRate);
    }
    
    if (hasRttResponse) {
        EmiNetUtil::write24(bufCur, header.rttResponse);
        bufCur += EMI_PACKET_SEQUENCE_NUMBER_LENGTH;
        
        *bufCur = header.rttResponseDelay;
        bufCur += sizeof(header.rttResponseDelay);
    }
    
    if (headerLength) {
        *headerLength = expectedSize;
    }
    
    return true;
}

bool EmiPacketHeader::writeEmpty(uint8_t *buf, size_t bufSize, size_t *headerLength) {
    if (0 >= bufSize) {
        return false;
    }
    
    *buf = 0;
    *headerLength = 1;
    
    return true;
}

void EmiPacketHeader::addFillerBytes(uint8_t *buf, size_t packetSize, uint16_t fillerSize) {
    ASSERT(1 <= packetSize);
    
    if (0 == fillerSize) {
        // We don't need to do anything
        return;
    }
    
    // Move the packet data
    std::copy_backward(buf+1, buf+packetSize, buf+packetSize+fillerSize);
    
    // Make sure we have the extra flags byte
    if (!(buf[0] & EMI_EXTRA_FLAGS_PACKET_FLAG)) {
        buf[0] |= EMI_EXTRA_FLAGS_PACKET_FLAG;
        buf[1] = 0;
        
        // Decrement the filler size here because by adding
        // the extra flags byte we have already increased
        // the size of the packet by one.
        fillerSize -= sizeof(EmiPacketExtraFlags);
    }
    
    if (0 == fillerSize) {
        // We're done. This happens when fillerSize was 1 and
        // the packet did not already contain the extra flags
        // byte
    }
    else if (1 == fillerSize) {
        buf[1] |= EMI_1_BYTE_FILLER_EXTRA_PACKET_FLAG;
        buf[2] = 0;
    }
    else {
        buf[1] |= EMI_2_BYTE_FILLER_EXTRA_PACKET_FLAG;
        
        *((uint16_t *)(buf+2)) = htons(fillerSize - 2);
        
        memset(buf+4, 0, fillerSize-2);
    }
}
