//
//  EmiRtoTimer.h
//  roshambo
//
//  Created by Per Eckerdal on 2012-04-25.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#ifndef roshambo_EmiRtoTimer_h
#define roshambo_EmiRtoTimer_h

// This class is separated from EmiConnTimers because its functionality
// is shared between EmiConn and EmiP2PConn, while only EmiConn uses
// EmiConnTimers. The class is also used by EmiLogicalConnection for
// (re)sending the NAT punchthrough packets.
template<class Binding, class Delegate>
class EmiRtoTimer {
    
    typedef typename Binding::Timer Timer;
    
    EmiConnTime&           _time;
    Timer                 *_rtoTimer;
    EmiTimeInterval        _rtoWhenRtoTimerWasScheduled;
    Timer                 *_connectionTimer;
    const EmiTimeInterval  _timeBeforeConnectionWarning;
    const EmiTimeInterval  _connectionTimeout;
    bool                   _issuedConnectionWarning;
    
    Delegate& _delegate;
    
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
    
    static void connectionTimeoutCallback(EmiTimeInterval now, Timer *timer, void *data) {
        EmiRtoTimer *ert = (EmiRtoTimer *)data;
        
        ert->_delegate.connectionTimeout();
    }
    
    static void connectionWarningCallback(EmiTimeInterval now, Timer *timer, void *data) {
        EmiRtoTimer *ert = (EmiRtoTimer *)data;
        
        ert->_issuedConnectionWarning = true;
        Binding::scheduleTimer(ert->_connectionTimer, connectionTimeoutCallback, ert,
                               ert->_connectionTimeout - ert->_timeBeforeConnectionWarning,
                               /*repeating:*/false);
        
        ert->_delegate.connectionLost();
    }
    
public:
    // A negative timeBeforeConnectionWarning means don't
    // issue connection warnings
    EmiRtoTimer(EmiTimeInterval timeBeforeConnectionWarning, EmiTimeInterval connectionTimeout, EmiConnTime& time, Delegate &delegate) :
    _time(time),
    _rtoTimer(Binding::makeTimer()),
    _rtoWhenRtoTimerWasScheduled(0),
    _connectionTimer(Binding::makeTimer()),
    _timeBeforeConnectionWarning(timeBeforeConnectionWarning),
    _connectionTimeout(connectionTimeout),
    _issuedConnectionWarning(false),
    _delegate(delegate) {
        resetConnectionTimeout();
    }
    
    virtual ~EmiRtoTimer() {
        Binding::freeTimer(_rtoTimer);
        Binding::freeTimer(_connectionTimer);
    }
    
    void forceResetRtoTimer() {
        Binding::descheduleTimer(_rtoTimer);
        updateRtoTimeout();
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
    
    void resetConnectionTimeout() {
        if (_timeBeforeConnectionWarning >= 0 &&
            _timeBeforeConnectionWarning < _connectionTimeout) {
            Binding::scheduleTimer(_connectionTimer, connectionWarningCallback,
                                   this, _timeBeforeConnectionWarning, /*repeating:*/false);
        }
        else {
            Binding::scheduleTimer(_connectionTimer, connectionTimeoutCallback,
                                   this, _connectionTimeout, /*repeating:*/false);
        }
        
        if (_issuedConnectionWarning) {
            _issuedConnectionWarning = false;
            _delegate.connectionRegained();
        }
    }
    
    inline bool issuedConnectionWarning() const {
        return _issuedConnectionWarning;
    }

};

#endif
