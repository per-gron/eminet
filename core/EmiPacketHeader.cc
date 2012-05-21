//
//  EmiPacketHeader.cc
//  rock
//
//  Created by Per Eckerdal on 2012-05-10.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#include "EmiPacketHeader.h"

#include "EmiNetUtil.h"

bool EmiPacketHeader::parse(const uint8_t *buf, size_t bufSize, EmiPacketHeader& header, size_t *headerLength) {
    if (0 >= bufSize) {
        return false;
    }
    
    EmiPacketFlags flags = *buf;
    
    bool hasNak          = !!(flags & EMI_NAK_PACKET_FLAG);
    bool hasLinkCapacity = !!(flags & EMI_LINK_CAPACITY_PACKET_FLAG);
    bool hasArrivalRate  = !!(flags & EMI_ARRIVAL_RATE_PACKET_FLAG);
    bool hasRttRequest   = !!(flags & EMI_RTT_REQUEST_PACKET_FLAG);
    bool hasRttResponse  = !!(flags & EMI_RTT_RESPONSE_PACKET_FLAG);
    
    static const size_t minSize = sizeof(EmiPacketFlags) + sizeof(EmiTimestamp) + EMI_PACKET_SEQUENCE_NUMBER_LENGTH;
    
    // 1 for the flags byte
    size_t expectedSize = minSize;
    
    expectedSize += (hasNak          ? EMI_PACKET_SEQUENCE_NUMBER_LENGTH : 0);
    expectedSize += (hasLinkCapacity ? sizeof(header.linkCapacity) : 0);
    expectedSize += (hasArrivalRate  ? sizeof(header.linkCapacity) : 0);
    expectedSize += (hasRttRequest   ? EMI_RTT_SEQUENCE_NUMBER_LENGTH : 0);
    expectedSize += (hasRttResponse  ? EMI_RTT_SEQUENCE_NUMBER_LENGTH : 0);
    
    if (bufSize < expectedSize) {
        return false;
    }
    
    header.flags = flags;
    header.timestamp = *((EmiTimestamp *)(buf+sizeof(EmiPacketFlags)));
    header.sequenceNumber = EmiNetUtil::read24(buf+sizeof(EmiPacketFlags)+sizeof(EmiTimestamp));
    header.nak = 0;
    header.linkCapacity = 0;
    header.arrivalRate = 0;
    header.rttRequest = 0;
    header.rttResponse = 0;
    
    const uint8_t *bufCur = buf+minSize;
    
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
    
    if (hasRttRequest) {
        header.rttRequest = *((EmiRttSequenceNumber *)bufCur);
        bufCur += sizeof(EmiRttSequenceNumber);
    }
    
    if (hasRttResponse) {
        header.rttResponse = *((EmiRttSequenceNumber *)bufCur);
        bufCur += sizeof(EmiRttSequenceNumber);
    }
    
    if (headerLength) {
        *headerLength = expectedSize;
    }
    
    return true;
}