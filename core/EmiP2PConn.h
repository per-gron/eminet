//
//  EmiP2PConn.h
//  roshambo
//
//  Created by Per Eckerdal on 2012-04-24.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#ifndef roshambo_EmiP2PConn_h
#define roshambo_EmiP2PConn_h

#include "EmiTypes.h"
#include "EmiNetUtil.h"
#include "EmiConnTime.h"
#include "EmiMessage.h"
#include "EmiRtoTimer.h"

template<class P2PSockDelegate, int EMI_P2P_COOKIE_SIZE>
class EmiP2PConn {
public:
    
    class ConnCookie {
    public:
        uint8_t cookie[EMI_P2P_COOKIE_SIZE];
        
        ConnCookie(const uint8_t *cookie_, size_t cookieLength) {
            ASSERT(EMI_P2P_COOKIE_SIZE == cookieLength);
            memcpy(cookie, cookie_, sizeof(cookie));
        }
        
        inline ConnCookie(const ConnCookie& other) {
            memcpy(cookie, other.cookie, sizeof(cookie));
        }
        inline ConnCookie& operator=(const ConnCookie& other) {
            memcpy(cookie, other.cookie, sizeof(cookie));
        }
        
        inline bool operator<(const ConnCookie& rhs) const {
            return 0 > memcmp(cookie, rhs.cookie, sizeof(cookie));
        }
    };
    
private:
    typedef typename P2PSockDelegate::Binding Binding;
    typedef typename Binding::Address         Address;
    typedef typename Binding::AddressCmp      AddressCmp;
    typedef typename Binding::SocketHandle    SocketHandle;
    typedef typename Binding::TemporaryData   TemporaryData;
    typedef typename Binding::Timer           Timer;
    
    typedef EmiRtoTimer<Binding, EmiP2PConn> ERT;
    
    // Private copy constructor and assignment operator
    inline EmiP2PConn(const EmiP2PConn& other);
    inline EmiP2PConn& operator=(const EmiP2PConn& other);
    
    SocketHandle     *_sock;
    const AddressCmp  _acmp;
    Address           _peers[2];
    Address           _innerEndpoints[2];
    EmiConnTime       _times[2];
    bool              _waitingForPrxAck[2];
    
    const size_t     _rateLimit;
    size_t           _bytesSentSinceRateLimitTimeout;
    
    Timer *_connectionTimer[2];
    ERT    _rtoTimer0;
    ERT    _rtoTimer1;
    Timer *_rateLimitTimer;
    
    // Returns -1 on error
    int addressIndex(const Address& address) const {
        if (0 == _acmp(_peers[0], address)) {
            return 0;
        }
        else if (0 == _acmp(_peers[1], address)) {
            return 1;
        }
        else {
            return -1;
        }
    }
    
    // Returns NULL on error
    const Address* otherAddress(const Address& address) const {
        int idx = addressIndex(address);
        if (-1 == idx) {
            return NULL;
        }
        
        const Address *addr(&_peers[idx]);
        
        // Return NULL if the address is a nil address
        if (EmiBinding::isNilAddress(*addr)) {
            return NULL;
        }
        
        return addr;
    }
    
    void sendData(const Address& address,
                  const TemporaryData& data,
                  size_t offset,
                  size_t len) {
        _bytesSentSinceRateLimitTimeout += len;
        if (_bytesSentSinceRateLimitTimeout > _rateLimit) {
            return;
        }
        
        P2PSockDelegate::sendData(_sock,
                                  address,
                                  Binding::extractData(data)+offset,
                                  len);
    }
    
    void sendSynAck(int addrIdx) {
        EmiMessage<Binding>::writeControlPacket(EMI_SYN_FLAG | EMI_ACK_FLAG, ^(uint8_t *buf, size_t size) {
            EmiMessage<Binding>::fillTimestamps(_times[addrIdx], buf, size);
            P2PSockDelegate::sendData(_sock, _peers[addrIdx], buf, size);
        });
    }
    
    static void rateLimitTimeoutCallback(EmiTimeInterval now, Timer *timer, void *data) {
        EmiP2PConn *conn = (EmiP2PConn *)data;
        
        conn->_bytesSentSinceRateLimitTimeout = 0;
    }
    
public:
    ConnCookie cookie;
    
