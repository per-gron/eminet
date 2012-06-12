//
//  EmiDispatchTimer.mm
//  rock
//
//  Created by Per Eckerdal on 2012-06-12.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#include "EmiDispatchTimer.h"

#include "EmiObjCBindingHelper.h"
#import "EmiDispatchQueueWrapper.h"
#include "EmiNetUtil.h"

EmiDispatchTimer::EmiDispatchTimer(EmiDispatchQueueWrapper *timerCookie) :
_timerQueue(timerCookie->queue), _timer(NULL) {
    ASSERT(_timerQueue);
    dispatch_retain(_timerQueue);
}

EmiDispatchTimer::~EmiDispatchTimer() {
    deschedule();
    dispatch_release(_timerQueue);
}

void EmiDispatchTimer::deschedule_() {
    if (_timer) {
        dispatch_source_cancel(_timer);
        dispatch_release(_timer);
        _timer = NULL;
    }
}

void EmiDispatchTimer::schedule_(TimerCb *timerCb, void *data, EmiTimeInterval interval,
                                 bool repeating, bool reschedule) {
    if (!reschedule && _timer) {
        // We were told not to re-schedule the timer. 
        // The timer is already active, so do nothing.
        return;
    }
    
    if (!_timer) {
        _timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, NULL, /*mask:*/0, _timerQueue);
    }
    else {
        dispatch_source_cancel(_timer);
        dispatch_suspend(_timer);
    }
    
    dispatch_source_set_timer(_timer, dispatch_time(DISPATCH_TIME_NOW, interval * NSEC_PER_SEC),
                              interval * NSEC_PER_SEC, /*leeway:*/0);
    EmiDispatchTimer *timer = this;
    
    dispatch_source_set_event_handler(_timer, ^{
        if (!repeating) {
            timer->deschedule_();
        }
        
        timerCb([NSDate timeIntervalSinceReferenceDate], timer, data);
    });
    
    dispatch_resume(_timer);
}

void EmiDispatchTimer::schedule(TimerCb *timerCb, void *data, EmiTimeInterval interval,
                                 bool repeating, bool reschedule) {
    DISPATCH_SYNC(_timerQueue, ^{
        schedule_(timerCb, data, interval, repeating, reschedule);
    });
}

void EmiDispatchTimer::deschedule() {
    DISPATCH_SYNC(_timerQueue, ^{
        deschedule_();
    });
}

bool EmiDispatchTimer::isActive() const {
    __block BOOL active;
    DISPATCH_SYNC(_timerQueue, ^{
        active = !!_timer;
    });
    return active;
}
