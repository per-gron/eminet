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

template<class P2PSockDelegate>
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
    EmiConnTime      _times[2];
    size_t           _bytesSentSinceRateLimitTimeout;
    
    // TODO connection timeout
    // TODO rate limit timeout
    
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
    EmiP2PConn(const Address& firstPeer) :
    _acmp(AddressCmp()),
    _times(),
    _bytesSentSinceRateLimitTimeout(0) {
        int family = EmiBinding::extractFamily(firstPeer);
        
        _peers[0] = firstPeer;
        EmiBinding::fillNilAddress(family, _peers[1]);
        EmiBinding::fillNilAddress(family, _innerEndpoints[0]);
        EmiBinding::fillNilAddress(family, _innerEndpoints[1]);
    }
    virtual ~EmiP2PConn() {}
    
    void gotPacket(const Address& address) {
        int idx(addressIndex(address));
        if (-1 != idx) {
            // TODO Reset connection timeout
        }
    }
    
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
    
    void gotOtherAddress(const Address& address) {
        _peers[1] = address;
        
        // TODO Send reliable SYN-ACK message
    }
    
    void gotTimestamp(const Address& address, EmiTimeInterval now, const uint8_t *data, size_t len) {
        int idx(addressIndex(address));
        if (-1 != idx) {
            // Since these P2P connections don't have a proper heartbeat frequency,
            // we tell EmiConnTime that it is 3. It is a good enough default.
            _times[idx].gotTimestamp(/*heartbeatFrequency:*/3, now, data, len);
        }
    }
};

#endif
