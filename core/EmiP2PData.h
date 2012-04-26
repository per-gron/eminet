//
//  EmiP2PData.h
//  roshambo
//
//  Created by Per Eckerdal on 2012-04-26.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#ifndef roshambo_EmiP2PData_h
#define roshambo_EmiP2PData_h

#include <cstring>

// The purpose of this class is to encapsulate the memory
// management of the P2P cookie and shared secret
class EmiP2PData {
public:
    EmiP2PData() :
    p2pCookie(NULL),
    p2pCookieLength(0),
    sharedSecret(NULL),
    sharedSecretLength(0) {}
    
    EmiP2PData(const uint8_t *p2pCookie_, size_t p2pCookieLength_,
               const uint8_t *sharedSecret_, size_t sharedSecretLength_) :
    p2pCookie(p2pCookie_ ? (uint8_t *)malloc(p2pCookieLength_) : NULL),
    p2pCookieLength(p2pCookieLength_),
    sharedSecret(sharedSecret_ ? (uint8_t *)malloc(sharedSecretLength_) : NULL),
    sharedSecretLength(sharedSecretLength_) {
        if (p2pCookie_) {
            std::memcpy(p2pCookie, p2pCookie_, p2pCookieLength_);
        }
        if (sharedSecret_) {
            std::memcpy(sharedSecret, sharedSecret_, sharedSecretLength_);
        }
    }
    
    ~EmiP2PData() {
        if (p2pCookie) {
            free(p2pCookie);
        }
        if (sharedSecret) {
            free(sharedSecret);
        }
    }
    
    EmiP2PData(const EmiP2PData& other) :
    p2pCookie(other.p2pCookie ? (uint8_t *)malloc(other.p2pCookieLength) : NULL),
    p2pCookieLength(other.p2pCookieLength),
    sharedSecret(other.sharedSecret ? (uint8_t *)malloc(other.sharedSecretLength) : NULL),
    sharedSecretLength(other.sharedSecretLength) {
        if (p2pCookie) {
            std::memcpy(p2pCookie, other.p2pCookie, p2pCookieLength);
        }
        
        if (sharedSecret) {
            std::memcpy(sharedSecret, other.sharedSecret, sharedSecretLength);
        }
    }
    EmiP2PData& operator=(const EmiP2PData& other) {
        if (p2pCookie) {
            free(p2pCookie);
        }
        if (sharedSecret) {
            free(sharedSecret);
        }
        
        p2pCookieLength = other.p2pCookieLength;
        p2pCookie = other.p2pCookie ? (uint8_t *)malloc(other.p2pCookieLength) : NULL;
        if (p2pCookie) {
            memcpy(p2pCookie, other.p2pCookie, other.p2pCookieLength);
        }
        
        sharedSecretLength = other.sharedSecretLength;
        sharedSecret = other.sharedSecret ? (uint8_t *)malloc(other.sharedSecretLength) : NULL;
        if (sharedSecret) {
            memcpy(sharedSecret, other.sharedSecret, other.sharedSecretLength);
        }
        
        return *this;
    }
    
    uint8_t *p2pCookie;
    size_t p2pCookieLength;
    uint8_t *sharedSecret;
    size_t sharedSecretLength;
};

#endif
