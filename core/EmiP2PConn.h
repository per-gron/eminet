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
#include "EmiUdpSocket.h"

template<class Binding, class Delegate, int EMI_P2P_RAND_NUM_SIZE>
class EmiP2PConn {
public:
    
    class ConnCookieRandNum {
    public:
        uint8_t randNum[EMI_P2P_RAND_NUM_SIZE];
        
        ConnCookieRandNum(const uint8_t *cookie_, size_t cookieLength) {
            ASSERT(cookieLength >= EMI_P2P_RAND_NUM_SIZE);
            memcpy(randNum, cookie_, sizeof(randNum));
        }
        
        inline ConnCookieRandNum(const ConnCookieRandNum& other) {
            memcpy(randNum, other.randNum, sizeof(randNum));
        }
        inline ConnCookieRandNum& operator=(const ConnCookieRandNum& other) {
            memcpy(randNum, other.randNum, sizeof(randNum));
        }
        
        inline bool operator<(const ConnCookieRandNum& rhs) const {
            return 0 > memcmp(randNum, rhs.randNum, sizeof(randNum));
        }
    };
    
private:
    
    typedef typename Binding::SocketHandle    SocketHandle;
    typedef typename Binding::TemporaryData   TemporaryData;
    typedef typename Binding::Timer           Timer;
    typedef typename Binding::TimerCookie     TimerCookie;
    
    typedef EmiRtoTimer<Binding, EmiP2PConn> ERT;
    
    friend class EmiRtoTimer<Binding, EmiP2PConn>;
    
    // Private copy constructor and assignment operator
    inline EmiP2PConn(const EmiP2PConn& other);
    inline EmiP2PConn& operator=(const EmiP2PConn& other);
    
    Delegate& _delegate;
    
    EmiUdpSocket<Binding> *_sock;
    bool                   _firstPeerHadComplementaryCookie;
    sockaddr_storage       _peers[2];
    sockaddr_storage       _innerEndpoints[2];
    // EmiP2PConn itself is transparent with regards to timing; for
    // the peers it seems like they are communicating directly with
    // each other (but with a slightly slower and lossier connection).
    //
    // EmiP2PConn "eavesdrops" on the connection to both peers to
    // infer RTO/RTT times.
    EmiConnTime            _times[2];
    bool                   _waitingForPrxAck[2];
    // We need to store the inbound address for the SYN-RST packets for rtoTimeout
    sockaddr_storage       _synRstInboundAddr;
    EmiSequenceNumber      _initialSequenceNumbers[2];
    
    const size_t     _rateLimit;
    size_t           _bytesSentSinceRateLimitTimeout;
    
    ERT                    _rtoTimer0;
    ERT                    _rtoTimer1;
    Timer                 *_rateLimitTimer;
    
    // Returns -1 on error
    int addressIndex(const sockaddr_storage& address) const {
        if (0 == EmiAddressCmp::compare(_peers[0], address)) {
            return 0;
        }
        else if (0 == EmiAddressCmp::compare(_peers[1], address)) {
            return 1;
        }
        else {
            return -1;
        }
    }
    
    // Sends data and makes it count towards the rate limit
    void sendData(const sockaddr_storage& inboundAddress,
                  int remoteAddressIndex,
                  const TemporaryData& data,
                  size_t offset,
                  size_t len) {
        
        const sockaddr_storage& remoteAddress(_peers[remoteAddressIndex]);
        
        if (_rateLimit) {
            _bytesSentSinceRateLimitTimeout += len;
            if (_bytesSentSinceRateLimitTimeout > _rateLimit) {
                return;
            }
        }
        
        uint8_t *rawData = (uint8_t *)Binding::extractData(data)+offset;
        
        _sock->sendData(inboundAddress, remoteAddress, rawData, len);
    }
    
    void sendSynRst(const sockaddr_storage& inboundAddress, int addrIdx) {
        EmiSequenceNumber otherHostSN = _initialSequenceNumbers[0 == addrIdx ? 1 : 0];
        
        uint8_t buf[96];
        size_t size = EmiMessage<Binding>::writeControlPacket(EMI_SYN_FLAG | EMI_RST_FLAG, buf, sizeof(buf), otherHostSN);
        ASSERT(0 != size); // size == 0 when the buffer was too small
        _sock->sendData(inboundAddress, _peers[addrIdx], buf, size);
    }
    
