//
//  EmiSock.h
//  roshambo
//
//  Created by Per Eckerdal on 2012-04-11.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#ifndef emilir_EmiSock_h
#define emilir_EmiSock_h

#include "EmiMessageHeader.h"
#include "EmiSendQueue.h"
#include "EmiSockConfig.h"
#include "EmiConnParams.h"
#include "EmiAddressCmp.h"
#include "EmiUdpSocket.h"
#include "EmiNetUtil.h"
#include "EmiMessageHandler.h"

#include <map>
#include <set>
#include <cstdlib>
#include <netinet/in.h>

template<class SockDelegate, class ConnDelegate>
class EmiSock {
    typedef typename SockDelegate::Binding     Binding;
    typedef typename Binding::Error            Error;
    typedef typename Binding::TemporaryData    TemporaryData;
    typedef typename Binding::SocketHandle     SocketHandle;
    typedef typename SockDelegate::ConnectionOpenedCallbackCookie  ConnectionOpenedCallbackCookie;
    
    struct AddressKey {
    public:
        explicit AddressKey(const sockaddr_storage& address_) :
        address(address_) {}
        
        sockaddr_storage address;
        
        inline bool operator<(const AddressKey& rhs) const {
            return 0 > EmiAddressCmp::compare(address, rhs.address);
        }
    };
    
    typedef EmiConnParams<Binding>                  ECP;
    typedef EmiConn<SockDelegate, ConnDelegate>     EC;
    typedef EmiMessage<Binding>                     EM;
    typedef EmiUdpSocket<Binding>                   EUS;
    typedef EmiMessageHandler<EC, EmiSock, Binding> EMH;
    
    typedef std::map<AddressKey, EC*>              ServerConnectionMap;
    typedef typename ServerConnectionMap::iterator ServerConnectionMapIter;
    
    typedef std::map<EUS*, EC*>                    ClientConnectionMap;
    typedef typename ClientConnectionMap::iterator ClientConnectionMapIter;
    
    friend class EmiMessageHandler<EC, EmiSock, Binding>;
    
private:
    // Private copy constructor and assignment operator
    inline EmiSock(const EmiSock& other);
    inline EmiSock& operator=(const EmiSock& other);
    
    EMH                   _messageHandler;
    EUS                  *_serverSocket;
    ServerConnectionMap   _serverConns;
    ClientConnectionMap   _clientConns;
    SockDelegate          _delegate;
    
    // SockDelegate::connectionOpened will be called on the cookie iff this function returns true.
    bool connectHelper(EmiTimeInterval now, const sockaddr_storage& remoteAddress,
                       const uint8_t *p2pCookie, size_t p2pCookieLength,
                       const uint8_t *sharedSecret, size_t sharedSecretLength,
                       const ConnectionOpenedCallbackCookie& callbackCookie, Error& err) {
        sockaddr_storage ss(config.address);
        EmiNetUtil::addrSetPort(ss, 0); // Bind to a random free port number
        
        EUS *socket = EUS::open(_delegate.getSocketCookie(), onMessage, this, ss, err);
        
        if (!socket) {
            return false;
        }
        
        uint16_t inboundPort = socket->getLocalPort();
        
        if (!inboundPort) {
            return false;
        }
        
        EC *ec(_delegate.makeConnection(ECP(socket, remoteAddress, inboundPort, 
                                            p2pCookie, p2pCookieLength,
                                            sharedSecret, sharedSecretLength)));
        _clientConns.insert(std::make_pair(socket, ec));
        ec->open(now, callbackCookie);
        
        return true;
    }
    
    EC *getConnectionForMessage(EUS *sock,
                                const sockaddr_storage& remoteAddr,
                                bool *unexpectedRemoteHost) {
        *unexpectedRemoteHost = false;
        
        EC *result = NULL;
        
        if (_serverSocket == sock) {
            ServerConnectionMapIter cur(_serverConns.find(AddressKey(remoteAddr)));
            
            result = (_serverConns.end() == cur ? NULL : (*cur).second);
        }
        else {
            ClientConnectionMapIter cur(_clientConns.find(sock));
            if (_clientConns.end() != cur) {
                result = (*cur).second;
                
                *unexpectedRemoteHost = (0 != EmiAddressCmp::compare(result->getRemoteAddress(), remoteAddr));
            }
        }
        
        return result;
    }
    
