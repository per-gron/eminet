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

inline static void extractFlagsAndSize(EmiPacketFlags flags,
                                       bool *hasSequenceNumber,
                                       bool *hasNak,
                                       bool *hasLinkCapacity,
                                       bool *hasArrivalRate, 
                                       bool *hasRttRequest,
                                       bool *hasRttResponse,
                                       size_t *expectedSize) {
    *hasSequenceNumber = !!(flags & EMI_SEQUENCE_NUMBER_PACKET_FLAG);
    *hasNak            = !!(flags & EMI_NAK_PACKET_FLAG);
    *hasLinkCapacity   = !!(flags & EMI_LINK_CAPACITY_PACKET_FLAG);
    *hasArrivalRate    = !!(flags & EMI_ARRIVAL_RATE_PACKET_FLAG);
    *hasRttRequest     = !!(flags & EMI_RTT_REQUEST_PACKET_FLAG);
    *hasRttResponse    = !!(flags & EMI_RTT_RESPONSE_PACKET_FLAG);
    
    // 1 for the flags byte
    *expectedSize = sizeof(EmiPacketFlags);
    
    *expectedSize += (hasSequenceNumber ? EMI_PACKET_SEQUENCE_NUMBER_LENGTH : 0);
    *expectedSize += (hasNak            ? EMI_PACKET_SEQUENCE_NUMBER_LENGTH : 0);
    *expectedSize += (hasLinkCapacity   ? sizeof(uint32_t) : 0);
    *expectedSize += (hasArrivalRate    ? sizeof(uint32_t) : 0);
    *expectedSize += (hasRttResponse    ? EMI_PACKET_SEQUENCE_NUMBER_LENGTH : 0);
}

EmiPacketHeader::EmiPacketHeader() :
flags(0),
sequenceNumber(0),
nak(0),
linkCapacity(0),
arrivalRate(0),
rttResponse(0) {}

EmiPacketHeader::~EmiPacketHeader() {}

bool EmiPacketHeader::parse(const uint8_t *buf, size_t bufSize, EmiPacketHeader *header, size_t *headerLength) {
    if (0 >= bufSize) {
        return false;
    }
    
    EmiPacketFlags flags = *buf;
    
    bool hasSequenceNumber, hasNak, hasLinkCapacity;
    bool hasArrivalRate, hasRttRequest, hasRttResponse;
    size_t expectedSize;
    extractFlagsAndSize(flags,
                        &hasSequenceNumber,
                        &hasNak,
                        &hasLinkCapacity,
                        &hasArrivalRate, 
                        &hasRttRequest,
                        &hasRttResponse,
                        &expectedSize);
    
    if (bufSize < expectedSize) {
        return false;
    }
    
    header->flags = flags;
    header->sequenceNumber = EmiNetUtil::read24(buf+sizeof(EmiPacketFlags));
    header->nak = 0;
    header->linkCapacity = 0;
    header->arrivalRate = 0;
    header->rttResponse = 0;
    
    const uint8_t *bufCur = buf+sizeof(header->flags);
    
    if (hasSequenceNumber) {
        header->sequenceNumber = EmiNetUtil::read24(bufCur);
        bufCur += EMI_PACKET_SEQUENCE_NUMBER_LENGTH;
    }
    
    if (hasNak) {
        header->nak = EmiNetUtil::read24(bufCur);
        bufCur += EMI_PACKET_SEQUENCE_NUMBER_LENGTH;
    }
    
    if (hasLinkCapacity) {
        header->linkCapacity = *((int32_t *)bufCur);
        bufCur += sizeof(header->linkCapacity);
    }
    
    if (hasArrivalRate) {
        header->arrivalRate = *((int32_t *)bufCur);
        bufCur += sizeof(header->arrivalRate);
    }
    
    if (hasRttResponse) {
        header->rttResponse = EmiNetUtil::read24(bufCur);
        bufCur += EMI_PACKET_SEQUENCE_NUMBER_LENGTH;
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
    
    bool hasSequenceNumber, hasNak, hasLinkCapacity;
    bool hasArrivalRate, hasRttRequest, hasRttResponse;
    size_t expectedSize;
    extractFlagsAndSize(header.flags,
                        &hasSequenceNumber,
                        &hasNak,
                        &hasLinkCapacity,
                        &hasArrivalRate, 
                        &hasRttRequest,
                        &hasRttResponse,
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
    
    if (hasNak) {
        EmiNetUtil::write24(bufCur, header.nak);
        bufCur += EMI_PACKET_SEQUENCE_NUMBER_LENGTH;
    }
    
    if (hasLinkCapacity) {
        *((int32_t *)bufCur) = header.linkCapacity;
        bufCur += sizeof(header.linkCapacity);
    }
    
    if (hasArrivalRate) {
        *((int32_t *)bufCur) = header.arrivalRate;
        bufCur += sizeof(header.arrivalRate);
    }
    
    if (hasRttResponse) {
        EmiNetUtil::write24(bufCur, header.rttResponse);
        bufCur += EMI_PACKET_SEQUENCE_NUMBER_LENGTH;
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
