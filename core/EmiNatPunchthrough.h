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
#include "EmiMessage.h"
#include "EmiAddressCmp.h"

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
    
    Delegate&               _delegate;
    const EmiSequenceNumber _initialSequenceNumber;
    const EmiP2PData&       _p2p;
    // TODO What happens with _time once the P2P connection is established? Shouldn't this replace the EmiConn's EmiConnTime?
    EmiConnTime             _time;
    ERT                     _rtoTimer;
    
    uint8_t* const         _myEndpointPair;
    const size_t           _myEndpointPairLength;
    const sockaddr_storage _peerInnerAddr;
    const sockaddr_storage _peerOuterAddr;
    
    void sendPrxSynPackets() {
        uint8_t hashBuf[Binding::HMAC_HASH_SIZE];
        
        Binding::hmacHash(_p2p.sharedSecret, _p2p.sharedSecretLength,
                          _myEndpointPair, _myEndpointPairLength,
                          hashBuf, sizeof(hashBuf));
        
        /// Prepare the message headers
        EmiFlags flags(EMI_PRX_FLAG | EMI_SYN_FLAG);
        EmiMessage<Binding>::template writeControlPacketWithData<128>(flags, hashBuf, sizeof(hashBuf), ^(uint8_t *buf,
                                                                                                         size_t size) {
            /// Fill the timestamps
            EmiMessage<Binding>::fillTimestamps(_time, buf, size);
            
            /// Actually send the packet(s)
            _delegate.sendPrxSynPacket(_peerInnerAddr, buf, size);
            
            if (0 != EmiAddressCmp::compare(_peerInnerAddr, _peerOuterAddr)) {
                // Only send one packet if the inner and outer endpoints are equal
                _delegate.sendPrxSynPacket(_peerOuterAddr, buf, size);
            }
        });
    }
    
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
        // It seems like the PRX-SYN packets we sent got lost.
        // Try re-sending them.
        sendPrxSynPackets();
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
                       EmiSequenceNumber initialSequenceNumber,
                       const EmiP2PData& p2p,
                       const uint8_t *myEndpointPair, size_t myEndpointPairLength,
                       const sockaddr_storage& peerInnerAddr,
                       const sockaddr_storage& peerOuterAddr) :
    _delegate(delegate),
    _initialSequenceNumber(initialSequenceNumber),
    _p2p(p2p),
    _time(),
    _rtoTimer(/*disable connection warning:*/-1, connectionTimeout, _time, *this),
    _myEndpointPair((uint8_t *)malloc(myEndpointPairLength)),
    _myEndpointPairLength(myEndpointPairLength),
    _peerInnerAddr(peerInnerAddr),
    _peerOuterAddr(peerOuterAddr) {
        memcpy(_myEndpointPair, myEndpointPair, myEndpointPairLength);
        
        _rtoTimer.updateRtoTimeout();
        
        sendPrxSynPackets();
    }
    
    virtual ~EmiNatPunchthrough() {
        free(_myEndpointPair);
    }
    
};

#endif
