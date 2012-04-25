//
//  EmiP2P.h
//  roshambo
//
//  Created by Per Eckerdal on 2012-04-23.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#ifndef roshambo_EmiP2PSock_h
#define roshambo_EmiP2PSock_h

#include "EmiTypes.h"
#include "EmiP2PSockConfig.h"
#include "EmiP2PConn.h"
#include "EmiMessageHeader.h"
#include "EmiMessage.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <utility>

static const EmiTimeInterval EMI_P2P_COOKIE_RESOLUTION  = 5*60; // In seconds

template<class P2PSockDelegate>
class EmiP2PSock {
    static const uint64_t ARC4RANDOM_MAX = 0x100000000;
    
    typedef typename P2PSockDelegate::Binding  Binding;
    typedef typename Binding::SocketHandle     SocketHandle;
    typedef typename Binding::TemporaryData    TemporaryData;
    typedef typename Binding::Error            Error;
    typedef typename Binding::Address          Address;
    typedef typename Binding::AddressCmp       AddressCmp;
    
    static const size_t          EMI_P2P_SERVER_SECRET_SIZE = 32;
    static const size_t          EMI_P2P_SHARED_SECRET_SIZE = 32;
    static const size_t          EMI_P2P_RAND_NUM_SIZE = 8;
    static const size_t          EMI_P2P_COOKIE_SIZE = EMI_P2P_RAND_NUM_SIZE + Binding::HMAC_HASH_SIZE;
    
    typedef EmiP2PConn<P2PSockDelegate, EmiP2PSock, EMI_P2P_COOKIE_SIZE> Conn;
    
    typedef EmiP2PSockConfig<Address>            SockConfig;
    typedef typename Conn::ConnCookie            ConnCookie;
    typedef std::map<Address, Conn*, AddressCmp> ConnMap;
    typedef typename ConnMap::iterator           ConnMapIter;
    typedef std::map<ConnCookie, Conn*>          ConnCookieMap;
    typedef typename ConnCookieMap::iterator     ConnCookieMapIter;
    
private:
    // Private copy constructor and assignment operator
    inline EmiP2PSock(const EmiP2PSock& other);
    inline EmiP2PSock& operator=(const EmiP2PSock& other);
    
    uint8_t         _serverSecret[EMI_P2P_SERVER_SECRET_SIZE];
    SocketHandle   *_socket;
    P2PSockDelegate _delegate;
    
    // The keys of this map are the Conn*'s peer addresses;
    // each conn has two entries in _conns.
    ConnMap       _conns;
    ConnCookieMap _connCookies;
    
    inline bool shouldArtificiallyDropPacket() const {
        if (0 == config.fabricatedPacketDropRate) return false;
        
        return ((float)arc4random() / ARC4RANDOM_MAX) < config.fabricatedPacketDropRate;
    }
    
    void hashCookie(EmiTimeInterval stamp, const uint8_t *randNum,
                    uint8_t *buf, size_t bufLen, bool minusOne = false) const {
        if (bufLen < Binding::HMAC_HASH_SIZE) {
            Binding::panic();
        }
        
        uint64_t integerStamp = floor(stamp/EMI_P2P_COOKIE_RESOLUTION - (minusOne ? 1 : 0));
        
        uint8_t toBeHashed[EMI_P2P_RAND_NUM_SIZE+sizeof(integerStamp)];
        
        memcpy(toBeHashed, randNum, EMI_P2P_RAND_NUM_SIZE);
        *((uint64_t *)toBeHashed+EMI_P2P_RAND_NUM_SIZE) = integerStamp;
        
        Binding::hmacHash(_serverSecret, sizeof(_serverSecret),
                          toBeHashed, sizeof(toBeHashed),
                          buf, bufLen);
    }
    