    EmiP2PConn(const ConnCookie &cookie_, SocketHandle *sock, const Address& firstPeer, size_t rateLimit) :
    cookie(cookie_),
    _sock(sock),
    _acmp(AddressCmp()),
    _times(),
    _rateLimit(rateLimit),
    _bytesSentSinceRateLimitTimeout(0),
    _rtoTimer0(_times[0], *this),
    _rtoTimer1(_times[0], *this),
    _rateLimitTimer(rateLimit ? Binding::makeTimer() : NULL) {
        int family = EmiBinding::extractFamily(firstPeer);
        
        _peers[0] = firstPeer;
        EmiBinding::fillNilAddress(family, _peers[1]);
        EmiBinding::fillNilAddress(family, _innerEndpoints[0]);
        EmiBinding::fillNilAddress(family, _innerEndpoints[1]);
        
        _waitingForPrxAck[0] = false;
        _waitingForPrxAck[1] = false;
        
        _connectionTimer[0] = Binding::makeTimer();
        _connectionTimer[1] = Binding::makeTimer();
        
        if (rateLimit) {
            Binding::scheduleTimer(_rateLimitTimer, rateLimitTimeoutCallback,
                                   this, 1, /*repeating:*/true);
        }
    }
    
    virtual ~EmiP2PConn() {
        Binding::freeTimer(_connectionTimer[0]);
        Binding::freeTimer(_connectionTimer[1]);
        
        // _rateLimitTimer is never created if 0 == _rateLimit
        if (_rateLimitTimer) {
            Binding::freeTimer(_rateLimitTimer);
        }
    }
    
    void gotPacket(const Address& address) {
        int idx(addressIndex(address));
        if (-1 != idx) {
            // TODO Reset connection timeout
        }
    }
    
    void forwardPacket(EmiTimeInterval now,
                       const Address& address,
                       const TemporaryData& data,
                       size_t offset,
                       size_t len) {
        const Address *otherAddr = otherAddress(address);
        if (!otherAddr) {
            return;
        }
        
        sendData(*otherAddr, data, offset, len);
    }
    
    void gotOtherAddress(const Address& address) {
        _peers[1] = address;
        
        _waitingForPrxAck[0] = true;
        _waitingForPrxAck[1] = true;
        sendSynAck(0);
        sendSynAck(1);
        
        // Make sure that we re-send the syn-ack messages on rto timeout
        _rtoTimer0.updateRtoTimeout();
        _rtoTimer1.updateRtoTimeout();
    }
    
    const Address& getFirstAddress() const { return _peers[0]; }
    const Address& getOtherAddress() const { return _peers[0]; }
    
    void gotTimestamp(const Address& address, EmiTimeInterval now, const uint8_t *data, size_t len) {
        int idx(addressIndex(address));
        if (-1 != idx) {
            // Since these P2P connections don't have a proper heartbeat frequency,
            // we tell EmiConnTime that it is 3. It is a good enough default.
            _times[idx].gotTimestamp(/*heartbeatFrequency:*/3, now, data, len);
        }
    }
    
    void sendPrx(const Address& address) {
        EmiMessage<Binding>::writeControlPacket(EMI_PRX_FLAG, ^(uint8_t *buf, size_t size) {
            int idx(addressIndex(address));
            ASSERT(-1 != idx);
            EmiMessage<Binding>::fillTimestamps(_times[idx], buf, size);
            P2PSockDelegate::sendData(_sock, address, buf, size);
        });
    }
    
    void rtoTimeout(EmiTimeInterval now, EmiTimeInterval rtoWhenRtoTimerWasScheduled) {
        if (_waitingForPrxAck[0]) sendSynAck(0);
        if (_waitingForPrxAck[1]) sendSynAck(1);
    }
    
    bool senderBufferIsEmpty(ERT& ert) const {
        int idx = (&_rtoTimer0 == &ert) ? 0 : 1;
        return !_waitingForPrxAck[idx];
    }
    
    void gotInnerAddress(const Address& address, const Address& innerAddress) {
        int idx(addressIndex(address));
        ASSERT(-1 != idx);
        
        _innerEndpoints[0 == idx ? 1 : 0] = innerAddress;
        
        // Stop re-sending the SYN-ACK that this PRX-ACK message is a response to
        _waitingForPrxAck[idx] = false;
        (0 == idx ? _rtoTimer0 : _rtoTimer1).updateRtoTimeout();
    }
    
    bool hasBothInnerAddresses() const {
        return !EmiBinding::isNilAddress(_innerEndpoints[0]) &&
               !EmiBinding::isNilAddress(_innerEndpoints[1]);
    }
    
    void sendEndpointPair(int idx) {
        EmiFlags flags(EMI_PRX_FLAG | EMI_RST_FLAG | EMI_SYN_FLAG | EMI_ACK_FLAG);
        EmiMessage<Binding>::writeControlPacket(flags, ^(uint8_t *buf, size_t size) {
            ASSERT(0 == idx || 1 == idx);
            EmiMessage<Binding>::fillTimestamps(_times[idx], buf, size);
            P2PSockDelegate::sendData(_sock, _peers[idx], buf, size);
            
            // TODO These packets should contain the endpoint pairs
        });
    }
};

#endif
