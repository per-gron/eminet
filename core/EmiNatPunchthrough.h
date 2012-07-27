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
#include "EmiP2PEndpoints.h"

#include <netinet/in.h>

// This class takes care of sending and verifying (and, if
// applicable, resending) PRX-SYN messages, and sending and
// verifying PRX-SYN-ACK messages. After the NAT punchthrough
// handshake process is finished, this class takes care of
// the proxy teardown handshake with the P2P mediator.
template<class Binding, class Delegate>
class EmiNatPunchthrough {
    
    typedef EmiRtoTimer<Binding, EmiNatPunchthrough> ERT;
    typedef typename Binding::TimerCookie TimerCookie;
    
    friend class EmiRtoTimer<Binding, EmiNatPunchthrough>;
    
#define EMI_NAT_PUNCHTHROUGH_PRX_SYN_ACK_SALT "HELLO"
    
private:
    // Private copy constructor and assignment operator
    inline EmiNatPunchthrough(const EmiNatPunchthrough& other);
    inline EmiNatPunchthrough& operator=(const EmiNatPunchthrough& other);
    
    Delegate&               _delegate;
    const EmiSequenceNumber _initialSequenceNumber;
    const sockaddr_storage  _mediatorAddress;
    const EmiP2PData&       _p2p;
    EmiConnTime             _time;
    ERT                     _rtoTimer;
    
    bool _isInProxyTeardownPhase;
    
    EmiP2PEndpoints        _endpoints;
    const sockaddr_storage _peerInnerAddr;
    const sockaddr_storage _peerOuterAddr;
    
    void sendPrxRstPacket() {
        EmiMessageFlags flags(EMI_PRX_FLAG | EMI_RST_FLAG);
        
        uint8_t buf[96];
        size_t size = EmiMessage<Binding>::writeControlPacket(flags, buf, sizeof(buf));
        ASSERT(0 != size); // size is 0 when the buffer was too small
        
        _delegate.sendNatPunchthroughPacket(_mediatorAddress, buf, size);
    }
    
    void sendPrxSynPackets() {
        uint8_t hashBuf[Binding::HMAC_HASH_SIZE];
        
        Binding::hmacHash(_p2p.sharedSecret, _p2p.sharedSecretLength,
                          _endpoints.myEndpointPair, _endpoints.myEndpointPairLength,
                          hashBuf, sizeof(hashBuf));
        
        /// Prepare the message headers
        EmiMessageFlags flags(EMI_PRX_FLAG | EMI_SYN_FLAG);
        uint8_t buf[128];
        size_t size = EmiMessage<Binding>::writeControlPacketWithData(flags, buf, sizeof(buf), hashBuf, sizeof(hashBuf));
        ASSERT(0 != size); // size is 0 when the buffer was too small
        
        /// Actually send the packet(s)
        _delegate.sendNatPunchthroughPacket(_peerInnerAddr, buf, size);
        
        if (0 != EmiAddressCmp::compare(_peerInnerAddr, _peerOuterAddr)) {
            // Send two packets only if the inner and outer endpoints are different
            _delegate.sendNatPunchthroughPacket(_peerOuterAddr, buf, size);
        }
    }
    
    static void hashForPrxSynAck(const EmiP2PData& p2p,
                                 uint8_t *hashBuf, size_t hashBufLen,
                                 uint8_t *endpointPair, size_t endpointPairLen) {
        uint8_t toBeHashed[128];
        ASSERT(sizeof(toBeHashed) >= endpointPairLen+strlen(EMI_NAT_PUNCHTHROUGH_PRX_SYN_ACK_SALT));
        memcpy(toBeHashed, endpointPair, endpointPairLen);
        memcpy(toBeHashed+endpointPairLen,
               EMI_NAT_PUNCHTHROUGH_PRX_SYN_ACK_SALT,
               strlen(EMI_NAT_PUNCHTHROUGH_PRX_SYN_ACK_SALT));
        
        ASSERT(hashBufLen >= Binding::HMAC_HASH_SIZE);
        
        Binding::hmacHash(p2p.sharedSecret, p2p.sharedSecretLength,
                          toBeHashed, endpointPairLen+strlen(EMI_NAT_PUNCHTHROUGH_PRX_SYN_ACK_SALT),
                          hashBuf, hashBufLen);
    }
    
    static void sendPrxSynAckPacket(Delegate& delegate,
                                    const EmiP2PData& p2p,
                                    const EmiP2PEndpoints& endpoints,
                                    const sockaddr_storage& remoteAddr) {
        uint8_t responseHashBuf[Binding::HMAC_HASH_SIZE];
        hashForPrxSynAck(p2p, responseHashBuf, sizeof(responseHashBuf),
                         endpoints.myEndpointPair, endpoints.myEndpointPairLength);
        
        /// Prepare the message headers
        EmiMessageFlags flags(EMI_PRX_FLAG | EMI_SYN_FLAG | EMI_ACK_FLAG);
        uint8_t buf[128];
        size_t size = EmiMessage<Binding>::writeControlPacketWithData(flags, buf, sizeof(buf),
                                                                      responseHashBuf, sizeof(responseHashBuf));
        ASSERT(0 != size); // size == 0 when the buffer was too small
        
        /// Actually send the packet
        delegate.sendNatPunchthroughPacket(remoteAddr, buf, size);
    }
    
    // Invoked by EmiRtoTimer, but this shouldn't happen
    // because connection warnings are disabled when _rtoTimer
    // is initialized.
    inline void connectionLost()     { ASSERT(0 && "Internal error"); }
    inline void connectionRegained() { ASSERT(0 && "Internal error"); }
    
