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

#include <algorithm>
#include <cmath>
#include <map>

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
    
    typedef EmiP2PSockConfig<Address>                        SockConfig;
    typedef EmiP2PConn<P2PSockDelegate, EMI_P2P_COOKIE_SIZE> Conn;
    typedef std::map<Address, Conn*, AddressCmp>             ConnMap;
    typedef typename ConnMap::iterator                       ConnMapIter;
    
private:
    // Private copy constructor and assignment operator
    inline EmiP2PSock(const EmiP2PSock& other);
    inline EmiP2PSock& operator=(const EmiP2PSock& other);
    
    uint8_t         _serverSecret[EMI_P2P_SERVER_SECRET_SIZE];
    SocketHandle   *_socket;
    P2PSockDelegate _delegate;
    
    // The keys of this map are the Conn*'s peer addresses;
    // each conn has two entries in _conns.
    ConnMap _conns;
    
    inline bool shouldArtificiallyDropPacket() const {
        if (0 == config.fabricatedPacketDropRate) return false;
        
        return ((float)arc4random() / ARC4RANDOM_MAX) < config.fabricatedPacketDropRate;
    }
    
    void hashCookie(EmiTimeInterval stamp, uint8_t *randNum,
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
    
    bool checkCookie(EmiTimeInterval stamp,
                        uint8_t *buf, size_t bufLen) const {
        if (EMI_P2P_COOKIE_SIZE != bufLen) {
            return false;
        }
        
        char testBuf[Binding::HMAC_HASH_SIZE];
        
        hashCookie(stamp, buf, testBuf, sizeof(testBuf));
        generateCookie(stamp, buf+EMI_P2P_RAND_NUM_SIZE, bufLen-EMI_P2P_RAND_NUM_SIZE);
        if (0 == memcmp(testBuf, buf, sizeof(testBuf))) {
            return true;
        }
        
        hashCookie(stamp, buf, testBuf, sizeof(testBuf), /*minusOne:*/true);
        generateCookie(stamp, buf+EMI_P2P_RAND_NUM_SIZE, bufLen-EMI_P2P_RAND_NUM_SIZE);
        if (0 == memcmp(testBuf, buf, sizeof(testBuf))) {
            return true;
        }
        
        return false;
    }
    
    Conn *findConn(const Address& address) {
        ConnMapIter cur = _conns.find(address);
        return _conns.end() == cur ? NULL : (*cur).second;
    }
    
    void gotConnectionOpen() {
        // TODO
    }
    
    void gotConnectionOpenAck() {
        // TODO
    }
    
    void gotConnectionClose(EmiTimeInterval now,
                            SocketHandle *sock,
                            const Address& address) {
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
        
        P2PSockDelegate::randomBytes(buf, EMI_P2P_RAND_NUM_SIZE);
        
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
        
        Conn *conn = findConn(address);
        if (conn) {
            conn->gotPacket();
        }
        
        if (EMI_TIMESTAMP_LENGTH+1 == len) {
            // This is a heartbeat packet. Just forward the packet (if we can)
            if (conn) {
                conn->forwardPacket(now, sock, address, data, offset, len);
            }
        }
        else if (len < EMI_TIMESTAMP_LENGTH + EMI_HEADER_LENGTH) {
            err = "Packet too short";
            goto error;
        }
        else {
            const uint8_t *rawData(Binding::extractData(data)+offset);
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
                    gotConnectionOpen();
                }
                else if ((EMI_PRX_FLAG | EMI_ACK_FLAG) == relevantFlags) {
                    // This is a connection open ACK message.
                    gotConnectionOpenAck();
                }
                else if ((EMI_PRX_FLAG | EMI_RST_FLAG) == relevantFlags ||
                         EMI_RST_FLAG                  == relevantFlags) {
                    // This is a proxy connection close message,
                    // or a plain connection close message.
                    gotConnectionClose(now, sock, address);
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
                    conn->forwardPacket(now, sock, address, data, offset, len);
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
