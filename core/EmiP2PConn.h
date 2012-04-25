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
    bool             _waitingForPrxAck[2];
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
    
    void sendData(SocketHandle *sock,
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
    
    void sendSynAck(SocketHandle *sock, int addrIdx) {
        EmiMessage<Binding>::writeControlPacket(EMI_SYN_FLAG | EMI_ACK_FLAG, ^(uint8_t *buf, size_t size) {
            EmiMessage<Binding>::fillTimestamps(_times[addrIdx], buf, size);
            P2PSockDelegate::sendData(sock, _peers[addrIdx], buf, size);
        });
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
        
        _waitingForPrxAck[0] = false;
        _waitingForPrxAck[1] = false;
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
    
    void gotOtherAddress(SocketHandle *sock, const Address& address) {
        _peers[1] = address;
        
        _waitingForPrxAck[0] = true;
        _waitingForPrxAck[1] = true;
        sendSynAck(sock, 0);
        sendSynAck(sock, 1);
        
        // TODO Make sure that we re-send the syn-ack messages on rto timeout
    }
    
    void gotTimestamp(const Address& address, EmiTimeInterval now, const uint8_t *data, size_t len) {
        int idx(addressIndex(address));
        if (-1 != idx) {
            // Since these P2P connections don't have a proper heartbeat frequency,
            // we tell EmiConnTime that it is 3. It is a good enough default.
            _times[idx].gotTimestamp(/*heartbeatFrequency:*/3, now, data, len);
        }
    }
    
    void sendPrxPacket(SocketHandle *sock, const Address& address) {
        EmiMessage<Binding>::writeControlPacket(EMI_PRX_FLAG, ^(uint8_t *buf, size_t size) {
            int idx(addressIndex(address));
            ASSERT(-1 != idx);
            EmiMessage<Binding>::fillTimestamps(_times[idx], buf, size);
            P2PSockDelegate::sendData(sock, address, buf, size);
        });
    }
};

#endif
