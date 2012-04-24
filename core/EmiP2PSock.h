//
//  EmiP2P.h
//  roshambo
//
//  Created by Per Eckerdal on 2012-04-23.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#ifndef roshambo_EmiP2PSock_h
#define roshambo_EmiP2PSock_h

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
        
        __block const char *err = NULL;
        
        const uint8_t *rawData(Binding::extractData(data)+offset);
        
        ConnMapIter cur = _conns.find(address);
        __block Conn *conn = _conns.end() == cur ? NULL : (*cur).second;
        
        if (conn) {
            conn->gotPacket();
        }
        
        if (EMI_TIMESTAMP_LENGTH+1 == len) {
            // This is a heartbeat packet
        }
        else if (len < EMI_TIMESTAMP_LENGTH + EMI_HEADER_LENGTH) {
            err = "Packet too short";
            goto error;
        }
        else {
            EmiMessageHeader::EmiParseMessageBlock block =
            ^ bool (const EmiMessageHeader& header, size_t dataOffset) {
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
    
    P2PSockDelegate& getDelegate() {
        return _delegate;
    }
    
    const P2PSockDelegate& getDelegate() const {
        return _delegate;
    }
};

#endif
