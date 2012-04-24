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

static const uint64_t ARC4RANDOM_MAX = 0x100000000;


template<class SockDelegate>
class EmiP2PSock {
    
    static const size_t          EMI_P2P_SERVER_SECRET_SIZE = 32;
    static const size_t          EMI_P2P_SHARED_SECRET_SIZE = 32;
    static const size_t          EMI_P2P_RAND_NUM_SIZE = 8;
    static const size_t          EMI_P2P_COOKIE_SIZE = EMI_P2P_RAND_NUM_SIZE + SockDelegate::HMAC_HASH_SIZE;
    
    typedef typename SockDelegate::SocketHandle     SocketHandle;
    typedef typename SockDelegate::TemporaryData    TemporaryData;
    typedef typename SockDelegate::Error            Error;
    typedef typename SockDelegate::Address          Address;
    typedef typename SockDelegate::AddressCmp       AddressCmp;
    
    typedef EmiP2PSockConfig<Address>                     SockConfig;
    typedef EmiP2PConn<SockDelegate, EMI_P2P_COOKIE_SIZE> Conn;
    typedef std::map<Address, Conn*, AddressCmp>          ConnMap;
    typedef typename ConnMap::iterator                    ConnMapIter;
    
private:
    // Private copy constructor and assignment operator
    inline EmiP2PSock(const EmiP2PSock& other);
    inline EmiP2PSock& operator=(const EmiP2PSock& other);
    
    uint8_t       _serverSecret[EMI_P2P_SERVER_SECRET_SIZE];
    SocketHandle *_socket;
    
    // The keys of this map are the Conn*'s peer addresses;
    // each conn has two entries in _conns.
    ConnMap _conns;
    
    inline bool shouldArtificiallyDropPacket() const {
        if (0 == config.fabricatedPacketDropRate) return false;
        
        return ((float)arc4random() / ARC4RANDOM_MAX) < config.fabricatedPacketDropRate;
    }
    
    void hashCookie(EmiTimeInterval stamp, uint8_t *randNum,
                       uint8_t *buf, size_t bufLen, bool minusOne = false) const {
        if (bufLen < SockDelegate::HMAC_HASH_SIZE) {
            SockDelegate::panic();
        }
        
        uint64_t integerStamp = floor(stamp/EMI_P2P_COOKIE_RESOLUTION - (minusOne ? 1 : 0));
        
        uint8_t toBeHashed[EMI_P2P_RAND_NUM_SIZE+sizeof(integerStamp)];
        
        memcpy(toBeHashed, randNum, EMI_P2P_RAND_NUM_SIZE);
        *((uint64_t *)toBeHashed+EMI_P2P_RAND_NUM_SIZE) = integerStamp;
        
        SockDelegate::hmacHash(_serverSecret, sizeof(_serverSecret),
                               toBeHashed, sizeof(toBeHashed),
                               buf, bufLen);
    }
    
    bool checkCookie(EmiTimeInterval stamp,
                        uint8_t *buf, size_t bufLen) const {
        if (EMI_P2P_COOKIE_SIZE != bufLen) {
            return false;
        }
        
        char testBuf[SockDelegate::HMAC_HASH_SIZE];
        
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
    
    EmiP2PSock(const SockConfig& config_, const SockDelegate& delegate) :
    config(config_), _socket(NULL) {
        SockDelegate::randomBytes(_serverSecret, sizeof(_serverSecret));
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
            SockDelegate::closeSocket(*this, _socket);
            _socket = NULL;
        }
    }
    
    bool desuspend(Error& err) {
        if (!_socket) {
            _socket = _delegate.openSocket(*this, config.port, err);
            if (!_socket) return false;
        }
        
        return true;
    }
    
    // Returns the size of the cookie
    size_t generateCookie(EmiTimeInterval stamp,
                          uint8_t *buf, size_t bufLen) const {
        if (bufLen < EMI_P2P_COOKIE_SIZE) {
            SockDelegate::panic();
        }
        
        SockDelegate::randomBytes(buf, EMI_P2P_RAND_NUM_SIZE);
        
        hashCookie(stamp, /*randNum:*/buf,
                      buf+EMI_P2P_RAND_NUM_SIZE, bufLen-EMI_P2P_RAND_NUM_SIZE);
        
        return EMI_P2P_COOKIE_SIZE;
    }
    
    // Returns the size of the shared secret
    static size_t generateSharedSecret(uint8_t *buf, size_t bufLen) {
        if (bufLen < EMI_P2P_SHARED_SECRET_SIZE) {
            SockDelegate::panic();
        }
        
        SockDelegate::randomBytes(buf, EMI_P2P_SHARED_SECRET_SIZE);
        
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
        
        const uint8_t *rawData(SockDelegate::extractData(data)+offset);
        
        ConnMapIter cur = _conns.find(ckey);
        __block Conn *conn = _conns.end() == cur ? NULL : (*cur).second;
        
        if (conn) {
            conn->gotPacket(len);
        }
        
        if (EMI_TIMESTAMP_LENGTH+1 == len) {
            // This is a heartbeat packet
            if (conn) {
                conn->gotTimestamp(now, rawData, len);
                conn->gotHeartbeat(!!(rawData[EMI_TIMESTAMP_LENGTH]));
            }
        }
        else if (len < EMI_TIMESTAMP_LENGTH + EMI_HEADER_LENGTH) {
            err = "Packet too short";
            goto error;
        }
        else {
            EmiMessageHeader::EmiParseMessageBlock block =
            ^ bool (const EmiMessageHeader& header, size_t dataOffset) {
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
};

#endif
