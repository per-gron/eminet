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
    
    typedef typename Binding::Timer       Timer;
    typedef typename Binding::TimerCookie TimerCookie;
    
    EmiConnTime&           _time;
    Timer                 *_rtoTimer;
    EmiTimeInterval        _rtoWhenRtoTimerWasScheduled;
    Timer                 *_connectionTimer;
    const EmiTimeInterval  _timeBeforeConnectionWarning;
    const EmiTimeInterval  _connectionTimeout;
    const EmiTimeInterval  _initialConnectionTimeout;
    bool                   _connectionOpen;
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
    
    inline EmiTimeInterval getConnectionTimeout() const {
        return _connectionOpen ? _connectionTimeout : _initialConnectionTimeout;
    }
    
    static void connectionWarningCallback(EmiTimeInterval now, Timer *timer, void *data) {
        EmiRtoTimer *ert = (EmiRtoTimer *)data;
        
        ert->_issuedConnectionWarning = true;
        Binding::scheduleTimer(ert->_connectionTimer, connectionTimeoutCallback, ert,
                               ert->getConnectionTimeout() - ert->_timeBeforeConnectionWarning,
                               /*repeating:*/false, /*reschedule:*/true);
        
        ert->_delegate.connectionLost();
    }
    
    void resetConnectionTimeout() {
        EmiTimeInterval connectionTimeout = getConnectionTimeout();
        
        if (_timeBeforeConnectionWarning >= 0 &&
            _timeBeforeConnectionWarning < connectionTimeout) {
            Binding::scheduleTimer(_connectionTimer, connectionWarningCallback,
                                   this, _timeBeforeConnectionWarning,
                                   /*repeating:*/false, /*reschedule:*/true);
        }
        else {
            Binding::scheduleTimer(_connectionTimer, connectionTimeoutCallback,
                                   this, connectionTimeout,
                                   /*repeating:*/false, /*reschedule:*/true);
        }
        
        if (_issuedConnectionWarning) {
            _issuedConnectionWarning = false;
            _delegate.connectionRegained();
        }
    }
    
public:
    // A negative timeBeforeConnectionWarning means don't
    // issue connection warnings
    EmiRtoTimer(EmiTimeInterval timeBeforeConnectionWarning,
                EmiTimeInterval connectionTimeout,
                EmiTimeInterval initialConnectionTimeout,
                EmiConnTime& time,
                const TimerCookie& timerCookie,
                Delegate &delegate) :
    _time(time),
    _rtoTimer(Binding::makeTimer(timerCookie)),
    _rtoWhenRtoTimerWasScheduled(0),
    _connectionTimer(Binding::makeTimer(timerCookie)),
    _timeBeforeConnectionWarning(timeBeforeConnectionWarning),
    _connectionTimeout(connectionTimeout),
    _initialConnectionTimeout(initialConnectionTimeout),
    _connectionOpen(false),
    _issuedConnectionWarning(false),
    _delegate(delegate) {
        resetConnectionTimeout();
    }
    
    virtual ~EmiRtoTimer() {
        Binding::freeTimer(_rtoTimer);
        Binding::freeTimer(_connectionTimer);
    }
    
    void deschedule() {
        Binding::descheduleTimer(_rtoTimer);
        Binding::descheduleTimer(_connectionTimer);
    }
    
    void forceResetRtoTimer() {
        Binding::descheduleTimer(_rtoTimer);
        updateRtoTimeout();
    }
    
    void updateRtoTimeout() {
        if (!_delegate.senderBufferIsEmpty(*this)) {
            // this._rto will likely change before the timeout fires. When
            // the timeout fires we want the value of _rto at the time
            // the timeout was set, not when it fires. That's why we store
            // rto here.
            _rtoWhenRtoTimerWasScheduled = _time.getRto();
            Binding::scheduleTimer(_rtoTimer, rtoTimeoutCallback,
                                   this, _rtoWhenRtoTimerWasScheduled,
                                   /*repeating:*/false, /*reschedule:*/false);
        }
        else {
            // The queue is empty. Clear the timer
            Binding::descheduleTimer(_rtoTimer);
        }
    }
    
    inline void gotPacket() {
        resetConnectionTimeout();
    }
    
    inline void connectionOpened() {
        _connectionOpen = true;
    }
    
    inline bool issuedConnectionWarning() const {
        return _issuedConnectionWarning;
    }

};

#endif
