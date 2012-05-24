//
//  EmiDataArrivalRate.h
//  rock
//
//  Created by Per Eckerdal on 2012-05-22.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#ifndef rock_EmiDataArrivalRate_h
#define rock_EmiDataArrivalRate_h

#include "EmiTypes.h"
#include "EmiMedianFilter.h"

// This class implements the logic for calculating the
// data arrival rate, as used by the UDT congestion control
// algorithm.
class EmiDataArrivalRate {
    
    EmiTimeInterval        _lastPacketTime;
    // The values this filter handles are in the unit of
    // bytes per second
    EmiMedianFilter<float> _medianFilter;
    
public:
    
    EmiDataArrivalRate();
    virtual ~EmiDataArrivalRate();
    
    // Call this when a packet has been received. This
    // method is fast.
    inline void gotPacket(EmiTimeInterval now, size_t packetLength) {
        if (-1 != _lastPacketTime) {
            _medianFilter.pushValue(packetLength/(now-_lastPacketTime));
        }
        _lastPacketTime = now;
    }
    
    // Calculates the current data arrival rate, in
    // bytes per second. Note that this method is
    // a bit slow, and is not intended to be called
    // as often as once per packet.
    inline float calculate() const {
        return _medianFilter.calculate();
    }
};

#endif
