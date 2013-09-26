//
//  EmiDispatchTimer.h
//  eminet
//
//  Created by Per Eckerdal on 2012-06-12.
//  Copyright (c) 2012 Per Eckerdal. All rights reserved.
//

#ifndef eminet_EmiDispatchTimer_h
#define eminet_EmiDispatchTimer_h

#include "EmiTypes.h"
#include <dispatch/dispatch.h>

@class EmiDispatchQueueWrapper;

class EmiDispatchTimer;
typedef void (TimerCb)(EmiTimeInterval now, EmiDispatchTimer *timer, void *data);
class EmiDispatchTimer {
    dispatch_queue_t  _timerQueue;
    dispatch_source_t _timer;
    
private:
    // Private copy constructor and assignment operator
    inline EmiDispatchTimer(const EmiDispatchTimer& other);
    inline EmiDispatchTimer& operator=(const EmiDispatchTimer& other);
    
    // Non thread safe version
    void deschedule_();
    // Non thread safe version
    void schedule_(TimerCb *timerCb, void *data, EmiTimeInterval interval,
                   bool repeating, bool reschedule);
    
public:
    EmiDispatchTimer(EmiDispatchQueueWrapper *timerCookie);
    virtual ~EmiDispatchTimer();
    
    void schedule(TimerCb *timerCb, void *data, EmiTimeInterval interval,
                  bool repeating, bool reschedule);
    void deschedule();
};

#endif
