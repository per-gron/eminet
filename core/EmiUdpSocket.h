//
//  EmiUdpSocket.h
//  roshambo
//
//  Created by Per Eckerdal on 2012-04-27.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#ifndef roshambo_EmiUdpSocket_h
#define roshambo_EmiUdpSocket_h

#include "EmiAddressCmp.h"

#include <netinet/in.h>
#include <vector>
#include <utility>

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
    typedef std::pair<sockaddr_storage, SocketHandle*> AddrSocketPair;
    typedef std::vector<AddrSocketPair>                SocketVector;
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
            
            sockaddr_storage localAddr;
            SockDelegate::extractLocalAddress(handle, localAddr);
            
            _sockets.push_back(std::make_pair(localAddr, handle));
            
            if (0 == _localPort) {
                _localPort = EmiNetUtil::addrPortH(localAddr);
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
            SocketHandle* sh((*iter).second);
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
    
    // To send from all sockets, specify a fromAddress with a port number of 0
    void sendData(const sockaddr_storage& fromAddress,
                  const sockaddr_storage& toAddress,
                  const uint8_t *data,
                  size_t size) {
        uint16_t fromAddrPort(EmiNetUtil::addrPortH(fromAddress));
        
        SocketVectorIter iter(_sockets.begin());
        SocketVectorIter  end(_sockets.end());
        while (iter != end) {
            AddrSocketPair &asp(*iter);
            
            if (0 == fromAddrPort ||
                0 == EmiAddressCmp::compare(fromAddress, asp.first)) {
                
                SocketHandle* sh(asp.second);
                if (sh) {
                    _delegate.sendData(sh, toAddress, data, size);
                }
                
            }
            
            ++iter;
        }
    }
    
    inline uint16_t getLocalPort() const {
        return _localPort;
    }
    
};

#endif
