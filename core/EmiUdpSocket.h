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
template<class Binding>
class EmiUdpSocket {
private:
    // Private copy constructor and assignment operator
    inline EmiUdpSocket(const EmiUdpSocket& other);
    inline EmiUdpSocket& operator=(const EmiUdpSocket& other);
    
    typedef typename Binding::TemporaryData            TemporaryData;
    typedef typename Binding::NetworkInterfaces        NetworkInterfaces;
    typedef typename Binding::Error                    Error;
    typedef typename Binding::SocketHandle             SocketHandle;
    typedef std::pair<sockaddr_storage, SocketHandle*> AddrSocketPair;
    typedef std::vector<AddrSocketPair>                SocketVector;
    typedef typename SocketVector::iterator            SocketVectorIter;
    
    typedef void (OnMessage)(EmiUdpSocket *socket,
                             void *userData,
                             EmiTimeInterval now,
                             const sockaddr_storage& inboundAddress,
                             const sockaddr_storage& remoteAddress,
                             const TemporaryData& data,
                             size_t offset,
                             size_t len);
    
    SocketVector  _sockets;
    uint16_t      _localPort;
    OnMessage    *_callback;
    void         *_userData;
    
    EmiUdpSocket(OnMessage *callback, void *userData) :
    _sockets(),
    _localPort(0),
    _callback(callback),
    _userData(userData) {}
    
    static void onMessage(SocketHandle *sock,
                          void *userData,
                          EmiTimeInterval now,
                          const sockaddr_storage& remoteAddress,
                          const TemporaryData& data,
                          size_t offset,
                          size_t len) {
        EmiUdpSocket *eus((EmiUdpSocket *)userData);
        
        sockaddr_storage inboundAddress;
        Binding::extractLocalAddress(sock, inboundAddress);
        
        eus->_callback(eus, eus->_userData, now, inboundAddress, remoteAddress, data, offset, len);
    }
    
    template<class SocketCookie>
    bool init(const SocketCookie& socketCookie, const sockaddr_storage& address, Error& err) {
        // TODO Do something more with address than just use
        // its address family; we should probably only bind
        // address unless address is 0.0.0.0.
        
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
            
            // TODO Should we really bind to address here??
            SocketHandle *handle = Binding::openSocket(socketCookie, onMessage, this, address, err);
            if (!handle) {
                return false;
            }
            
            sockaddr_storage localAddr;
            Binding::extractLocalAddress(handle, localAddr);
            
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
                Binding::closeSocket(sh);
            }
            
            ++iter;
        }
    }
    
    template<class SocketCookie>
    static EmiUdpSocket *open(const SocketCookie& socketCookie,
                              OnMessage *callback,
                              void *userData,
                              const sockaddr_storage& address,
                              Error& err) {
        EmiUdpSocket *sock = new EmiUdpSocket(callback, userData);
        
        if (!sock->init(socketCookie, address, err)) {
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
                    Binding::sendData(sh, toAddress, data, size);
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
