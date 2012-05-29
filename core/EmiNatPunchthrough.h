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

// This class takes care of sending and verifying (and, if
// applicable, resending) PRX-SYN messages, and sending and
// verifying PRX-SYN-ACK messages. After the NAT punchthrough
// handshake process is finished, this class takes care of
// the proxy teardown handshake with the P2P mediator.
template<class Binding, class Delegate>
class EmiNatPunchthrough {
    
    typedef EmiRtoTimer<Binding, EmiNatPunchthrough> ERT;
    
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
    
    uint8_t* const         _myEndpointPair;
    const size_t           _myEndpointPairLength;
    uint8_t* const         _peerEndpointPair;
    const size_t           _peerEndpointPairLength;
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
                          _myEndpointPair, _myEndpointPairLength,
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
    
    void hashForPrxSynAck(uint8_t *hashBuf, size_t hashBufLen,
                          uint8_t *endpointPair, size_t endpointPairLen) {
        uint8_t toBeHashed[128];
        ASSERT(sizeof(toBeHashed) >= endpointPairLen+strlen(EMI_NAT_PUNCHTHROUGH_PRX_SYN_ACK_SALT));
        memcpy(toBeHashed, endpointPair, endpointPairLen);
        memcpy(toBeHashed+endpointPairLen,
               EMI_NAT_PUNCHTHROUGH_PRX_SYN_ACK_SALT,
               strlen(EMI_NAT_PUNCHTHROUGH_PRX_SYN_ACK_SALT));
        
        ASSERT(hashBufLen >= Binding::HMAC_HASH_SIZE);
        
        Binding::hmacHash(_p2p.sharedSecret, _p2p.sharedSecretLength,
                          toBeHashed, endpointPairLen+strlen(EMI_NAT_PUNCHTHROUGH_PRX_SYN_ACK_SALT),
                          hashBuf, sizeof(hashBuf));
    }
    
    void sendPrxSynAckPacket(const sockaddr_storage& remoteAddr) {
        uint8_t hashBuf[Binding::HMAC_HASH_SIZE];
        hashForPrxSynAck(hashBuf, sizeof(hashBuf), _myEndpointPair, _myEndpointPairLength);
        
        /// Prepare the message headers
        EmiMessageFlags flags(EMI_PRX_FLAG | EMI_SYN_FLAG | EMI_ACK_FLAG);
        uint8_t buf[128];
        size_t size = EmiMessage<Binding>::writeControlPacketWithData(flags, buf, sizeof(buf), hashBuf, sizeof(hashBuf));
        ASSERT(0 != size); // size == 0 when the buffer was too small
        
        /// Actually send the packet
        _delegate.sendNatPunchthroughPacket(remoteAddr, buf, size);
    }
    
    // Invoked by EmiRtoTimer, but this shouldn't happen
    // because connection warnings are disabled when _rtoTimer
    // is initialized.
    inline void connectionLost()     { ASSERT(0 && "Internal error"); }
    inline void connectionRegained() { ASSERT(0 && "Internal error"); }
    
    // Invoked by EmiRtoTimer
    inline void connectionTimeout() {
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
                       EmiSequenceNumber initialSequenceNumber,
                       const sockaddr_storage& mediatorAddress,
                       const EmiP2PData& p2p,
                       const uint8_t *myEndpointPair, size_t myEndpointPairLength,
                       const uint8_t *peerEndpointPair, size_t peerEndpointPairLength,
                       const sockaddr_storage& peerInnerAddr,
                       const sockaddr_storage& peerOuterAddr) :
    _delegate(delegate),
    _initialSequenceNumber(initialSequenceNumber),
    _mediatorAddress(mediatorAddress),
    _p2p(p2p),
    _time(),
    _rtoTimer(/*disable connection warning:*/-1, connectionTimeout,
              /*initialConnectionTimeout:*/connectionTimeout, _time, *this),
    _isInProxyTeardownPhase(false),
    _myEndpointPair((uint8_t *)malloc(myEndpointPairLength)),
    _myEndpointPairLength(myEndpointPairLength),
    _peerEndpointPair((uint8_t *)malloc(peerEndpointPairLength)),
    _peerEndpointPairLength(peerEndpointPairLength),
    _peerInnerAddr(peerInnerAddr),
    _peerOuterAddr(peerOuterAddr) {
        memcpy(_myEndpointPair,   myEndpointPair,   myEndpointPairLength);
        memcpy(_peerEndpointPair, peerEndpointPair, peerEndpointPairLength);
        
        _rtoTimer.updateRtoTimeout();
        
        sendPrxSynPackets();
    }
    
    virtual ~EmiNatPunchthrough() {
        free(_myEndpointPair);
    }
    
    void gotPrxSyn(const sockaddr_storage& remoteAddr,
                   const uint8_t *data,
                   size_t len) {
        /// Check that the hash is correct
        uint8_t hashBuf[Binding::HMAC_HASH_SIZE];
        
        Binding::hmacHash(_p2p.sharedSecret, _p2p.sharedSecretLength,
                          _peerEndpointPair, _peerEndpointPairLength,
                          hashBuf, sizeof(hashBuf));
        
        if (len != sizeof(hashBuf) ||
            0 != memcmp(data, hashBuf, len)) {
            // Invalid hash
            return;
        }
        
        /// Respond with PRX-SYN-ACK packet
        sendPrxSynAckPacket(remoteAddr);
    }
    
    template<class ConnRtoTimer>
    void gotPrxSynAck(const sockaddr_storage& remoteAddr,
                      const uint8_t *data,
                      size_t len,
                      ConnRtoTimer& connRtoTimer,
                      sockaddr_storage *connsRemoteAddr,
                      EmiConnTime *connsTime) {
        /// Check that the hash is correct
        uint8_t hashBuf[Binding::HMAC_HASH_SIZE];
        hashForPrxSynAck(hashBuf, sizeof(hashBuf), _peerEndpointPair, _peerEndpointPairLength);
        
        if (len != sizeof(hashBuf) ||
            0 != memcmp(data, hashBuf, len)) {
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
        
        /// This will make sure we stop re-sending the PRX-RST message,
        /// and instead re-send the PRX-RST message if necessary.
        _isInProxyTeardownPhase = true;
        
        /// Send a PRX-RST packet to the P2P mediator
        sendPrxRstPacket();
        
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