    // Returns true if the cookie is valid
    bool checkCookie(EmiTimeInterval stamp,
                     const uint8_t *buf, size_t bufLen) const {
        if (EMI_P2P_COOKIE_SIZE != bufLen) {
            return false;
        }
        
        uint8_t testBuf[Binding::HMAC_HASH_SIZE];
        
        hashCookie(stamp, buf, testBuf, sizeof(testBuf));
        if (0 == memcmp(testBuf, buf+EMI_P2P_RAND_NUM_SIZE, sizeof(testBuf))) {
            return true;
        }
        
        hashCookie(stamp, buf, testBuf, sizeof(testBuf), /*minusOne:*/true);
        if (0 == memcmp(testBuf, buf+EMI_P2P_RAND_NUM_SIZE, sizeof(testBuf))) {
            return true;
        }
        
        return false;
    }
    
    Conn *findConn(const Address& address) {
        ConnMapIter cur = _conns.find(address);
        return _conns.end() == cur ? NULL : (*cur).second;
    }
    
    void gotConnectionOpen(EmiTimeInterval now,
                           const uint8_t *rawData,
                           size_t len,
                           SocketHandle *sock,
                           const Address& address,
                           const uint8_t *cookie,
                           size_t cookieLength) {
        if (!checkCookie(now, cookie, cookieLength)) {
            // Invalid cookie. Ignore packet.
            return;
        }
        
        Conn *conn = findConn(address);
        
        if (!conn) {
            // We did not already have a connection with this address
            // Check to see if we have a connection with this cookie
            ConnCookie cc(cookie, cookieLength);
            
            ConnCookieMapIter cur = _connCookies.find(cc);
            
            if (_connCookies.end() != cur) {
                // There was a connection open with this cookie
                
                conn = (*cur).second;
                // We don't need to save the cookie anymore
                _connCookies.erase(cur);
                
                conn->gotOtherAddress(address);
            }
            else {
                // There was no connection open with this cookie. Open new one
                
                conn = new Conn(*this, cc, sock, address, config.connectionTimeout, config.rateLimit);
                _connCookies.insert(std::make_pair(cc, conn));
            }
            
            _conns.insert(std::make_pair(address, conn));
        }
        
        conn->gotTimestamp(address, now, rawData, len);
        
        // Regardless of whether we had an EmiP2PConn object set up
        // for this address, we want to reply to the host with an
        // acknowledgement that we have received the SYN message.
        conn->sendPrx(address);
    }
    
    inline size_t ipLength(const Address& address) {
        int family = Binding::extractFamily(address);
        if (AF_INET == family) {
            return 4;
        }
        else if (AF_INET6 == family) {
            return 16;
        }
        else {
            ASSERT(0 && "unexpected address family");
            abort();
        }
    }
    
    // conn must not be NULL
    void gotConnectionOpenAck(const Address& address,
                              Conn *conn,
                              EmiTimeInterval now,
                              const uint8_t *rawData,
                              size_t len) {
        size_t ipLen = ipLength(address);
        size_t portLen = 2;
        if (len != Binding::HMAC_HASH_SIZE+ipLen+portLen) {
            // Invalid packet
            return;
        }
        
        uint8_t hashResult[Binding::HMAC_HASH_SIZE];
        
        Binding::hmacHash(conn->cookie.cookie, EMI_P2P_COOKIE_SIZE,
                          rawData+Binding::HMAC_HASH_SIZE, sizeof(ipLen+portLen),
                          hashResult, sizeof(hashResult));
        
        if (0 != memcmp(hashResult, rawData, Binding::HMAC_HASH_SIZE)) {
            // Invalid packet
            return;
        }
        
        conn->gotTimestamp(address, now, rawData, len);
        
        Address innerAddress(Binding::makeAddress(Binding::extractFamily(address),
                                                  rawData+Binding::HMAC_HASH_SIZE, ipLen,
                                                  *((uint16_t *)(rawData+Binding::HMAC_HASH_SIZE+ipLen))));
        
        conn->gotInnerAddress(address, innerAddress);
        
        if (conn->hasBothInnerAddresses()) {
            conn->sendEndpointPair(0);
            conn->sendEndpointPair(1);
        }
    }
    
public:
    
    // conn might be NULL. In that case, this is a no-op
    void removeConnection(Conn *conn) {
        if (conn) {
            _conns.erase(conn->getFirstAddress());
            _conns.erase(conn->getOtherAddress());
            _connCookies.erase(conn->cookie);
            
            delete conn;
        }
    }
    
    const SockConfig config;
    
