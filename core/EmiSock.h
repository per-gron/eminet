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
    
    // For makeServerConnection
    friend class EmiMessageHandler<EC, EmiSock, Binding>;
    
private:
    // Private copy constructor and assignment operator
    inline EmiSock(const EmiSock& other);
    inline EmiSock& operator=(const EmiSock& other);
    
    EMH                   _messageHandler;
    EUS                  *_serverSocket;
    ServerConnectionMap   _serverConns;
    SockDelegate          _delegate;
    
    // SockDelegate::connectionOpened will be called on the cookie iff this function returns true.
    bool connectHelper(EmiTimeInterval now, const sockaddr_storage& remoteAddress,
                       const uint8_t *p2pCookie, size_t p2pCookieLength,
                       const uint8_t *sharedSecret, size_t sharedSecretLength,
                       const ConnectionOpenedCallbackCookie& callbackCookie, Error& err) {
        sockaddr_storage bindAddress(config.address);
        EmiNetUtil::addrSetPort(bindAddress, 0); // Bind to a random free port number
        
        EC *ec(_delegate.makeConnection(ECP(remoteAddress,
                                            p2pCookie, p2pCookieLength,
                                            sharedSecret, sharedSecretLength)));
        if (!ec->open(now, _delegate.getSocketCookie(), bindAddress, callbackCookie, err)) {
            ec->forceClose();
            return false;
        }
        
        return true;
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
        
        ASSERT(sock->_serverSocket == socket);
        
        ServerConnectionMapIter cur(sock->_serverConns.find(AddressKey(remoteAddress)));
        EC *conn = (sock->_serverConns.end() == cur ? NULL : (*cur).second);
        
        sock->_messageHandler.onMessage(sock->config.acceptConnections,
                                        now, socket,
                                        /*unexpectedRemoteHost:*/false, conn,
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
        
        do {
            size_t numConns = _serverConns.size();
            ServerConnectionMapIter iter = _serverConns.begin();
            ServerConnectionMapIter end  = _serverConns.end();
            while (iter != end) {
                // This will remove the connection from _conns
                (*iter).second->forceClose();
                
                // We do this check to make sure we don't enter an infinite loop.
                // It shouldn't be required.
                size_t newNumConns = _serverConns.size();
                ASSERT(newNumConns < numConns);
                numConns = newNumConns;
                
                // We can't increment iter, it has been
                // invalidated because the connection was
                // removed from _conns
                iter = _serverConns.begin();
            }
        } while (0);
        
        /// Close the server socket
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
    
    void deregisterServerConnection(EC *conn) {
        ASSERT(EMI_CONNECTION_TYPE_SERVER == conn->getType());
        
        _serverConns.erase(AddressKey(conn->getRemoteAddress()));
    }
};

#endif
