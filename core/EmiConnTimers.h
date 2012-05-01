//
//  EmiConnTimers.h
//  roshambo
//
//  Created by Per Eckerdal on 2012-05-01.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#ifndef emilir_EmiConnTimers_h
#define emilir_EmiConnTimers_h

#include "EmiSockConfig.h"
#include "EmiConnTime.h"

template<class Binding, class Delegate>
class EmiConnTimers {
    typedef typename Binding::Timer Timer;
    
    bool _sentDataSinceLastHeartbeat;
    
    Delegate &_delegate;
    
    Timer           *_tickTimer;
    Timer           *_heartbeatTimer;
    Timer           *_connectionTimer;
    EmiTimeInterval  _warningTimeoutWhenWarningTimerWasScheduled;
    
    EmiConnTime&     _time;
    Timer           *_rtoTimer;
    EmiTimeInterval  _rtoWhenRtoTimerWasScheduled;
    
    bool _issuedConnectionWarning;

private:
    // Private copy constructor and assignment operator
    inline EmiConnTimers(const EmiConnTimers& other);
    inline EmiConnTimers& operator=(const EmiConnTimers& other);
    
    EmiTimeInterval timeBeforeConnectionWarning() const {
        const EmiSockConfig& sc(_delegate.getEmiSock().config);
        return 1/sc.heartbeatFrequency * sc.heartbeatsBeforeConnectionWarning;
    }
    
    static void connectionTimeoutCallback(EmiTimeInterval now, Timer *timer, void *data) {
        EmiConnTimers *timers = (EmiConnTimers *)data;
        
        timers->_delegate.connectionTimeout();
    }
    static void connectionWarningCallback(EmiTimeInterval now, Timer *timer, void *data) {
        EmiConnTimers *timers = (EmiConnTimers *)data;
        
        EmiTimeInterval connectionTimeout = timers->_delegate.getEmiSock().config.connectionTimeout;
        
        timers->_issuedConnectionWarning = true;
        Binding::scheduleTimer(timers->_connectionTimer, connectionTimeoutCallback, timers,
                               connectionTimeout - timers->_warningTimeoutWhenWarningTimerWasScheduled,
                               /*repeating:*/false);
        
        timers->_delegate.connectionLost();
    }
    
    static void tickTimeoutCallback(EmiTimeInterval now, Timer *timer, void *data) {
        EmiConnTimers *timers = (EmiConnTimers *)data;
        
        
        if (timers->_delegate.flush(now)) {
            timers->resetHeartbeatTimeout();
        }
    }
    
    static void heartbeatTimeoutCallback(EmiTimeInterval now, Timer *timer, void *data) {
        EmiConnTimers *timers = (EmiConnTimers *)data;
        
        if (!timers->_sentDataSinceLastHeartbeat) {
            timers->_delegate.enqueueHeartbeat();
            timers->ensureTickTimeout();
        }
        timers->resetHeartbeatTimeout();
    }
    
    static void rtoTimeoutCallback(EmiTimeInterval now, Timer *timer, void *data) {
        EmiConnTimers *ect = (EmiConnTimers *)data;
        
        ect->_delegate.rtoTimeout(now, ect->_rtoWhenRtoTimerWasScheduled);
        
        ect->_time.onRtoTimeout();
        
        ect->updateRtoTimeout();
    }

public:
    EmiConnTimers(Delegate& delegate, EmiConnTime& time) :
    _delegate(delegate),
    _sentDataSinceLastHeartbeat(false),
    _tickTimer(Binding::makeTimer()),
    _heartbeatTimer(Binding::makeTimer()),
    _connectionTimer(Binding::makeTimer()),
    _warningTimeoutWhenWarningTimerWasScheduled(0),
    _issuedConnectionWarning(false),
    _time(time),
    _rtoTimer(Binding::makeTimer()),
    _rtoWhenRtoTimerWasScheduled(0) {
        resetConnectionTimeout();
    }
    
    virtual ~EmiConnTimers() {
        Binding::freeTimer(_tickTimer);
        Binding::freeTimer(_heartbeatTimer);
        Binding::freeTimer(_connectionTimer);
        Binding::freeTimer(_rtoTimer);
    }
    
    
    void sentPacket() {
        _sentDataSinceLastHeartbeat = true;
    }
    
    void resetConnectionTimeout() {
        EmiTimeInterval warningTimeout = timeBeforeConnectionWarning();
        EmiTimeInterval connectionTimeout = _delegate.getEmiSock().config.connectionTimeout;
        
        if (warningTimeout < connectionTimeout) {
            _warningTimeoutWhenWarningTimerWasScheduled = warningTimeout;
            Binding::scheduleTimer(_connectionTimer, connectionWarningCallback,
                                   this, warningTimeout, /*repeating:*/false);
        }
        else {
            Binding::scheduleTimer(_connectionTimer, connectionTimeoutCallback,
                                   this, connectionTimeout, /*repeating:*/false);
        }
        
        if (_issuedConnectionWarning) {
            _issuedConnectionWarning = false;
            _delegate.connectionRegained();
        }
    }
    
    void resetHeartbeatTimeout() {
        _sentDataSinceLastHeartbeat = false;
        
        // Don't send heartbeats until we've got a response from the remote host
        if (!_delegate.isOpening()) {
            Binding::scheduleTimer(_heartbeatTimer, heartbeatTimeoutCallback,
                                   this, 1/_delegate.getEmiSock().config.heartbeatFrequency,
                                   /*repeating:*/false);
        }
    }
    
    void ensureTickTimeout() {
        if (!Binding::timerIsActive(_tickTimer)) {
            Binding::scheduleTimer(_tickTimer, tickTimeoutCallback, this,
                                   1/_delegate.getEmiSock().config.tickFrequency,
                                   /*repeating:*/false);
        }
    }
    
    void updateRtoTimeout() {
        if (!_delegate.senderBufferIsEmpty()) {
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
    
    inline bool issuedConnectionWarning() const {
        return _issuedConnectionWarning;
    }
};

#endif
