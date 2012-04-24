//
//  EmiP2PConn.h
//  roshambo
//
//  Created by Per Eckerdal on 2012-04-24.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#ifndef roshambo_EmiP2PConn_h
#define roshambo_EmiP2PConn_h

class EmiP2PConn {
private:
    // Private copy constructor and assignment operator
    inline EmiP2PConn(const EmiP2PConn& other);
    inline EmiP2PConn& operator=(const EmiP2PConn& other);
    
    Address peers[2];
    Address innerEndpoints[2];
    uint8_t cookie[COOKIE_SIZE];
    size_t bytesSentSinceRateLimitTimeout;
    
    // connection timeout
    // rate limit timeout
    
public:
    EmiP2PConn() {}
    virtual ~EmiP2PConn() {}
};

#endif
