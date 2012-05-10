//
//  EmiNatPunchthrough.h
//  rock
//
//  Created by Per Eckerdal on 2012-05-10.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#ifndef rock_EmiNatPunchthrough_h
#define rock_EmiNatPunchthrough_h

#include "EmiNetUtil.h"
#include "EmiConnTime.h"
#include "EmiRtoTimer.h"
#include "EmiP2PData.h"

#include <netinet/in.h>

// This class takes care of sending (and, if applicable,
// resending) PRX-SYN messages, and verifying PRX-SYN-ACK
// messages.
template<class Binding, class Delegate>
class EmiNatPunchthrough {
    
    typedef EmiRtoTimer<Binding, EmiNatPunchthrough> ERT;
    
    friend class EmiRtoTimer<Binding, EmiNatPunchthrough>;
    
private:
    // Private copy constructor and assignment operator
    inline EmiNatPunchthrough(const EmiNatPunchthrough& other);
    inline EmiNatPunchthrough& operator=(const EmiNatPunchthrough& other);
    
    Delegate&   _delegate;
    const EmiP2PData& _p2p;
    EmiConnTime _time;
    ERT         _rtoTimer;
    
    uint8_t* const         _myEndpointPair;
    const size_t           _myEndpointPairLength;
    const sockaddr_storage _peerInnerAddr;
    const sockaddr_storage _peerOuterAddr;
    
    
    // Invoked by EmiRtoTimer, but this shouldn't happen
    // because connection warnings are disabled when _rtoTimer
    // is initialized.
    inline void connectionLost()     { ASSERT(0 && "Internal error"); }
    inline void connectionRegained() { ASSERT(0 && "Internal error"); }
    
    // Invoked by EmiRtoTimer
    void connectionTimeout() {
        // TODO
    }
    
    // Invoked by EmiRtoTimer
    void rtoTimeout(EmiTimeInterval now, EmiTimeInterval rtoWhenRtoTimerWasScheduled) {
        // TODO
    }
    
    // Invoked by EmiRtoTimer
    inline bool senderBufferIsEmpty(ERT&) const {
        // This is always false, because as soon as the sender buffer
        // gets empty (that is, when our PRX-ACK packet is acknowledged)
        // this entire object has served its purpose and is destroyed.
        return false;
    }
    
public:
    
    EmiNatPunchthrough(EmiTimeInterval connectionTimeout,
                       Delegate& delegate,
                       const EmiP2PData& p2p,
                       const uint8_t *myEndpointPair, size_t myEndpointPairLength,
                       const sockaddr_storage& peerInnerAddr,
                       const sockaddr_storage& peerOuterAddr) :
    _delegate(delegate),
    _p2p(p2p),
    _time(),
    _rtoTimer(/*disable connection warning:*/-1, connectionTimeout, _time, *this),
    _myEndpointPair((uint8_t *)malloc(myEndpointPairLength)),
    _myEndpointPairLength(myEndpointPairLength),
    _peerInnerAddr(peerInnerAddr),
    _peerOuterAddr(peerOuterAddr) {
        memcpy(_myEndpointPair, myEndpointPair, myEndpointPairLength);
        
        _rtoTimer.updateRtoTimeout();
        
        // TODO Send PRX-SYN messages
    }
    
    virtual ~EmiNatPunchthrough() {
        free(_myEndpointPair);
    }
    
};

#endif
