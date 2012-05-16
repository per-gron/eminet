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
    
    typedef EmiConnParams<Binding>              ECP;
    typedef EmiConn<SockDelegate, ConnDelegate> EC;
    typedef EmiMessage<Binding>                 EM;
    typedef EmiUdpSocket<Binding>               EUS;
    
    typedef std::map<AddressKey, EC*>              ServerConnectionMap;
    typedef typename ServerConnectionMap::iterator ServerConnectionMapIter;
    
    typedef std::map<EUS*, EC*>                    ClientConnectionMap;
    typedef typename ClientConnectionMap::iterator ClientConnectionMapIter;
    
private:
    // Private copy constructor and assignment operator
    inline EmiSock(const EmiSock& other);
    inline EmiSock& operator=(const EmiSock& other);
    
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
                                bool acceptPacketFromUnexpectedHost = false) {
        if (_serverSocket == sock) {
            ServerConnectionMapIter cur(_serverConns.find(AddressKey(remoteAddr)));
            return _serverConns.end() == cur ? NULL : (*cur).second;
        }
        else {
            ClientConnectionMapIter cur(_clientConns.find(sock));
            if (_clientConns.end() == cur) {
                return NULL;
            }
            
            EC *conn = (*cur).second;
            
            if (!acceptPacketFromUnexpectedHost &&
                0 != EmiAddressCmp::compare(conn->getRemoteAddress(), remoteAddr)) {
                // We only want to accept packets from the correct remote host.
                return NULL;
            }
            
            return conn;
        }
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
        sock->onMessage(now, socket, inboundAddress, remoteAddress, data, offset, len);
    }
    
