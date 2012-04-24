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

template<class P2PSockDelegate, int CookieSize>
class EmiP2PConn {
    typedef typename P2PSockDelegate::Binding Binding;
    typedef typename Binding::Address         Address;
    typedef typename Binding::AddressCmp      AddressCmp;
    typedef typename Binding::SocketHandle    SocketHandle;
    typedef typename Binding::TemporaryData   TemporaryData;
    
private:
    // Private copy constructor and assignment operator
    inline EmiP2PConn(const EmiP2PConn& other);
    inline EmiP2PConn& operator=(const EmiP2PConn& other);
    
    const AddressCmp _acmp;
    Address          _peers[2];
    Address          _innerEndpoints[2];
    uint8_t          _cookie[CookieSize];
    size_t           _bytesSentSinceRateLimitTimeout;
    
    // TODO connection timeout
    // TODO rate limit timeout
    
    // Returns NULL on error
    const Address* otherAddress(const Address& address) {
        bool eq0 = (0 == _acmp(_peers[0], address));
        bool eq1 = (1 == _acmp(_peers[1], address));
        
        // TODO Return NULL if the address is a nil address
        
        if (eq0) {
            return &_peers[1];
        }
        else if (eq1) {
            return &_peers[0];
        }
        else {
            return NULL;
        }
    }
    
    void sendData(uv_udp_t *sock,
                  const Address& address,
                  const TemporaryData& data,
                  size_t offset,
                  size_t len) {
        // TODO Implement rate limiting
        _bytesSentSinceRateLimitTimeout += len;
        
        P2PSockDelegate::sendData(sock,
                                  address,
                                  Binding::extractData(data)+offset,
                                  len);
    }
    
public:
    EmiP2PConn(const Address& firstPeer, const uint8_t *cookie, size_t cookieSize) :
    _acmp(AddressCmp()),
    _bytesSentSinceRateLimitTimeout(0) {
        int family = EmiBinding::extractFamily(firstPeer);
        
        _peers[0] = firstPeer;
        EmiBinding::fillNilAddress(family, _peers[1]);
        EmiBinding::fillNilAddress(family, _innerEndpoints[0]);
        EmiBinding::fillNilAddress(family, _innerEndpoints[1]);
        
        ASSERT(CookieSize == cookieSize);
        memcpy(_cookie, cookie, CookieSize);
    }
    virtual ~EmiP2PConn() {}
    
    void gotPacket() {}
    
    void forwardPacket(EmiTimeInterval now,
                       SocketHandle *sock,
                       const Address& address,
                       const TemporaryData& data,
                       size_t offset,
                       size_t len) {
        const Address *otherAddr = otherAddress(address);
        if (!otherAddr) {
            return;
        }
        
        sendData(sock, *otherAddr, data, offset, len);
    }
};

#endif