    static void onMessage(EUS *socket,
                          void *userData,
                          EmiTimeInterval now,
                          const sockaddr_storage& inboundAddress,
                          const sockaddr_storage& remoteAddress,
                          const TemporaryData& data,
                          size_t offset,
                          size_t len) {
        EmiSock *sock((EmiSock *)userData);
        
        if (sock->shouldArtificiallyDropPacket()) {
            return;
        }
        
        bool unexpectedRemoteHost;
        EC *conn = sock->getConnectionForMessage(socket, remoteAddress, &unexpectedRemoteHost);
        
        sock->_messageHandler.onMessage(sock->config.acceptConnections,
                                        now, socket,
                                        unexpectedRemoteHost, conn,
                                        inboundAddress, remoteAddress,
                                        data, offset, len);
    }
            
    EC *makeServerConnection(const sockaddr_storage& remoteAddress, uint16_t inboundPort) {
        EC *conn = _delegate.makeConnection(ECP(_serverSocket, remoteAddress, inboundPort));
        ASSERT(0 == _serverConns.count(AddressKey(remoteAddress)));
        _serverConns.insert(std::make_pair(AddressKey(remoteAddress), conn));
        _delegate.gotConnection(*conn);
        
        return conn;
    }
    
    inline bool shouldArtificiallyDropPacket() const {
        if (0 == config.fabricatedPacketDropRate) return false;
        
        return ((float)arc4random() / EmiNetUtil::ARC4RANDOM_MAX) < config.fabricatedPacketDropRate;
    }
    
public:
    const EmiSockConfig config;
    
    EmiSock(const EmiSockConfig& config_, const SockDelegate& delegate) :
    config(config_),
    _messageHandler(*this),
    _delegate(delegate),
    _serverSocket(NULL) {}
    
    virtual ~EmiSock() {
        /// EmiSock should not be deleted before all open connections are closed,
        /// but just to be sure, we close all remaining connections.
        
#define X(map, Iter)                                                                  \
        do {                                                                          \
            size_t numConns = map.size();                                             \
            Iter iter = map.begin();                                                  \
            Iter end  = map.end();                                                    \
            while (iter != end) {                                                     \
                /* This will remove the connection from _conns */                     \
                (*iter).second->forceClose();                                         \
                                                                                      \
                /* We do this check to make sure we don't enter an infinite loop. */  \
                /* It shouldn't be required.                                      */  \
                size_t newNumConns = map.size();                                      \
                ASSERT(newNumConns < numConns);                                       \
                numConns = newNumConns;                                               \
                                                                                      \
                /* We can't increment iter, it has been   */                          \
                /* invalidated because the connection was */                          \
                /* removed from _conns                    */                          \
                iter = map.begin();                                                   \
            }                                                                         \
        } while (0);
        
        /// Close all client connections
        X(_clientConns, ClientConnectionMapIter);
        /// Close all server connections
        X(_serverConns, ServerConnectionMapIter);
        
#undef X
        
        /// Close the server socket (all client sockets will be closed by now)
        if (_serverSocket) {
            delete _serverSocket;
        }
    }
    
    SockDelegate& getDelegate() {
        return _delegate;
    }
    
    const SockDelegate& getDelegate() const {
        return _delegate;
    }
    
    bool open(Error& err) {
        if (!_serverSocket && config.acceptConnections) {
            sockaddr_storage ss(config.address);
            EmiNetUtil::addrSetPort(ss, config.port);
            
            _serverSocket = EUS::open(_delegate.getSocketCookie(), onMessage, this, ss, err);
            
            if (!_serverSocket) {
                return false;
            }
        }
        
        return true;
    }
        
    // SockDelegate::connectionOpened will be called on the cookie iff this function returns true.
    bool connect(EmiTimeInterval now,
                 const sockaddr_storage& remoteAddress,
                 const ConnectionOpenedCallbackCookie& callbackCookie,
                 Error& err) {
        return connectHelper(now, remoteAddress,
                             /*p2pCookie:*/NULL, /*p2pCookieLength:*/0,
                             /*sharedSecret:*/NULL, /*sharedSecretLength:*/0,
                             callbackCookie, err);
    }
    
    // SockDelegate::connectionOpened will be called on the cookie iff this function returns true.
    bool connectP2P(EmiTimeInterval now, const sockaddr_storage& remoteAddress,
                    const uint8_t *p2pCookie, size_t p2pCookieLength,
                    const uint8_t *sharedSecret, size_t sharedSecretLength,
                    const ConnectionOpenedCallbackCookie& callbackCookie, Error& err) {
        return connectHelper(now, remoteAddress,
                             p2pCookie, p2pCookieLength,
                             sharedSecret, sharedSecretLength,
                             callbackCookie, err);
    }
    
    void deregisterConnection(EC *conn) {
        if (EMI_CONNECTION_TYPE_SERVER != conn->getType()) {
            EUS *socket = conn->getSocket();
            
            _clientConns.erase(socket);
        }
        else {
            _serverConns.erase(AddressKey(conn->getRemoteAddress()));
        }
    }
};

#endif
