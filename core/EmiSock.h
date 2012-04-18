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
#include "EmiConnDelegate.h"

#include <map>
#include <set>

template<class Address>
class EmiSockConfig {
public:
    EmiSockConfig() :
    mtu(EMI_DEFAULT_MTU),
    heartbeatFrequency(EMI_DEFAULT_HEARTBEAT_FREQUENCY),
    tickFrequency(EMI_DEFAULT_TICK_FREQUENCY),
    connectionTimeout(EMI_DEFAULT_CONNECTION_TIMEOUT),
    heartbeatsBeforeConnectionWarning(EMI_DEFAULT_HEARTBEATS_BEFORE_CONNECTION_WARNING),
    receiverBufferSize(EMI_DEFAULT_RECEIVER_BUFFER_SIZE),
    senderBufferSize(EMI_DEFAULT_SENDER_BUFFER_SIZE),
    acceptConnections(false),
    port(0),
    address(NULL) {}
    
    size_t mtu;
    float heartbeatFrequency;
    float tickFrequency;
    EmiTimeInterval connectionTimeout;
    float heartbeatsBeforeConnectionWarning;
    size_t receiverBufferSize;
    size_t senderBufferSize;
    bool acceptConnections;
    uint16_t port;
    Address address;
};

template<class SockDelegate, class ConnDelegate>
class EmiSock {
    typedef typename SockDelegate::Error            Error;
    typedef typename SockDelegate::Data             Data;
    typedef typename SockDelegate::Address          Address;
    typedef typename SockDelegate::AddressCmp       AddressCmp;
    typedef typename SockDelegate::SocketHandle     SocketHandle;
    
    class EmiConnectionKey {
        const AddressCmp _cmp;
    public:
        EmiConnectionKey(const Address& address_, uint16_t inboundPort_) :
        address(address_), inboundPort(inboundPort_), _cmp() {}
        EmiConnectionKey(const Address& address_, uint16_t inboundPort_, const AddressCmp& cmp) :
        address(address_), inboundPort(inboundPort_), _cmp(cmp) {}
        
        Address address;
        uint16_t inboundPort;
        
        inline bool operator<(const EmiConnectionKey& rhs) const {
            if (inboundPort < rhs.inboundPort) return true;
            else if (inboundPort > rhs.inboundPort) return false;
            else {
                return 0 > _cmp(address, rhs.address);
            }
        }
    };
    
    struct EmiClientSocketKey {
        const AddressCmp _cmp;
    public:
        EmiClientSocketKey(const Address& address_) :
        address(address_), _cmp() {}
        EmiClientSocketKey(const Address& address_, const AddressCmp& cmp) :
        address(address_), _cmp(cmp) {}
        
        Address address;
        
        inline bool operator<(const EmiClientSocketKey& rhs) const {
            return 0 > _cmp(address, rhs.address);
        }
    };
    
    struct EmiClientSocket {
        EmiClientSocket(EmiSock *emiSock_, uint16_t port_, SocketHandle *socket_) :
        emiSock(emiSock_), port(port_), socket(socket_) {}
        
        EmiSock *emiSock;
        uint16_t port;
        SocketHandle *socket;
        std::set<EmiClientSocketKey> addresses;
        
        bool open(Error& err) {
            if (!socket) {
                socket = emiSock->_delegate.openSocket(port, err);
            }
            
            return !!socket;
        }
        
        void close() {
            SockDelegate::closeSocket(socket);
            socket = NULL;
        }
    };
    
    typedef EmiConn<SockDelegate, ConnDelegate> EC;
    typedef void (^EmiConnectionOpenedBlock)(const Error& err, EC& connection);
    typedef EmiSendQueue<SockDelegate, ConnDelegate> ESQ;
    
    typedef std::map<EmiConnectionKey, EC*> EmiConnectionMap;
    typedef std::map<uint16_t, EmiClientSocket> EmiClientSocketMap;
    
    
protected:
    SocketHandle                 *_serverSocket;
    EmiConnectionMap              _conns;
    EmiClientSocketMap            _clientSockets;
    SockDelegate                  _delegate;
    
    int32_t findFreeClientPort(const Address& address) {
        EmiClientSocketKey key(address);
        
        typename EmiClientSocketMap::iterator iter = _clientSockets.begin();
        typename EmiClientSocketMap::iterator end = _clientSockets.end();
        while (iter != end) {
            if (0 == (*iter).second.addresses.count(key)) {
                return (*iter).first;
            }
            
            ++iter;
        }
        
        return -1;
    }
    
    uint16_t openClientSocket(const Address& address, Error& err) {
        // Ensure that the datagram sockets are open
        if (!desuspend(err)) {
            return 0;
        }
        
        int32_t inboundPort = findFreeClientPort(address);
        if (-1 == inboundPort) {
            SocketHandle *socket = _delegate.openSocket(0, err);
            if (!socket) {
                return 0;
            }
            inboundPort = [socket localPort];
            
            _clientSockets.insert(typename EmiClientSocketMap::value_type(inboundPort, EmiClientSocket(this, inboundPort, socket)));
        }
        
        (*(_clientSockets.find(inboundPort))).second.addresses.insert(EmiClientSocketKey(address));
        
        return inboundPort;
    }
    
