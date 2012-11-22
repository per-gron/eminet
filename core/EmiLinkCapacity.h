//
//  EmiLinkCapacity.h
//  eminet
//
//  Created by Per Eckerdal on 2012-05-24.
//  Copyright (c) 2012 Per Eckerdal. All rights reserved.
//

#ifndef eminet_EmiLinkCapacity_h
#define eminet_EmiLinkCapacity_h

#include "EmiTypes.h"
#include "EmiMedianFilter.h"

// This class implements the receiver side logic for calculating
// the link capacity, as used by the UDT congestion control
// algorithm.
class EmiLinkCapacity {
    
    EmiPacketSequenceNumber _lastPacket;
    EmiTimeInterval         _lastPacketTime;
    size_t                  _lastPacketSize;
    // The values this filter handles are in the unit of
    // bytes per second
    EmiMedianFilter<float>  _medianFilter;
    
public:
    EmiLinkCapacity();
    virtual ~EmiLinkCapacity();
    
    void gotPacket(EmiTimeInterval now, EmiPacketSequenceNumber sequenceNumber, size_t packetLength);
    
    // Calculates the current estimated link capaticy, in
    // bytes per second. Note that this method is a bit
    // slow, and is not intended to be called as often as
    // once per packet.
    inline float calculate() const {
        return _medianFilter.calculate();
    }
};

#endif
