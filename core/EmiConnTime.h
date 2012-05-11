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

class EmiConnTime {
    EmiTimeInterval _initTime; // -1 if not set
    EmiTimestamp _largestReceivedTime;
    bool _hasReceivedTime;
    EmiTimeInterval _gotLargestReceivedTimeAt;
    
    EmiTimeInterval _rto;
    EmiTimeInterval _srtt; // -1 if not set
    EmiTimeInterval _rttvar; // -1 if not set
    
public:
    EmiConnTime();
    
    void swap(EmiConnTime& other);
    
    void onRtoTimeout();
    void gotTimestamp(float heartbeatFrequency, EmiTimeInterval now, const void *buf, size_t bufSize);
    EmiTimeInterval getCurrentTime(EmiTimeInterval now);
    
    EmiTimeInterval getRto() const;
    EmiTimestamp getLargestReceivedTime() const;
    bool hasReceivedTime() const;
    EmiTimeInterval gotLargestReceivedTimeAt() const;
};

#endif