    EmiP2PSock(const SockConfig& config_, const P2PSockDelegate& delegate) :
    _socket(NULL), _delegate(delegate), config(config_) {
        Binding::randomBytes(_serverSecret, sizeof(_serverSecret));
    }
    virtual ~EmiP2PSock() {
        // This will close the socket
        suspend();
        
        ConnMapIter iter = _conns.begin();
        ConnMapIter end  = _conns.end();
        while (iter != end) {
            delete (*iter).second;
            ++iter;
        }
    }
    
    bool isOpen() const {
        return _socket;
    }
    
    void suspend() {
        if (_socket) {
            P2PSockDelegate::closeSocket(*this, _socket);
            _socket = NULL;
        }
    }
    
    bool desuspend(Error& err) {
        if (!_socket) {
            _socket = _delegate.openSocket(config.port, err);
            if (!_socket) return false;
        }
        
        return true;
    }
    
    // Returns the size of the cookie
    size_t generateCookie(EmiTimeInterval stamp,
                          uint8_t *buf, size_t bufLen) const {
        if (bufLen < EMI_P2P_COOKIE_SIZE) {
            Binding::panic();
        }
        
        Binding::randomBytes(buf, EMI_P2P_RAND_NUM_SIZE);
        
        hashCookie(stamp, /*randNum:*/buf,
                      buf+EMI_P2P_RAND_NUM_SIZE, bufLen-EMI_P2P_RAND_NUM_SIZE);
        
        return EMI_P2P_COOKIE_SIZE;
    }
    
    // Returns the size of the shared secret
    static size_t generateSharedSecret(uint8_t *buf, size_t bufLen) {
        if (bufLen < EMI_P2P_SHARED_SECRET_SIZE) {
            Binding::panic();
        }
        
        Binding::randomBytes(buf, EMI_P2P_SHARED_SECRET_SIZE);
        
        return EMI_P2P_SHARED_SECRET_SIZE;
    }
    
