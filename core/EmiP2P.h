//
//  EmiP2P.h
//  roshambo
//
//  Created by Per Eckerdal on 2012-04-23.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#ifndef roshambo_EmiP2P_h
#define roshambo_EmiP2P_h

#include <algorithm>

static const size_t          EMI_P2P_SERVER_SECRET_SIZE = 32;
static const size_t          EMI_P2P_SHARED_SECRET_SIZE = 32;
static const EmiTimeInterval EMI_P2P_COOKIE_RESOLUTION  = 5*60; // In seconds

template<class SockDelegate>
class EmiP2P {
    static const size_t RAND_NUM_SIZE = 8;
    static const size_t COOKIE_SIZE = RAND_NUM_SIZE + SockDelegate::HMAC_HASH_SIZE;
    
private:
    // Private copy constructor and assignment operator
    inline EmiP2P(const EmiP2P& other);
    inline EmiP2P& operator=(const EmiP2P& other);
    
    uint8_t _serverSecret[EMI_P2P_SERVER_SECRET_SIZE];
    
    void hashP2PCookie(EmiTimeInterval stamp, uint8_t *randNum,
                       uint8_t *buf, size_t bufLen, bool minusOne = false) {
        if (bufLen < SockDelegate::HMAC_HASH_SIZE) {
            SockDelegate::panic();
        }
        
        uint64_t integerStamp = floor(stamp/EMI_P2P_COOKIE_RESOLUTION - (minusOne ? 1 : 0));
        
        uint8_t toBeHashed[RAND_NUM_SIZE+sizeof(integerStamp)];
        
        memcpy(toBeHashed, randNum, RAND_NUM_SIZE);
        *((uint64_t *)toBeHashed+RAND_NUM_SIZE) = integerStamp;
        
        SockDelegate::hmacHash(_serverSecret, sizeof(_serverSecret),
                               toBeHashed, sizeof(toBeHashed),
                               buf, bufLen);
    }
    
public:
    EmiP2P() {
        SockDelegate::randomBytes(_serverSecret, sizeof(_serverSecret));
    }
    virtual ~EmiP2P() {}
    
    // Returns the size of the shared secret
    static size_t generateSharedSecret(uint8_t *buf, size_t bufLen) {
        if (bufLen < EMI_P2P_SHARED_SECRET_SIZE) {
            SockDelegate::panic();
        }
        
        SockDelegate::randomBytes(buf, EMI_P2P_SHARED_SECRET_SIZE);
        
        return EMI_P2P_SHARED_SECRET_SIZE;
    }
    
    // Returns the size of the cookie
    size_t generateP2PCookie(EmiTimeInterval stamp,
                             uint8_t *buf, size_t bufLen) {
        if (bufLen < COOKIE_SIZE) {
            SockDelegate::panic();
        }
        
        SockDelegate::randomBytes(buf, RAND_NUM_SIZE);
        
        hashP2PCookie(stamp, /*randNum:*/buf,
                      buf+RAND_NUM_SIZE, bufLen-RAND_NUM_SIZE);
        
        return COOKIE_SIZE;
    }
    
    bool checkP2PCookie(EmiTimeInterval stamp,
                        uint8_t *buf, size_t bufLen) {
        if (COOKIE_SIZE != bufLen) {
            return false;
        }
        
        char testBuf[SockDelegate::HMAC_HASH_SIZE];
        
        hashP2PCookie(stamp, buf, testBuf, sizeof(testBuf));
        generateP2PCookie(stamp, buf+RAND_NUM_SIZE, bufLen-RAND_NUM_SIZE);
        if (0 == memcmp(testBuf, buf, sizeof(testBuf))) {
            return true;
        }
        
        hashP2PCookie(stamp, buf, testBuf, sizeof(testBuf), /*minusOne:*/true);
        generateP2PCookie(stamp, buf+RAND_NUM_SIZE, bufLen-RAND_NUM_SIZE);
        if (0 == memcmp(testBuf, buf, sizeof(testBuf))) {
            return true;
        }
        
        return false;
    }
};

#endif
