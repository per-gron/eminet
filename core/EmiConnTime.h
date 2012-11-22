//
//  EmiConnTime.h
//  eminet
//
//  Created by Per Eckerdal on 2012-04-09.
//  Copyright (c) 2012 Per Eckerdal. All rights reserved.
//

#ifndef eminet_EmiConnTime_h
#define eminet_EmiConnTime_h

#include "EmiTypes.h"

#include <cstddef>

class EmiPacketHeader;

// This class is responsible for calculating the RTO of
// a connection.
class EmiConnTime {
    // Note that _rto is not completely logically named. It does not
    // contain the value that getRto returns, only a partially computed
    // value. See getRto's implementation for details.
    EmiTimeInterval _rto;
    EmiTimeInterval _srtt; // -1 if not set
    EmiTimeInterval _rttvar; // -1 if not set
    int _expCount; // Number of rto timeouts since last received packet
    
    EmiPacketSequenceNumber _rttRequestSequenceNumber;
    EmiTimeInterval         _rttRequestTime;
    
    void gotRttResponse(EmiTimeInterval rtt);
    
public:
    EmiConnTime();
    
    void swap(EmiConnTime& other);
    
    void onRtoTimeout();
    void gotPacket(const EmiPacketHeader& header, EmiTimeInterval now);
    
    // Returns true if it is time to send an RTT request.
    //
    // Note that this is not just a getter; it modifies the
    // internal state of the object to be able to understand
    // RTT responses. So if this method returns true, the caller
    // should make sure to actually send the RTT request.
    bool rttRequest(EmiTimeInterval now, EmiPacketSequenceNumber sequenceNumber);
    
    inline EmiTimeInterval getRtt() const {
        return _srtt;
    }
    
    EmiTimeInterval getRto() const;
    EmiTimeInterval getNak() const;
};

#endif
