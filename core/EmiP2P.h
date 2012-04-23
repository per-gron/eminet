//
//  EmiP2P.h
//  roshambo
//
//  Created by Per Eckerdal on 2012-04-23.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#ifndef roshambo_EmiP2P_h
#define roshambo_EmiP2P_h

static const size_t          EMI_P2P_SERVER_SECRET_SIZE = 32;
static const size_t          EMI_SHARED_SECRET_SIZE     = 32;
static const EmiTimeInterval EMI_P2P_COOKIE_RESOLUTION  = 5*60; // In seconds

template<class SockDelegate>
class EmiP2P {
private:
    // Private copy constructor and assignment operator
    inline EmiP2P(const EmiP2P& other);
    inline EmiP2P& operator=(const EmiP2P& other);
    
    uint8_t _serverSecret[EMI_P2P_SERVER_SECRET_SIZE];
    
public:
    EmiP2P() {
        SockDelegate::randomBytes(_serverSecret, sizeof(_serverSecret));
    }
    virtual ~EmiP2P() {}
    
    static void generateSharedSecret();
    static void generateP2PCookie(EmiTimeInterval stamp,
                                  uint8_t *buf, size_t bufLen);
    static bool checkP2PCookie(EmiTimeInterval now,
                               uint8_t *buf, size_t bufLen);
};

#endif