    static void rateLimitTimeoutCallback(EmiTimeInterval now, Timer *timer, void *data) {
        EmiP2PConn *conn = (EmiP2PConn *)data;
        
        conn->_bytesSentSinceRateLimitTimeout = 0;
    }
    
    inline void connectionLost()     { ASSERT(0 && "Internal error"); }
    inline void connectionRegained() { ASSERT(0 && "Internal error"); }
    
    inline void connectionTimeout() {
        // Note: This will deallocate this object
        _delegate.removeConnection(this);
    }
    
public:
    const ConnCookieRandNum cookie;
    
    EmiP2PConn(Delegate& delegate,
               const TimerCookie& timerCookie,
               EmiSequenceNumber initialSequenceNumber,
               const ConnCookieRandNum &cookie_,
               bool firstPeerHadComplementaryCookie,
               EmiUdpSocket<Binding> *sock,
               const sockaddr_storage& firstPeer,
               EmiTimeInterval connectionTimeout,
               EmiTimeInterval initialConnectionTimeout,
               size_t rateLimit) :
    cookie(cookie_),
    _firstPeerHadComplementaryCookie(firstPeerHadComplementaryCookie),
    _delegate(delegate),
    _sock(sock),
    _times(),
    _rateLimit(rateLimit),
    _bytesSentSinceRateLimitTimeout(0),
    _rtoTimer0(/*timeBeforeConnectionWarning:*/-1, connectionTimeout, initialConnectionTimeout, _times[0], timerCookie, *this),
    _rtoTimer1(/*timeBeforeConnectionWarning:*/-1, connectionTimeout, initialConnectionTimeout, _times[1], timerCookie, *this),
    _rateLimitTimer(rateLimit ? Binding::makeTimer(timerCookie) : NULL) {
        int family = firstPeer.ss_family;
        
        _peers[0] = firstPeer;
        EmiNetUtil::fillNilAddress(family, _peers[1]);
        EmiNetUtil::fillNilAddress(family, _innerEndpoints[0]);
        EmiNetUtil::fillNilAddress(family, _innerEndpoints[1]);
        
        _initialSequenceNumbers[0] = initialSequenceNumber;
        _initialSequenceNumbers[1] = 0;
        
        _waitingForPrxAck[0] = false;
        _waitingForPrxAck[1] = false;
        EmiNetUtil::fillNilAddress(family, _synRstInboundAddr);
        
        if (rateLimit) {
            Binding::scheduleTimer(_rateLimitTimer, rateLimitTimeoutCallback,
                                   this, 1, /*repeating:*/true, /*reschedule:*/true);
        }
    }
    
    virtual ~EmiP2PConn() {
        // _rateLimitTimer is never created if 0 == _rateLimit
        if (_rateLimitTimer) {
            Binding::freeTimer(_rateLimitTimer);
        }
    }
    
    void gotPacket(const sockaddr_storage& address,
                   const EmiPacketHeader& packetHeader,
                   EmiTimeInterval now) {
        int idx(addressIndex(address));
        ASSERT(-1 != idx);
        
        if (0 == idx) _rtoTimer0.gotPacket();
        else          _rtoTimer1.gotPacket();
        
        _times[idx].gotPacket(packetHeader, now);
    }
    
    void forwardPacket(EmiTimeInterval now,
                       const sockaddr_storage& inboundAddress,
                       const sockaddr_storage& remoteAddress,
                       const TemporaryData& data,
                       size_t offset,
                       size_t len) {
        int idx = addressIndex(remoteAddress);
        if (-1 == idx) {
            return;
        }
        
        const int otherAddrIdx(0 == idx ? 1 : 0);
        const sockaddr_storage *otherAddr(&_peers[otherAddrIdx]);
        
        if (EmiNetUtil::isNilAddress(*otherAddr)) {
            return;
        }
        
        sendData(inboundAddress, otherAddrIdx, data, offset, len);
    }
    
