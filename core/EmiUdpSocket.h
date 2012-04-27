//
//  EmiUdpSocket.h
//  roshambo
//
//  Created by Per Eckerdal on 2012-04-27.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#ifndef roshambo_EmiUdpSocket_h
#define roshambo_EmiUdpSocket_h

#include <netinet/in.h>
#include <vector>

// The purpose of this class is to encapsulate opening one UDP socket
// per network interface, to be able to tell which the receiver address
// of each datagram is.
template<class SockDelegate>
class EmiUdpSocket {
private:
    // Private copy constructor and assignment operator
    inline EmiUdpSocket(const EmiUdpSocket& other);
    inline EmiUdpSocket& operator=(const EmiUdpSocket& other);
    
    typedef typename SockDelegate::Binding             Binding;
    typedef typename Binding::NetworkInterfaces        NetworkInterfaces;
    typedef typename Binding::Error                    Error;
    typedef typename Binding::SocketHandle             SocketHandle;
    typedef std::vector<SocketHandle*>                 SocketVector;
    typedef typename SocketVector::iterator            SocketVectorIter;
    
    SockDelegate& _delegate;
    SocketVector  _sockets;
    uint16_t      _localPort;
    
    EmiUdpSocket(SockDelegate& delegate) :
    _delegate(delegate), _sockets(), _localPort(0) {}
    
    bool init(const sockaddr_storage& address, Error& err) {
        NetworkInterfaces ni;
        
        if (!Binding::getNetworkInterfaces(ni, err)) {
            return false;
        }
        
        const char *ifName;
        sockaddr_storage ifAddr;
        while (Binding::nextNetworkInterface(ni, ifName, ifAddr)) {
            if (ifAddr.ss_family != address.ss_family) {
                continue;
            }
            
            EmiNetUtil::addrSetPort(ifAddr, _localPort);
            
            SocketHandle *handle = _delegate.openSocket(address, err);
            if (!handle) {
                return false;
            }
            
            _sockets.push_back(handle);
            
            if (0 == _localPort) {
                _localPort = SockDelegate::extractLocalPort(handle);
                ASSERT(0 != _localPort);
            }
        }
        Binding::freeNetworkInterfaces(ni);
        
        return true;
    }
    
public:
    
    virtual ~EmiUdpSocket() {
        SocketVectorIter iter(_sockets.begin());
        SocketVectorIter  end(_sockets.end());
        
        while (iter != end) {
            SocketHandle* sh(*iter);
            if (sh) {
                SockDelegate::closeSocket(_delegate, sh);
            }
            
            ++iter;
        }
    }
    
    static EmiUdpSocket *open(SockDelegate& delegate, const sockaddr_storage& address, Error& err) {
        EmiUdpSocket *sock = new EmiUdpSocket(delegate);
        
        if (!sock->init(address, err)) {
            goto error;
        }
        
        return sock;
        
    error:
        delete sock;
        return NULL;
    }
    
    void sendData(const sockaddr_storage& address,
                  const uint8_t *data,
                  size_t size) {
        SocketVectorIter iter(_sockets.begin());
        SocketVectorIter  end(_sockets.end());
        
        while (iter != end) {
            SocketHandle* sh(*iter);
            if (sh) {
                _delegate.sendData(sh, address, data, size);
            }
            
            ++iter;
        }
    }
    
    inline uint16_t getLocalPort() const {
        return _localPort;
    }
    
};

#endif
