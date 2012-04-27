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
    typedef std::pair<sockaddr_storage, SocketHandle*> AddrAndSocket;
    typedef std::vector<AddrAndSocket>                 SocketVector;
    typedef typename SocketVector::iterator            SocketVectorIter;
    
    SocketVector _sockets;
    
    EmiUdpSocket() : _sockets() {}
    
    bool init(const sockaddr_storage& address, Error& err) {
        NetworkInterfaces ni;
        
        if (!Binding::getNetworkInterfaces(ni, err)) {
            return false;
        }
        
        const char *ifName;
        sockaddr_storage ifAddr;
        while (Binding::nextNetworkInterface(ni, ifName, ifAddr)) {
            SocketHandle *handle = SockDelegate::openSocket(address, err);
            if (!handle) {
                return false;
            }
            
            _sockets.push_back(std::make_pair(address, handle));
            
            // TODO
            printf("?! %s\n", ifName);
        }
        Binding::freeNetworkInterfaces(ni);
    }
    
public:
    
    virtual ~EmiUdpSocket() {
        SocketVectorIter iter(_sockets.begin());
        SocketVectorIter  end(_sockets.end());
        
        while (iter != end) {
            AddrAndSocket& as(*iter);
            if (as.second) {
                Binding::closeSocket(as.second);
            }
            
            ++iter;
        }
    }
    
    static EmiUdpSocket *open(const sockaddr_storage& address, Error& err) {
        EmiUdpSocket *sock = new EmiUdpSocket();
        
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
            AddrAndSocket& as(*iter);
            if (as.second) {
                SockDelegate::sendData(as.second, address, data, size);
            }
            
            ++iter;
        }
    }
    
};

#endif