    // Invoked by EmiRtoTimer
    inline void connectionTimeout() {
        _rtoTimer.connectionOpened();
        
        if (_isInProxyTeardownPhase) {
            // We lost the connection to the P2P mediator.
            // There's not much we can do about it, and we
            // don't really care about it anyways.
            _delegate.natPunchthroughTeardownFinished();
        }
        else {
            _delegate.natPunchthroughFinished(/*successs:*/false);
        }
    }
    
    // Invoked by EmiRtoTimer
    inline void rtoTimeout(EmiTimeInterval now, EmiTimeInterval rtoWhenRtoTimerWasScheduled) {
        if (_isInProxyTeardownPhase) {
            // It seems like the PRX-RST packet we sent got lost.
            // Try re-sending it.
            sendPrxRstPacket();
        }
        else {
            // It seems like the PRX-SYN packets we sent got lost.
            // Try re-sending them.
            sendPrxSynPackets();
        }
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
                       const TimerCookie& timerCookie,
                       EmiSequenceNumber initialSequenceNumber,
                       const sockaddr_storage& mediatorAddress,
                       const EmiP2PData& p2p,
                       const EmiP2PEndpoints& endpoints,
                       const sockaddr_storage& peerInnerAddr,
                       const sockaddr_storage& peerOuterAddr) :
    _delegate(delegate),
    _initialSequenceNumber(initialSequenceNumber),
    _mediatorAddress(mediatorAddress),
    _p2p(p2p),
    _time(),
    _rtoTimer(/*disable connection warning:*/-1, connectionTimeout,
              /*initialConnectionTimeout:*/connectionTimeout, _time,
              timerCookie, *this),
    _isInProxyTeardownPhase(false),
    _endpoints(endpoints),
    _peerInnerAddr(peerInnerAddr),
    _peerOuterAddr(peerOuterAddr) {
        _rtoTimer.updateRtoTimeout();
        
        sendPrxSynPackets();
    }
    
    // The endpoints parameter must have non-NULL endpoints in it
    //
    // This method is static because even in valid circumstances,
    // a P2P connection might receive PRX-SYN messages after the
    // P2P connection is already set up and the mediator
    // connection is torn down from its perspective. In such cases,
    // this EmiNatPunchthrough object will have been deleted, but
    // the connection must be able to respond with a valid
    // PRX-SYN-ACK packet anyway.
    static void gotPrxSyn(Delegate& delegate,
                          const EmiP2PData& p2p,
                          const EmiP2PEndpoints& endpoints,
                          const sockaddr_storage& remoteAddr,
                          const uint8_t *data, size_t len) {
        ASSERT(endpoints.myEndpointPair && endpoints.peerEndpointPair);
        
        /// Check that the hash is correct
        uint8_t hashBuf[Binding::HMAC_HASH_SIZE];
        
        Binding::hmacHash(p2p.sharedSecret, p2p.sharedSecretLength,
                          endpoints.peerEndpointPair, endpoints.peerEndpointPairLength,
                          hashBuf, sizeof(hashBuf));
        
        if (len != sizeof(hashBuf) ||
            0 != memcmp(data, hashBuf, len)) {
            // Invalid hash
            return;
        }
        
        /// Respond with PRX-SYN-ACK packet
        sendPrxSynAckPacket(delegate, p2p,
                            endpoints, remoteAddr);
    }
    
    static bool prxSynAckIsValid(const EmiP2PData& p2p,
                                 const EmiP2PEndpoints& endpoints,
                                 const uint8_t *data, size_t len) {
        if (!endpoints.peerEndpointPair || 0 == endpoints.peerEndpointPairLength) {
            return false;
        }
        
        /// Check that the hash is correct
        uint8_t hashBuf[Binding::HMAC_HASH_SIZE];
        hashForPrxSynAck(p2p, hashBuf, sizeof(hashBuf),
                         endpoints.peerEndpointPair, endpoints.peerEndpointPairLength);
        
        return (len == sizeof(hashBuf) &&
                0 == memcmp(data, hashBuf, len));
    }
    
    template<class ConnRtoTimer>
    void gotPrxSynAck(const sockaddr_storage& remoteAddr,
                      const uint8_t *data,
                      size_t len,
                      ConnRtoTimer& connRtoTimer,
                      sockaddr_storage *connsRemoteAddr,
                      EmiConnTime *connsTime) {
        if (_isInProxyTeardownPhase) {
            // We have already received and processed a PRX-SYN-ACK packet.
            // This is a duplicate, and we can safely ignore it.
            return;
        }
        
        if (!prxSynAckIsValid(_p2p, _endpoints, data, len)) {
            // Invalid hash
            return;
        }
        
        /// Update the connection's remote address
        memcpy(connsRemoteAddr, &remoteAddr, sizeof(sockaddr_storage));
        
        /// Because we are now swapping remote hosts with the connection,
        /// we also need to swap EmiConnTime objects with it.
        _time.swap(*connsTime);
        
        /// Make sure both our and the connection's rto timers are updated,
        /// now that we have swapped the EmiConnTime objects.
        _rtoTimer.forceResetRtoTimer();
        connRtoTimer.forceResetRtoTimer();
        
        /// This will make sure we stop re-sending the PRX-SYN message,
        /// and instead re-send the PRX-RST message if necessary.
        _isInProxyTeardownPhase = true;
        
        /// Send a PRX-RST packet to the P2P mediator
        sendPrxRstPacket();
        
        _rtoTimer.connectionOpened();
        
        _delegate.natPunchthroughFinished(/*success:*/true);
    }
    
    void gotPrxRstAck(const sockaddr_storage& remoteAddr) {
        if (0 != EmiAddressCmp::compare(remoteAddr, _mediatorAddress)) {
            // This packet came from an unexpected source. Ignore it.
            return;
        }
        
        _delegate.natPunchthroughTeardownFinished();
    }
    
};

#endif
