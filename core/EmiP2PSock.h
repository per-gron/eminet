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
    
    class ConnCookie {
    public:
        uint8_t cookie[EMI_P2P_COOKIE_SIZE];
        
        ConnCookie(const uint8_t *cookie_, size_t cookieLength) {
            ASSERT(EMI_P2P_COOKIE_SIZE == cookieLength);
            memcpy(cookie, cookie_, sizeof(cookie));
        }
        
        inline ConnCookie(const ConnCookie& other) {
            memcpy(cookie, other.cookie, sizeof(cookie));
        }
        inline ConnCookie& operator=(const ConnCookie& other) {
            memcpy(cookie, other.cookie, sizeof(cookie));
        }
        
        inline bool operator<(const ConnCookie& rhs) const {
            return 0 > memcmp(cookie, rhs.cookie, sizeof(cookie));
        }
    };
    
    typedef EmiP2PSockConfig<Address>                        SockConfig;
    typedef EmiP2PConn<P2PSockDelegate> Conn;
    typedef std::map<Address, Conn*, AddressCmp>             ConnMap;
    typedef typename ConnMap::iterator                       ConnMapIter;
    typedef std::map<ConnCookie, Conn*>                      ConnCookieMap;
    typedef typename ConnCookieMap::iterator                 ConnCookieMapIter;
    
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
                
                conn = new Conn(sock, address, config.rateLimit);
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
    
    // conn must not be NULL
    void gotConnectionOpenAck(const Address& address,
                              Conn *conn,
                              EmiTimeInterval now,
                              const uint8_t *rawData,
                              size_t len) {
        conn->gotTimestamp(address, now, rawData, len);
        
        // TODO
    }
    
    // conn might be NULL
    void gotConnectionClose(Conn *conn,
                            EmiTimeInterval now,
                            const uint8_t *rawData,
                            size_t len,
                            SocketHandle *sock,
                            const Address& address) {
        if (conn) {
            conn->gotTimestamp(address, now, rawData, len);
        }
        
        // TODO
    }
    
public:
    
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
            
            bool isTheOnlyMessageInThisPacket = (len == EMI_TIMESTAMP_LENGTH+header.headerLength+header.length);
            
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
                else if ((EMI_PRX_FLAG | EMI_RST_FLAG) == relevantFlags ||
                         EMI_RST_FLAG                  == relevantFlags) {
                    // This is a proxy connection close message,
                    // or a plain connection close message.
                    gotConnectionClose(conn, now, rawData, len, sock, address);
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