    void gotOtherAddress(const sockaddr_storage& inboundAddress,
                         const sockaddr_storage& remoteAddress,
                         EmiSequenceNumber initialSequenceNumber) {
        _peers[1] = remoteAddress;
        _initialSequenceNumbers[1] = initialSequenceNumber;
        
        _waitingForPrxAck[0] = true;
        _waitingForPrxAck[1] = true;
        memcpy(&_synRstInboundAddr, &inboundAddress, sizeof(inboundAddress));
        sendSynRst(inboundAddress, 0);
        sendSynRst(inboundAddress, 1);
        
        // Make sure that we re-send the syn-ack messages on rto timeout
        _rtoTimer0.updateRtoTimeout();
        _rtoTimer1.updateRtoTimeout();
    }
    
    inline const sockaddr_storage& getFirstAddress() const { return _peers[0]; }
    inline const sockaddr_storage& getOtherAddress() const { return _peers[1]; }
    
    bool isInitialSequenceNumberMismatch(const sockaddr_storage& address, EmiSequenceNumber sequenceNumber) const {
        int idx(addressIndex(address));
        
        if (-1 == idx) {
            return true;
        }
        else {
            return _initialSequenceNumbers[idx] != sequenceNumber;
        }
    }
    
    void sendPrx(const sockaddr_storage& inboundAddress,
                 const sockaddr_storage& remoteAddress) {
        uint8_t buf[96];
        size_t size = EmiMessage<Binding>::writeControlPacket(EMI_PRX_FLAG, buf, sizeof(buf));
        ASSERT(0 != size); // size == 0 when the buffer was too small
        
        int idx(addressIndex(remoteAddress));
        ASSERT(-1 != idx);
        _sock->sendData(inboundAddress, remoteAddress, buf, size);
    }
    
    void rtoTimeout(EmiTimeInterval now,
                    EmiTimeInterval rtoWhenRtoTimerWasScheduled) {
        if (_waitingForPrxAck[0]) sendSynRst(_synRstInboundAddr, 0);
        if (_waitingForPrxAck[1]) sendSynRst(_synRstInboundAddr, 1);
    }
    
    bool senderBufferIsEmpty(ERT& ert) const {
        int idx = (&_rtoTimer0 == &ert) ? 0 : 1;
        return !_waitingForPrxAck[idx];
    }
    
    void gotInnerAddress(const sockaddr_storage& address,
                         const sockaddr_storage& innerAddress) {
        int idx(addressIndex(address));
        ASSERT(-1 != idx);
        
        _innerEndpoints[idx] = innerAddress;
        
        // Stop re-sending the SYN-ACK that this PRX-ACK message is a response to
        _waitingForPrxAck[idx] = false;
        (0 == idx ? _rtoTimer0 : _rtoTimer1).updateRtoTimeout();
    }
    
    bool hasBothInnerAddresses() const {
        return !EmiNetUtil::isNilAddress(_innerEndpoints[0]) &&
               !EmiNetUtil::isNilAddress(_innerEndpoints[1]);
    }
    
    inline bool firstPeerHadComplementaryCookie() const {
        return _firstPeerHadComplementaryCookie;
    }
    
    void sendEndpointPair(const sockaddr_storage& inboundAddress, int idx) {
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
            memcpy(bufCur, &myInnerPort, portLen);                            bufCur += portLen;
            EmiNetUtil::extractIp(_peers[idx], bufCur, sizeof(buf));          bufCur += ipLen;
            memcpy(bufCur, &myOuterPort, portLen);                            bufCur += portLen;
            
            EmiNetUtil::extractIp(_innerEndpoints[otherIdx], bufCur, sizeof(buf)); bufCur += ipLen;
            memcpy(bufCur, &otherInnerPort, portLen);                              bufCur += portLen;
            EmiNetUtil::extractIp(_peers[otherIdx], bufCur, sizeof(buf));          bufCur += ipLen;
            memcpy(bufCur, &otherOuterPort, portLen);                              bufCur += portLen;
        }
        
        
        /// Prepare the message headers
        EmiMessageFlags flags(EMI_PRX_FLAG | EMI_RST_FLAG | EMI_SYN_FLAG | EMI_ACK_FLAG);
        uint8_t packetBuf[128];
        size_t size = EmiMessage<Binding>::writeControlPacketWithData(flags, packetBuf, sizeof(packetBuf), buf, dataLen);
        ASSERT(0 != size); // size == 0 when the buffer was too small
        
        /// Actually send the packet
        _sock->sendData(inboundAddress, _peers[idx], packetBuf, size);
    }
};

#endif
