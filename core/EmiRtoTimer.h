//
//  EmiRtoTimer.h
//  roshambo
//
//  Created by Per Eckerdal on 2012-04-25.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#ifndef roshambo_EmiRtoTimer_h
#define roshambo_EmiRtoTimer_h

template<class Binding, class Delegate>
class EmiRtoTimer {
    
    typedef typename Binding::Timer Timer;
    
    EmiConnTime&     _time;
    Timer           *_rtoTimer;
    EmiTimeInterval  _rtoWhenRtoTimerWasScheduled;
    Delegate&        _delegate;
    
private:
    // Private copy constructor and assignment operator
    inline EmiRtoTimer(const EmiRtoTimer& other);
    inline EmiRtoTimer& operator=(const EmiRtoTimer& other);
    
    static void rtoTimeoutCallback(EmiTimeInterval now, Timer *timer, void *data) {
        EmiRtoTimer *ert = (EmiRtoTimer *)data;
        
        ert->_delegate.rtoTimeout(now, ert->_rtoWhenRtoTimerWasScheduled);
        
        ert->_time.onRtoTimeout();
        
        ert->updateRtoTimeout();
    }
    
public:
    EmiRtoTimer(EmiConnTime& time, Delegate &delegate) :
    _time(time),
    _rtoTimer(Binding::makeTimer()),
    _rtoWhenRtoTimerWasScheduled(0),
    _delegate(delegate) {}
    
    virtual ~EmiRtoTimer() {
        Binding::freeTimer(_rtoTimer);
    }
    
    void updateRtoTimeout() {
        if (!_delegate.senderBufferIsEmpty(*this)) {
            if (!Binding::timerIsActive(_rtoTimer)) {
                
                // this._rto will likely change before the timeout fires. When
                // the timeout fires we want the value of _rto at the time
                // the timeout was set, not when it fires. That's why we store
                // rto here.
                _rtoWhenRtoTimerWasScheduled = _time.getRto();
                Binding::scheduleTimer(_rtoTimer, rtoTimeoutCallback,
                                       this, _rtoWhenRtoTimerWasScheduled,
                                       /*repeating:*/false);
            }
        }
        else {
            // The queue is empty. Clear the timer
            Binding::descheduleTimer(_rtoTimer);
        }
    }

};

#endif