    void suspendIfInactive() {
        if (!config.acceptConnections && _clientSockets.empty()) {
            suspend();
        }
    }

    
public:
    const EmiSockConfig<Address>  config;
    
    EmiSock(const EmiSockConfig<Address>& config_, const SockDelegate& delegate, Error& err) :
    config(config_), _delegate(delegate) {
        desuspend(err);
    }
    
    virtual ~EmiSock() {
        // EmiSock should not be deleted before all open connections are closed,
        // but just to be sure, we close all remaining connections.
        typename EmiConnectionMap::iterator iter = _conns.begin();
        typename EmiConnectionMap::iterator end  = _conns.end();
        while (iter != end) {
            (*iter).second->forceClose();
            _conns.erase(iter);
            ++iter;
        }
        
        // This will close (which, depending on the binding, might mean deallocate)
        // all sockets. (By now all sockets except possibly the server should be closed
        // already.)
        suspend();
    }
    
    SockDelegate& getDelegate() {
        return _delegate;
    }
    
    const SockDelegate& getDelegate() const {
        return _delegate;
    }
    void setDelegate(const SockDelegate& delegate) {
        _delegate = delegate;
    }
    
    bool isOpen() const {
        return _serverSocket || !_clientSockets.empty();
    }
    
    void suspend() {
        if (isOpen()) {
            if (_serverSocket) {
                SockDelegate::closeSocket(_serverSocket);
                _serverSocket = NULL;
            }
            
            typename EmiClientSocketMap::iterator iter = _clientSockets.begin();
            typename EmiClientSocketMap::iterator end = _clientSockets.end();
            while (iter != end) {
                (*iter).second.close();
                ++iter;
            }
        }
    }
    
    bool desuspend(Error& err) {
        if (!isOpen()) {
            if (config.acceptConnections || !_conns.empty()) {
                _serverSocket = _delegate.openSocket(config.port, err);
                if (!_serverSocket) return false;
            }
            
            typename EmiClientSocketMap::iterator iter = _clientSockets.begin();
            typename EmiClientSocketMap::iterator end = _clientSockets.end();
            while (iter != end) {
                if (!(*iter).second.open(err)) {
                    return false;
                }
                ++iter;
            }
        }
        
        return YES;
    }
    
