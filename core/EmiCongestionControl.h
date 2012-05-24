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
    
    // State for knowing which ACKs to send and when
    EmiSequenceNumber _newestSeenSequenceNumber;
    EmiSequenceNumber _newestSentSequenceNumber;
    
public:
    EmiCongestionControl();
    virtual ~EmiCongestionControl();
    
    void gotPacket(EmiTimeInterval now, const EmiPacketHeader& packetHeader, size_t packetLength);
    
    // This method is intended to be called once per tick. It returns
    // the newest seen sequence number, or -1 if no sequence number
    // has been seen or if the newest sequence number seen has already
    // been returned once by this method.
    EmiSequenceNumber ack();
    
    inline float linkCapacity() const {
        return _linkCapacity.calculate();
    }
    
    inline float dataArrivalRate() const {
        return _dataArrivalRate.calculate();
    }
};

#endif
