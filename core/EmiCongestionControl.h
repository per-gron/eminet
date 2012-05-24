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

// This class implements the main congestion control algorithm.
// It is based on the design of UDT.
class EmiCongestionControl {
    
    size_t _congestionWindow;
    // A sending rate of 0 means that we're in the slow start phase
    float  _sendingRate;
    // TODO Actually update this variable
    size_t _totalDataSentInSlowStart;
    
    EmiLinkCapacity    _linkCapacity;
    EmiDataArrivalRate _dataArrivalRate;
    
    // The average number of NAKs in a congestion period.
    float _avgNakCount;
    // The number of NAKs in the current congestion period.
    int _nakCount;
    // The number of times the rate has been decreased in this
    // congestion period
    int _decCount;
    int _decRandom;
    // The biggest sequence number when last time the
    // packet sending rate is decreased. Initially -1
    EmiPacketSequenceNumber _lastDecSeq;
    
    // State for knowing which ACKs to send and when
    EmiPacketSequenceNumber _newestSeenSequenceNumber;
    EmiPacketSequenceNumber _newestSentSequenceNumber;
    
    float _remoteLinkCapacity;
    float _remoteDataArrivalRate;
    
    void onAck(EmiTimeInterval rtt);
    void onNak(EmiPacketSequenceNumber nak,
               EmiPacketSequenceNumber largestSNSoFar);
    
public:
    EmiCongestionControl();
    virtual ~EmiCongestionControl();
    
    void gotPacket(EmiTimeInterval now, EmiTimeInterval rtt,
                   EmiPacketSequenceNumber largestSNSoFar,
                   const EmiPacketHeader& packetHeader, size_t packetLength);
    
    void onRto();
    void onDataSent(size_t size);
    
    // This method is intended to be called once per tick. It returns
    // the newest seen sequence number, or -1 if no sequence number
    // has been seen or if the newest sequence number seen has already
    // been returned once by this method.
    EmiPacketSequenceNumber ack();
    
    inline float linkCapacity() const {
        return _linkCapacity.calculate();
    }
    
    inline float dataArrivalRate() const {
        return _dataArrivalRate.calculate();
    }
    
    inline float getSendingRate() const {
        return _sendingRate;
    }
};

#endif