    void onMessage(EmiTimeInterval now, SocketHandle *sock, uint16_t inboundPort, const Address& address, Data data) {
        __block const char *err = NULL;
        
        size_t len = SockDelegate::extractLength(data);
        
        EmiConnectionKey ckey(address, inboundPort);
        typename EmiConnectionMap::iterator cur = _conns.find(ckey);
        __block EC *conn = _conns.end() == cur ? NULL : (*cur).second;
        
        if (conn) conn->gotPacket();
        
        if (EMI_TIMESTAMP_LENGTH+1 == len) {
            if (conn) {
                conn->gotTimestamp(now, data);
                conn->gotHeartbeat(!!(SockDelegate::extractData(data)[EMI_TIMESTAMP_LENGTH]));
            }
        }
        else if (len < EMI_TIMESTAMP_LENGTH + EMI_HEADER_LENGTH) {
            err = "Packet too short";
            goto error;
        }
        else {
            EmiParseMessageBlock block =
            ^ bool (EmiMessageHeader *header, size_t dataOffset) {
                bool synFlag  = header->flags & EMI_SYN_FLAG;
                bool rstFlag  = header->flags & EMI_RST_FLAG;
                bool ackFlag  = header->flags & EMI_ACK_FLAG;
                bool sackFlag = header->flags & EMI_SACK_FLAG;
                
                if (synFlag && !rstFlag) {
                    // This is an initiate connection message
                    
                    if (!config.acceptConnections) {
                        err = "Got SYN but this socket doesn't accept incoming connections";
                        return false;
                    }
                    if (0 != header->length) {
                        err = "Got SYN message with message length != 0";
                        return false;
                    }
                    if (ackFlag) {
                        err = "Got SYN message with ACK flag";
                        return false;
                    }
                    if (sackFlag) {
                        err = "Got SYN message with SACK flag";
                        return false;
                    }
                    
                    if (conn && conn->isOpen() && conn->getOtherHostInitialSequenceNumber() != header->sequenceNumber) {
                        // The connection is already open, and we get a SYN message with a
                        // different initial sequence number. This probably means that the
                        // other host has forgot about the connection we have open. Force
                        // close it and continue as if conn did not exist.
                        conn->forceClose();
                        conn = NULL;
                    }
                    
                    if (!conn) {
                        conn = _delegate.makeConnection(address, inboundPort, /*initiator:*/false);
                        _conns.insert(std::make_pair(ckey, conn));
                    }
                    
                    conn->gotTimestamp(now, data);
                    
                    if (conn->open(now, header->sequenceNumber)) {
                        _delegate.gotConnection(conn);
                    }
                }
                else if (synFlag && rstFlag) {
                    if (ackFlag) {
                        // This is a close connection ack message
                        
                        if (sackFlag) {
                            err = "Got SYN-RST-ACK message with SACK flag";
                            return false;
                        }
                        if (!conn) {
                            err = "Got SYN-RST-ACK message but has no open \
                            connection for that address. Ignoring the \
                            packet. (This is not really an error \
                            condition, it is part of normal operation \
                            of the protocol.)";
                            return false;
                        }
                        
                        // With this packet type, we do not invoke gotTimestamp:
                        // on the connection object, because the timestamps might be bogus
                        // (since the other host might have forgot about the connection
                        // and thus the data required to send proper timestamps)
                        
                        conn->gotSynRstAck();
                    }
                    else {
                        // This is a connection initiated message
                        
                        if (sackFlag) {
                            err = "Got SYN-RST message with SACK flag";
                            return false;
                        }
                        if (!conn) {
                            err = "Got SYN-RST message but has no open connection for that address";
                            return false;
                        }
                        if (!conn->isOpening()) {
                            err = "Got SYN-RST message for open connection";
                            return false;
                        }
                        
                        conn->gotTimestamp(now, data);
                        if (!conn->gotSynRst(header->sequenceNumber)) {
                            err = "Failed to process SYN-RST message";
                            return false;
                        }
                    }
                }
                else if (!synFlag && rstFlag) {
                    // This is a close connection message
                    
                    if (ackFlag) {
                        err = "Got RST message with ACK flag";
                        return false;
                    }
                    if (sackFlag) {
                        err = "Got RST message with SACK flag";
                        return false;
                    }
                    
                    if (conn) {
                        conn->gotTimestamp(now, data);
                        conn->gotRst();
                    }
                    
                    // Regardless of whether we still have a connection up, respond with a SYN-RST-ACK message
                    ESQ::sendSynRstAckPacket(^(uint8_t *buf, size_t size) {
                        _delegate.sendData(sock, address, buf, size);
                    });
                }
                else if (!synFlag && !rstFlag) {
                    // This is a data message
                    
                    if (!conn) {
                        err = "Got data message but has no open connection for that address";
                        return false;
                    }
                    
                    conn->gotTimestamp(now, data);
                    conn->gotMessage(header, data, dataOffset+EMI_TIMESTAMP_LENGTH, /*dontFlush:*/false);
                }
                else {
                    err = "Invalid message flags";
                    return false;
                }
                
                return true;
            };
            
            if (!EmiMessageHeader::parseMessages(SockDelegate::extractData(data)+EMI_TIMESTAMP_LENGTH,
                                                 SockDelegate::extractLength(data)-EMI_TIMESTAMP_LENGTH,
                                                 block)) {
                goto error;
            }
        }
        
        return;
    error:
        
        return;
    }
    
    bool connect(EmiTimeInterval now, const Address& address, EmiConnectionOpenedBlock block, Error& err) {
        uint16_t inboundPort = openClientSocket(address, err);
        
        if (!inboundPort) {
            return false;
        }
        
        EmiConnectionKey key(address, inboundPort);
        if (0 != _conns.count(key)) {
            // This should not happen, because openClientSocket should
            // have returned an unused port number.
            err = SockDelegate::makeError("com.emilir.eminet.internalerror", 0);
            return false;
        }
        
        
        EC *ec(_delegate.makeConnection(address, inboundPort, /*initiator:*/true));
        _conns.insert(std::make_pair(key, ec));
        ec->open(now, block);
        
        return true;
    }
    
    void sendDatagram(EC *conn, const uint8_t *data, size_t size) {
        SocketHandle *socket = NULL;
        
        if (conn->isInitiator()) {
            typename EmiClientSocketMap::iterator cur = _clientSockets.find(conn->getInboundPort());
            socket = _clientSockets.end() == cur ? NULL : (*cur).second.socket;
        }
        else {
            socket = _serverSocket;
        }
        
        // I'm not 100% sure that socket will never be null
        if (socket) {
            _delegate.sendData(socket, conn->getAddress(), data, size);
        }
    }
    
    void deregisterConnection(EC *conn) {
        const Address& address = conn->getAddress();
        uint16_t inboundPort = conn->getInboundPort();
        
        typename EmiClientSocketMap::iterator cur = _clientSockets.find(inboundPort);
        if (_clientSockets.end() != cur) {
            EmiClientSocket& cs((*cur).second);
            cs.addresses.erase(EmiClientSocketKey(address));
            
            if (cs.addresses.empty()) {
                cs.close();
                _clientSockets.erase(inboundPort);
            }
        }
        
        _conns.erase(EmiConnectionKey(address, inboundPort));
        
        suspendIfInactive();
    }
};

#endif
