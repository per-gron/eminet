//
//  EmiConnTime.h
//  roshambo
//
//  Created by Per Eckerdal on 2012-04-09.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#ifndef emilir_EmiConnTime_h
#define emilir_EmiConnTime_h

#include "EmiTypes.h"

#include <cstddef>

class EmiPacketHeader;

class EmiConnTime {
    // Note that _rto is not completely logically named. It does not
    // contain the value that getRto returns, only a partially computed
    // value. See getRto's implementation for details.
    EmiTimeInterval _rto;
    EmiTimeInterval _srtt; // -1 if not set
    EmiTimeInterval _rttvar; // -1 if not set
    int _expCount; // Number of rto timeouts since last received packet
    
public:
    EmiConnTime();
    
    void swap(EmiConnTime& other);
    
    void onRtoTimeout();
    void gotPacket(const EmiPacketHeader& header);
    
    EmiTimeInterval getRto() const;
};

#endif
