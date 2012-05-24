//
//  EmiCongestionControl.h
//  rock
//
//  Created by Per Eckerdal on 2012-05-24.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#ifndef rock_EmiCongestionControl_h
#define rock_EmiCongestionControl_h

#include "EmiLinkCapacity.h"
#include "EmiDataArrivalRate.h"

class EmiPacketHeader;

class EmiCongestionControl {
    
    size_t _congestionWindow;
    float  _sendingRate; // A sending rate of 0 means that we're in the slow start phase
    
    EmiLinkCapacity    _linkCapacity;
    EmiDataArrivalRate _dataArrivalRate;
    
public:
    EmiCongestionControl();
    virtual ~EmiCongestionControl();
    
    void gotPacket(EmiTimeInterval now, const EmiPacketHeader& packetHeader, size_t packetLength);
    
    inline float linkCapacity() const {
        return _linkCapacity.calculate();
    }
    
    inline float dataArrivalRate() const {
        return _dataArrivalRate.calculate();
    }
};

#endif