public:
    const EmiSockConfig  config;
    
    EmiSock(const EmiSockConfig& config_, const SockDelegate& delegate) :
    config(config_), _delegate(delegate), _serverSocket(NULL) {}
    
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
    
    inline bool shouldArtificiallyDropPacket() const {
        if (0 == config.fabricatedPacketDropRate) return false;
        
        return ((float)arc4random() / EmiNetUtil::ARC4RANDOM_MAX) < config.fabricatedPacketDropRate;
    }
    
    void onMessage(EmiTimeInterval now,
                   EUS *sock,
                   const sockaddr_storage& inboundAddress,
                   const sockaddr_storage& remoteAddress,
                   const TemporaryData& data,
                   size_t offset,
                   size_t len) {
        if (shouldArtificiallyDropPacket()) {
            return;
        }
        
        __block const char *err = NULL;
        
        uint16_t inboundPort(EmiNetUtil::addrPortH(inboundAddress));
        
        const uint8_t *rawData(Binding::extractData(data)+offset);
        
        __block EC *conn(getConnectionForMessage(sock, remoteAddress));
        
        if (conn) {
            conn->gotPacket();
        }
        
        if (EMI_TIMESTAMP_LENGTH+1 == len) {
            // This is a heartbeat packet
            if (conn) {
                conn->gotTimestamp(now, rawData, len);
            }
        }
        else if (len < EMI_TIMESTAMP_LENGTH + EMI_HEADER_LENGTH) {
            err = "Packet too short";
            goto error;
        }
        else {
            EmiMessageHeader::EmiParseMessageBlock block =
            ^ bool (const EmiMessageHeader& header, size_t dataOffset) {
                size_t actualRawDataOffset = dataOffset+EMI_TIMESTAMP_LENGTH;
                
#define ENSURE_CONN_VAR(conn, msg)                                  \
                do {                                                \
                    if (!conn) {                                    \
                        err = "Got "msg" message but has no "       \
                              "open connection for that address";   \
                        return false;                               \
                    }                                               \
                } while (0)
#define ENSURE_CONN(msg) ENSURE_CONN_VAR(conn, msg)
#define ENSURE(check, errStr)                   \
                do {                            \
                    if (!(check)) {             \
                        err = errStr;           \
                        return false;           \
                    }                           \
                } while (0)
                
                bool prxFlag  = header.flags & EMI_PRX_FLAG;
                bool synFlag  = header.flags & EMI_SYN_FLAG;
                bool rstFlag  = header.flags & EMI_RST_FLAG;
                bool ackFlag  = header.flags & EMI_ACK_FLAG;
                bool sackFlag = header.flags & EMI_SACK_FLAG;
                
                if (prxFlag) {
                    // This is some kind of proxy/P2P connection message
                    
                    if (!synFlag && !rstFlag && !ackFlag) {
                        ENSURE_CONN("PRX");
                        
                        conn->gotPrx(now);
                    }
                    if (synFlag && rstFlag && ackFlag) {
                        ENSURE_CONN("PRX-RST-SYN-ACK");
                        
                        conn->gotPrxRstSynAck(now, rawData+actualRawDataOffset, header.length);
                    }
                    if (!synFlag && rstFlag && ackFlag) {
                        // We want to accept PRX-RST-ACK packets from hosts other than the
                        // current remote host of the connection.
                        EC *prxConn(getConnectionForMessage(sock,
                                                            remoteAddress,
                                                            /*acceptPacketFromUnexpectedHost:*/true));
                        ENSURE_CONN_VAR(prxConn, "PRX-RST-ACK");
                        
                        prxConn->gotPrxRstAck(remoteAddress);
                    }
                    if (synFlag && !rstFlag && !ackFlag) {
                        // We want to accept PRX-SYN packets from hosts other than the
                        // current remote host of the connection.
                        EC *prxConn(getConnectionForMessage(sock,
                                                            remoteAddress,
                                                            /*acceptPacketFromUnexpectedHost:*/true));
                        ENSURE_CONN_VAR(prxConn, "PRX-SYN");
                        
                        prxConn->gotPrxSyn(remoteAddress, rawData+actualRawDataOffset, header.length);
                    }
                    if (synFlag && !rstFlag && ackFlag) {
                        // We want to accept PRX-SYN-ACK packets from hosts other than the
                        // current remote host of the connection.
                        EC *prxConn(getConnectionForMessage(sock,
                                                            remoteAddress,
                                                            /*acceptPacketFromUnexpectedHost:*/true));
                        ENSURE_CONN_VAR(prxConn, "PRX-SYN-ACK");
                        
                        prxConn->gotPrxSynAck(remoteAddress, rawData+actualRawDataOffset, header.length);
                    }
                    else {
                        err = "Invalid message flags";
                        return false;
                    }
                }
                else if (synFlag && !rstFlag) {
                    // This is an initiate connection message
                    
                    ENSURE(config.acceptConnections,
                           "Got SYN but this socket doesn't \
                            accept incoming connections");
                    ENSURE(0 == header.length,
                           "Got SYN message with message length != 0");
                    ENSURE(!ackFlag, "Got SYN message with ACK flag");
                    ENSURE(!sackFlag, "Got SYN message with SACK flag");
                    
                    if (conn && conn->isOpen() && conn->getOtherHostInitialSequenceNumber() != header.sequenceNumber) {
                        // The connection is already open, and we get a SYN message with a
                        // different initial sequence number. This probably means that the
                        // other host has forgot about the connection we have open. Force
                        // close it and continue as if conn did not exist.
                        conn->forceClose();
                        conn = NULL;
                    }
                    
                    if (!conn) {
                        conn = _delegate.makeConnection(ECP(sock, remoteAddress, inboundPort));
                        ASSERT(0 == _serverConns.count(AddressKey(remoteAddress)));
                        _serverConns.insert(std::make_pair(AddressKey(remoteAddress), conn));
                    }
                    
                    conn->gotTimestamp(now, rawData, len);
                    
                    if (conn->opened(inboundAddress, now, header.sequenceNumber)) {
                        _delegate.gotConnection(*conn);
                    }
                }
                else if (synFlag && rstFlag) {
                    if (ackFlag) {
                        // This is a close connection ack message
                        
                        ENSURE(!sackFlag, "Got SYN-RST-ACK message with SACK flag");
                        ENSURE(conn,
                               "Got SYN-RST-ACK message but has no open \
                                connection for that address. Ignoring the \
                                packet. (This is not really an error \
                                condition, it is part of normal operation \
                                of the protocol.)");
                        
                        // With this packet type, we do not invoke gotTimestamp:
                        // on the connection object, because the timestamps might be bogus
                        // (since the other host might have forgot about the connection
                        // and thus the data required to send proper timestamps)
                        
                        conn->gotSynRstAck();
                        conn = NULL;
                    }
                    else {
                        // This is a connection initiated message
                        
                        ENSURE(!sackFlag, "Got SYN-RST message with SACK flag");
                        ENSURE_CONN("SYN-RST");
                        ENSURE(conn->isOpening(), "Got SYN-RST message for open connection");
                        
                        conn->gotTimestamp(now, rawData, len);
                        if (!conn->gotSynRst(now, inboundAddress, header.sequenceNumber)) {
                            err = "Failed to process SYN-RST message";
                            return false;
                        }
                    }
                }
                else if (!synFlag && rstFlag) {
                    // This is a close connection message
                    
                    ENSURE(!ackFlag, "Got RST message with ACK flag");
                    ENSURE(!sackFlag, "Got RST message with SACK flag");
                    
                    // Regardless of whether we still have a connection up,
                    // respond with a SYN-RST-ACK message.
                    EM::writeControlPacket(EMI_SYN_FLAG | EMI_RST_FLAG | EMI_ACK_FLAG, ^(uint8_t *buf, size_t size) {
                        sock->sendData(inboundAddress, remoteAddress, buf, size);
                    });
                    
                    // Note that this has to be done after we send the control
                    // packet, since invoking gotRst might deallocate the sock
                    // object.
                    //
                    // TODO: Closing the socket here might be the wrong thing
                    // to do. It might be better to let it stay alive for a full
                    // connection timeout cycle, just to make sure that the other
                    // host gets our SYN-RST-ACK response.
                    if (conn) {
                        conn->gotTimestamp(now, rawData, len);
                        conn->gotRst();
                        conn = NULL;
                    }
                }
                else if (!synFlag && !rstFlag) {
                    // This is a data message
                    ENSURE_CONN("data");
                    
                    conn->gotTimestamp(now, rawData, len);
                    conn->gotMessage(now, header, data, offset+actualRawDataOffset, /*dontFlush:*/false);
                    
                    // gotMessage might have invoked third-party code, which might have
                    // forceClose-d the connection, which might have deallocated conn.
                    // 
                    // To be safe, re-load conn from _conns; if conn is closed, this
                    // will set conn to NULL, which is correct, because we don't want
                    // to give any additional data to it anyway, even if this packet
                    // contains more messages.
                    conn = getConnectionForMessage(sock, remoteAddress);
                }
                else {
                    err = "Invalid message flags";
                    return false;
                }
                
                return true;
            };
            
            if (!EmiMessageHeader::parseMessages(rawData+EMI_TIMESTAMP_LENGTH,
                                                 len-EMI_TIMESTAMP_LENGTH,
                                                 block)) {
                goto error;
            }
        }
        
        return;
    error:
        
        return;
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
            
            conn->closeSocket();
        }
        else {
            _serverConns.erase(AddressKey(conn->getRemoteAddress()));
        }
    }
};

#endif
