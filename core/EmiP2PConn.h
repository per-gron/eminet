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
#include "EmiAddressCmp.h"

template<class P2PSockDelegate, class Delegate, int EMI_P2P_COOKIE_SIZE>
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
    typedef typename Binding::SocketHandle    SocketHandle;
    typedef typename Binding::TemporaryData   TemporaryData;
    typedef typename Binding::Timer           Timer;
    
    typedef EmiRtoTimer<Binding, EmiP2PConn> ERT;
    
    friend class EmiRtoTimer<Binding, EmiP2PConn>;
    
    // Private copy constructor and assignment operator
    inline EmiP2PConn(const EmiP2PConn& other);
    inline EmiP2PConn& operator=(const EmiP2PConn& other);
    
    Delegate& _delegate;
    
    SocketHandle        *_sock;
    const EmiAddressCmp  _acmp;
    sockaddr_storage     _peers[2];
    sockaddr_storage     _innerEndpoints[2];
    EmiConnTime          _times[2];
    bool                 _waitingForPrxAck[2];
    EmiSequenceNumber    _initialSequenceNumbers[2];
    
    const size_t     _rateLimit;
    size_t           _bytesSentSinceRateLimitTimeout;
    
    ERT                    _rtoTimer0;
    ERT                    _rtoTimer1;
    Timer                 *_rateLimitTimer;
    
    // Returns -1 on error
    int addressIndex(const sockaddr_storage& address) const {
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
    const sockaddr_storage* otherAddress(const sockaddr_storage& address) const {
        int idx = addressIndex(address);
        if (-1 == idx) {
            return NULL;
        }
        
        const sockaddr_storage *addr(&_peers[idx]);
        
        // Return NULL if the address is a nil address
        if (EmiBinding::isNilAddress(*addr)) {
            return NULL;
        }
        
        return addr;
    }
    
    void sendData(const sockaddr_storage& address,
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
    
    void sendSynRst(int addrIdx) {
        EmiSequenceNumber otherHostSN = _initialSequenceNumbers[0 == addrIdx ? 1 : 0];
        
        EmiMessage<Binding>::writeControlPacket(EMI_SYN_FLAG | EMI_RST_FLAG, otherHostSN, ^(uint8_t *buf, size_t size) {
            EmiMessage<Binding>::fillTimestamps(_times[addrIdx], buf, size);
            P2PSockDelegate::sendData(_sock, _peers[addrIdx], buf, size);
        });
    }
    
    static void rateLimitTimeoutCallback(EmiTimeInterval now, Timer *timer, void *data) {
        EmiP2PConn *conn = (EmiP2PConn *)data;
        
        conn->_bytesSentSinceRateLimitTimeout = 0;
    }
    
    inline void connectionLost()     { ASSERT(0 && "Internal error"); }
    inline void connectionRegained() { ASSERT(0 && "Internal error"); }
    
    inline void connectionTimeout() {
        _delegate.removeConnection(this);
    }
    
    void resetConnectionTimeout(int idx) {
        if (0 == idx) _rtoTimer0.resetConnectionTimeout();
        else          _rtoTimer1.resetConnectionTimeout();
    }
    
public:
    ConnCookie cookie;
    
    EmiP2PConn(Delegate& delegate,
               EmiSequenceNumber initialSequenceNumber,
               const ConnCookie &cookie_,
               SocketHandle *sock,
               const sockaddr_storage& firstPeer,
               EmiTimeInterval connectionTimeout,
               size_t rateLimit) :
    cookie(cookie_),
    _delegate(delegate),
    _sock(sock),
    _acmp(EmiAddressCmp()),
    _times(),
    _rateLimit(rateLimit),
    _bytesSentSinceRateLimitTimeout(0),
    _rtoTimer0(/*timeBeforeConnectionWarning:*/-1, connectionTimeout, _times[0], *this),
    _rtoTimer1(/*timeBeforeConnectionWarning:*/-1, connectionTimeout, _times[1], *this),
    _rateLimitTimer(rateLimit ? Binding::makeTimer() : NULL) {
        int family = firstPeer.ss_family;
        
        _peers[0] = firstPeer;
        EmiBinding::fillNilAddress(family, _peers[1]);
        EmiBinding::fillNilAddress(family, _innerEndpoints[0]);
        EmiBinding::fillNilAddress(family, _innerEndpoints[1]);
        
        _initialSequenceNumbers[0] = initialSequenceNumber;
        _initialSequenceNumbers[1] = 0;
        
        _waitingForPrxAck[0] = false;
        _waitingForPrxAck[1] = false;
        
        if (rateLimit) {
            Binding::scheduleTimer(_rateLimitTimer, rateLimitTimeoutCallback,
                                   this, 1, /*repeating:*/true);
        }
    }
    
    virtual ~EmiP2PConn() {
        // _rateLimitTimer is never created if 0 == _rateLimit
        if (_rateLimitTimer) {
            Binding::freeTimer(_rateLimitTimer);
        }
    }
    
    void gotPacket(const sockaddr_storage& address) {
        int idx(addressIndex(address));
        ASSERT(-1 != idx);
        
        resetConnectionTimeout(idx);
    }
    
    void forwardPacket(EmiTimeInterval now,
                       const sockaddr_storage& address,
                       const TemporaryData& data,
                       size_t offset,
                       size_t len) {
        const sockaddr_storage *otherAddr = otherAddress(address);
        if (!otherAddr) {
            return;
        }
        
        sendData(*otherAddr, data, offset, len);
    }
    
    void gotOtherAddress(const sockaddr_storage& address, EmiSequenceNumber initialSequenceNumber) {
        _peers[1] = address;
        _initialSequenceNumbers[1] = initialSequenceNumber;
        
        _waitingForPrxAck[0] = true;
        _waitingForPrxAck[1] = true;
        sendSynRst(0);
        sendSynRst(1);
        
        // Make sure that we re-send the syn-ack messages on rto timeout
        _rtoTimer0.updateRtoTimeout();
        _rtoTimer1.updateRtoTimeout();
    }
    
    inline const sockaddr_storage& getFirstAddress() const { return _peers[0]; }
    inline const sockaddr_storage& getOtherAddress() const { return _peers[0]; }
    
    bool isInitialSequenceNumberMismatch(const sockaddr_storage& address, EmiSequenceNumber sequenceNumber) const {
        int idx(addressIndex(address));
        
        if (-1 == idx) {
            return true;
        }
        else {
            return _initialSequenceNumbers[idx] != sequenceNumber;
        }
    }
    
    void gotTimestamp(const sockaddr_storage& address, EmiTimeInterval now, const uint8_t *data, size_t len) {
        int idx(addressIndex(address));
        if (-1 != idx) {
            // Since these P2P connections don't have a proper heartbeat frequency,
            // we tell EmiConnTime that it is 3. It is a good enough default.
            _times[idx].gotTimestamp(/*heartbeatFrequency:*/3, now, data, len);
        }
    }
    
    void sendPrx(const sockaddr_storage& address) {
        EmiMessage<Binding>::writeControlPacket(EMI_PRX_FLAG, ^(uint8_t *buf, size_t size) {
            int idx(addressIndex(address));
            ASSERT(-1 != idx);
            EmiMessage<Binding>::fillTimestamps(_times[idx], buf, size);
            P2PSockDelegate::sendData(_sock, address, buf, size);
        });
    }
    
    void rtoTimeout(EmiTimeInterval now, EmiTimeInterval rtoWhenRtoTimerWasScheduled) {
        if (_waitingForPrxAck[0]) sendSynRst(0);
        if (_waitingForPrxAck[1]) sendSynRst(1);
    }
    
    bool senderBufferIsEmpty(ERT& ert) const {
        int idx = (&_rtoTimer0 == &ert) ? 0 : 1;
        return !_waitingForPrxAck[idx];
    }
    
    void gotInnerAddress(const sockaddr_storage& address, const sockaddr_storage& innerAddress) {
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
        ASSERT(0 == idx || 1 == idx);
        
        int otherIdx = (0 == idx) ? 1 : 0;
        
        const size_t ipLen = EmiNetUtil::ipLength(_peers[idx]);
        static const size_t portLen = sizeof(uint16_t);
        const size_t endpointPairLen = 2*(ipLen+portLen);
        const size_t dataLen = 2*endpointPairLen;
        
        uint8_t buf[96];
        ASSERT(sizeof(buf) >= dataLen);
        
        // All IP addresses we deal with here are in network byte order
        
        // These port numbers are in network byte order
        uint16_t myInnerPort    = EmiNetUtil::addrPortN(_innerEndpoints[idx]);
        uint16_t myOuterPort    = EmiNetUtil::addrPortN(_peers[idx]);
        uint16_t otherInnerPort = EmiNetUtil::addrPortN(_innerEndpoints[otherIdx]);
        uint16_t otherOuterPort = EmiNetUtil::addrPortN(_peers[otherIdx]);
        
        /// Save the endpoint pairs in buf
        {
            uint8_t *bufCur = buf;
            
            EmiNetUtil::extractIp(_innerEndpoints[idx], bufCur, sizeof(buf)); bufCur += ipLen;
            memcpy(buf+ipLen, &myInnerPort, portLen);                         bufCur += portLen;
            EmiNetUtil::extractIp(_peers[idx], bufCur, sizeof(buf));          bufCur += ipLen;
            memcpy(bufCur, &myOuterPort, portLen);                            bufCur += portLen;
            
            EmiNetUtil::extractIp(_innerEndpoints[otherIdx], bufCur, sizeof(buf)); bufCur += ipLen;
            memcpy(buf+ipLen, &otherInnerPort, portLen);                           bufCur += portLen;
            EmiNetUtil::extractIp(_peers[otherIdx], bufCur, sizeof(buf));          bufCur += ipLen;
            memcpy(bufCur, &otherOuterPort, portLen);                              bufCur += portLen;
        }
        
        
        /// Prepare the message headers
        EmiFlags flags(EMI_PRX_FLAG | EMI_RST_FLAG | EMI_SYN_FLAG | EMI_ACK_FLAG);
        EmiMessage<Binding>::template writeControlPacketWithData<128>(flags, buf, dataLen, ^(uint8_t *buf, size_t size) {
            /// Fill the timestamps
            EmiMessage<Binding>::fillTimestamps(_times[idx], buf, size);
            
            /// Actually send the packet
            P2PSockDelegate::sendData(_sock, _peers[idx], buf, size);
        });
    }
};

#endif
