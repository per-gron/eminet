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

static const size_t PACKET_MIN_SIZE = sizeof(EmiPacketFlags) + EMI_PACKET_SEQUENCE_NUMBER_LENGTH;

inline bool extractFlagsAndSize(EmiPacketFlags flags,
                                bool *hasNak,
                                bool *hasLinkCapacity,
                                bool *hasArrivalRate, 
                                bool *hasRttRequest,
                                bool *hasRttResponse,
                                size_t *expectedSize) {
    *hasNak          = !!(flags & EMI_NAK_PACKET_FLAG);
    *hasLinkCapacity = !!(flags & EMI_LINK_CAPACITY_PACKET_FLAG);
    *hasArrivalRate  = !!(flags & EMI_ARRIVAL_RATE_PACKET_FLAG);
    *hasRttRequest   = !!(flags & EMI_RTT_REQUEST_PACKET_FLAG);
    *hasRttResponse  = !!(flags & EMI_RTT_RESPONSE_PACKET_FLAG);
    
    // 1 for the flags byte
    *expectedSize = PACKET_MIN_SIZE;
    
    *expectedSize += (hasNak          ? EMI_PACKET_SEQUENCE_NUMBER_LENGTH : 0);
    *expectedSize += (hasLinkCapacity ? sizeof(uint32_t) : 0);
    *expectedSize += (hasArrivalRate  ? sizeof(uint32_t) : 0);
    *expectedSize += (hasRttResponse  ? EMI_PACKET_SEQUENCE_NUMBER_LENGTH : 0);
}

bool EmiPacketHeader::parse(const uint8_t *buf, size_t bufSize, EmiPacketHeader& header, size_t *headerLength) {
    if (0 >= bufSize) {
        return false;
    }
    
    EmiPacketFlags flags = *buf;
    
    bool hasNak, hasLinkCapacity, hasArrivalRate, hasRttRequest, hasRttResponse;
    size_t expectedSize;
    extractFlagsAndSize(flags,
                        &hasNak,
                        &hasLinkCapacity,
                        &hasArrivalRate, 
                        &hasRttRequest,
                        &hasRttResponse,
                        &expectedSize);
    
    if (bufSize < expectedSize) {
        return false;
    }
    
    header.flags = flags;
    header.sequenceNumber = EmiNetUtil::read24(buf+sizeof(EmiPacketFlags));
    header.nak = 0;
    header.linkCapacity = 0;
    header.arrivalRate = 0;
    header.rttResponse = 0;
    
    const uint8_t *bufCur = buf+PACKET_MIN_SIZE;
    
    if (hasNak) {
        header.nak = EmiNetUtil::read24(bufCur);
        bufCur += EMI_PACKET_SEQUENCE_NUMBER_LENGTH;
    }
    
    if (hasLinkCapacity) {
        header.linkCapacity = *((int32_t *)bufCur);
        bufCur += sizeof(header.linkCapacity);
    }
    
    if (hasArrivalRate) {
        header.arrivalRate = *((int32_t *)bufCur);
        bufCur += sizeof(header.arrivalRate);
    }
    
    if (hasRttResponse) {
        header.rttResponse = EmiNetUtil::read24(bufCur);
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
    
    bool hasNak, hasLinkCapacity, hasArrivalRate, hasRttRequest, hasRttResponse;
    size_t expectedSize;
    extractFlagsAndSize(header.flags,
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
    EmiNetUtil::write24(buf+sizeof(header.flags), header.sequenceNumber);
    
    uint8_t *bufCur = buf+PACKET_MIN_SIZE;
    
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
}
