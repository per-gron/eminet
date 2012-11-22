//
//  EmiLinkCapacity.cc
//  eminet
//
//  Created by Per Eckerdal on 2012-05-24.
//  Copyright (c) 2012 Per Eckerdal. All rights reserved.
//

#include "EmiLinkCapacity.h"

#include "EmiNetUtil.h"

EmiLinkCapacity::EmiLinkCapacity() :
_lastPacket(-1),
_lastPacketTime(0),
_lastPacketSize(0),
// Start with 512 B/s. That should be a sufficiently
// conservative initial choice, while avoiding to
// confuse the congestion control algorithm with a
// much too low value (such as 0 or 1)
_medianFilter(512) {}

EmiLinkCapacity::~EmiLinkCapacity() {}

void EmiLinkCapacity::gotPacket(EmiTimeInterval now, EmiPacketSequenceNumber sequenceNumber, size_t packetLength) {
    EmiPacketSequenceNumber snMod16 = (sequenceNumber % EMI_PACKET_PAIR_INTERVAL);
    
    if (0 == snMod16) {
        _lastPacket = sequenceNumber;
        _lastPacketTime = now;
        _lastPacketSize = packetLength;
    }
    else if (1 == snMod16 &&
             _lastPacketSize == packetLength &&
             1 == EmiNetUtil::cyclicDifferenceSigned<EMI_PACKET_SEQUENCE_NUMBER_LENGTH>(sequenceNumber, _lastPacket)) {
        
        EmiTimeInterval timeDifference = now-_lastPacketTime;
        if (0 != timeDifference) {
            _medianFilter.pushValue(packetLength/timeDifference);
        }
        
        // We only want to count this packet pair once.
        _lastPacket = -1;
    }
}
