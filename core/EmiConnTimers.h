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
#include "EmiRtoTimer.h"
#include "EmiLossList.h"

template<class Binding, class Delegate>
class EmiConnTimers {
    
    friend class EmiRtoTimer<Binding, EmiConnTimers>;
    
    typedef typename Binding::Timer Timer;
    typedef EmiRtoTimer<Binding, EmiConnTimers> ERT;
    
    bool _sentDataSinceLastHeartbeat;
    
    Delegate &_delegate;
    
    EmiConnTime _time;
    EmiLossList _lossList;
    
    Timer *_nakTimer;
    Timer *_tickTimer;
    Timer *_heartbeatTimer;
    ERT    _rtoTimer;

private:
    // Private copy constructor and assignment operator
    inline EmiConnTimers(const EmiConnTimers& other);
    inline EmiConnTimers& operator=(const EmiConnTimers& other);
    
    inline static EmiTimeInterval timeBeforeConnectionWarning(Delegate& delegate) {
        const EmiSockConfig& sc(delegate.getEmiSock().config);
        return 1/sc.heartbeatFrequency * sc.heartbeatsBeforeConnectionWarning;
    }
    
    static void nakTimeoutCallback(EmiTimeInterval now, Timer *timer, void *data) {
        EmiConnTimers *timers = (EmiConnTimers *)data;
        
        EmiPacketSequenceNumber nak = timers->_lossList.calculateNak(now, timers->_time.getRto());
        
        if (-1 != nak) {
            timers->_delegate.enqueueNak(nak);
            timers->ensureTickTimeout();
        }
        timers->ensureNakTimeout();
    }
    
    static void tickTimeoutCallback(EmiTimeInterval now, Timer *timer, void *data) {
        EmiConnTimers *timers = (EmiConnTimers *)data;
        
        // Tick returns true if a packet has been sent since the last tick
        if (timers->_delegate.tick(now)) {
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
    
    // Invoked by EmiRtoTimer
    inline bool senderBufferIsEmpty(ERT&) const {
        return _delegate.senderBufferIsEmpty();
    }
    
    // Invoked by EmiRtoTimer
    inline void rtoTimeout(EmiTimeInterval now, EmiTimeInterval rtoWhenRtoTimerWasScheduled) {
        _delegate.rtoTimeout(now, rtoWhenRtoTimerWasScheduled);
    }
    
    // Invoked by EmiRtoTimer
    inline void connectionTimeout() {
        _delegate.connectionTimeout();
    }
    
    // Invoked by EmiRtoTimer
    inline void connectionLost() {
        _delegate.connectionLost();
    }
    
    // Invoked by EmiRtoTimer
    inline void connectionRegained() {
        _delegate.connectionRegained();
    }

public:
    EmiConnTimers(Delegate& delegate) :
    _delegate(delegate),
    _time(),
    _lossList(),
    _sentDataSinceLastHeartbeat(false),
    _nakTimer(Binding::makeTimer()),
    _tickTimer(Binding::makeTimer()),
    _heartbeatTimer(Binding::makeTimer()),
    _rtoTimer(timeBeforeConnectionWarning(delegate),
              _delegate.getEmiSock().config.connectionTimeout,
              _delegate.getEmiSock().config.initialConnectionTimeout,
              _time,
              *this) {}
    
    virtual ~EmiConnTimers() {
        Binding::freeTimer(_nakTimer);
        Binding::freeTimer(_tickTimer);
        Binding::freeTimer(_heartbeatTimer);
    }
    
    
    void sentPacket() {
        _sentDataSinceLastHeartbeat = true;
    }
    
    void gotPacket(const EmiPacketHeader& header, EmiTimeInterval now) {
        _time.gotPacket(header, now);
        _lossList.gotPacket(now, header.sequenceNumber);
        _rtoTimer.gotPacket();
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
    
    void ensureNakTimeout() {
        // Don't send NAKs until we've got a response from the remote host
        if (!_delegate.isOpening() &&
            !Binding::timerIsActive(_nakTimer)) {
            Binding::scheduleTimer(_nakTimer, nakTimeoutCallback,
                                   this, _time.getNak(),
                                   /*repeating:*/false);
        }
    }
    
    void ensureTickTimeout() {
        if (!Binding::timerIsActive(_tickTimer)) {
            Binding::scheduleTimer(_tickTimer, tickTimeoutCallback, this,
                                   EMI_TICK_TIME,
                                   /*repeating:*/false);
        }
        ensureNakTimeout();
    }
    
    inline void updateRtoTimeout() {
        _rtoTimer.updateRtoTimeout();
    }
    
    inline void forceResetRtoTimer() {
        _rtoTimer.forceResetRtoTimer();
    }
    
    inline bool issuedConnectionWarning() const {
        return _rtoTimer.issuedConnectionWarning();
    }
    
    inline void connectionOpened() {
        _rtoTimer.connectionOpened();
    }
    
    inline EmiConnTime& getTime() {
        return _time;
    }
    
    inline const EmiConnTime& getTime() const {
        return _time;
    }
};

#endif