    void onMessage(EmiTimeInterval now,
                   SocketHandle *sock,
                   const Address& address,
                   const TemporaryData& data,
                   size_t offset,
                   size_t len) {
        if (shouldArtificiallyDropPacket()) {
            return;
        }
        
        const char *err = NULL;
        const uint8_t *rawData(Binding::extractData(data)+offset);
        
        Conn *conn = findConn(address);
        if (conn) {
            conn->gotPacket(address);
        }
        
        if (EMI_TIMESTAMP_LENGTH+1 == len) {
            // This is a heartbeat packet. Just forward the packet (if we can)
            if (conn) {
                conn->gotTimestamp(address, now, rawData, len);
                conn->forwardPacket(now, address, data, offset, len);
            }
        }
        else if (len < EMI_TIMESTAMP_LENGTH + EMI_HEADER_LENGTH) {
            err = "Packet too short";
            goto error;
        }
        else {
            EmiMessageHeader header;
            if (!EmiMessageHeader::parseMessageHeader(rawData+EMI_TIMESTAMP_LENGTH,
                                                      len-EMI_TIMESTAMP_LENGTH,
                                                      header)) {
                err = "Invalid message header";
                goto error;
            }
            
            bool isControlMessage = !!(header.flags & (EMI_PRX_FLAG | EMI_RST_FLAG | EMI_SYN_FLAG));
            
            size_t expectedPacketLength = EMI_TIMESTAMP_LENGTH+header.headerLength+header.length;
            bool isTheOnlyMessageInThisPacket = (len == expectedPacketLength);
            
            if (isControlMessage) {
                if (!isTheOnlyMessageInThisPacket) {
                    // This check also ensures that we don't buffer overflow
                    // when we access the message's data part.
                    err = "Invalid message length";
                    goto error;
                }
                
                // EMI_SACK_FLAG counts as a relevant flag because a control
                // message with this flag is an invalid flag, and counting it
                // as relevant makes sure that it is interpreted as an invalid
                // message.
                EmiFlags relevantFlags = (header.flags & (EMI_PRX_FLAG | EMI_RST_FLAG |
                                                          EMI_SYN_FLAG | EMI_ACK_FLAG |
                                                          EMI_SACK_FLAG));
                
                if (EMI_SYN_FLAG == relevantFlags) {
                    // This is a connection open message.
                    gotConnectionOpen(now,
                                      rawData,
                                      len,
                                      sock,
                                      address,
                                      rawData+EMI_TIMESTAMP_LENGTH+header.headerLength,
                                      header.length);
                }
                else if ((EMI_PRX_FLAG | EMI_ACK_FLAG) == relevantFlags) {
                    // This is a connection open ACK message.
                    if (conn) {
                        gotConnectionOpenAck(address, conn, now, rawData, len);
                    }
                    else {
                        err = "Got PRX-ACK message without open conection";
                        goto error;
                    }
                }
                else if (EMI_RST_FLAG == relevantFlags) {
                    // This is a non-proxy connection close message.
                    //
                    // We don't need to be smart about this type of packet,
                    // we can simply forward it.
                    if (conn) {
                        conn->gotTimestamp(address, now, rawData, len);
                        conn->forwardPacket(now, address, data, offset, len);
                    }
                    else {
                        err = "Got RST message without open conection";
                        goto error;
                    }
                }
                else if ((EMI_RST_FLAG | EMI_ACK_FLAG) == relevantFlags) {
                    // This is a non-proxy connection close message.
                    if (conn) {
                        // We still have an open connection. This means that
                        // we haven't yet received confirmation from the other
                        // host that it has received the RST-ACK message, so
                        // we forward it.
                        conn->gotTimestamp(address, now, rawData, len);
                        conn->forwardPacket(now, address, data, offset, len);
                    }
                    else {
                        // We don't have an open connection. This probably means
                        // that the other host has already acknowledged the
                        // connection close and that the host that sent this packet
                        // did not receive the confirmation.
                        //
                        // In this case, we simply respond with a SYN-RST-ACK packet.
                        
                        EmiFlags responseFlags(EMI_SYN_FLAG | EMI_RST_FLAG | EMI_ACK_FLAG);
                        EmiMessage<Binding>::writeControlPacket(responseFlags, ^(uint8_t *buf, size_t size) {
                            P2PSockDelegate::sendData(_socket, address, buf, size);
                        });
                    }
                }
                else if ((EMI_RST_FLAG | EMI_SYN_FLAG | EMI_ACK_FLAG) == relevantFlags) {
                    // This is a non-proxy connection close ack message.
                    if (conn) {
                        // We still have an open connection. This means that
                        // both hosts now know that the connection is closed,
                        // so we can forget about it.
                        //
                        // But before we do so, we want to forward this packet.
                        // (Not forwarding this packet will only force the other
                        // host to resend the RST-ACK message, in which case we
                        // will respond with SYN-RST-ACK anyways, but that is slower
                        // than immediately forwarding this packet)
                        conn->forwardPacket(now, address, data, offset, len);
                        
                        removeConnection(conn);
                    }
                    else {
                        // We don't have an open connection. In this case we don't
                        // need to do anything.
                    }
                }
                else if ((EMI_PRX_FLAG | EMI_RST_FLAG) == relevantFlags) {
                    // This is a proxy connection close message.
                    //
                    // Forget about the connection and respond with a PRX-RST-ACK packet.
                    
                    // Note that conn might very well be NULL, but it doesn't matter
                    removeConnection(conn);
                    
                    EmiFlags responseFlags(EMI_PRX_FLAG | EMI_RST_FLAG | EMI_ACK_FLAG);
                    EmiMessage<Binding>::writeControlPacket(responseFlags, ^(uint8_t *buf, size_t size) {
                        P2PSockDelegate::sendData(_socket, address, buf, size);
                    });
                }
                else {
                    err = "Invalid message flags";
                    goto error;
                }
            }
            else {
                // This is not a control message, so we don't care about its
                // contents. Just forward it.
                if (conn) {
                    conn->gotTimestamp(address, now, rawData, len);
                    conn->forwardPacket(now, address, data, offset, len);
                }
            }
        }
        
        return;
    error:
        
        return;
    }
    
    P2PSockDelegate& getDelegate() {
        return _delegate;
    }
    
    const P2PSockDelegate& getDelegate() const {
        return _delegate;
    }
};

#endif
