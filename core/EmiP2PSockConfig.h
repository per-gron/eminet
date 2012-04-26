//
//  EmiP2PSockConfig.h
//  roshambo
//
//  Created by Per Eckerdal on 2012-04-23.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#ifndef roshambo_EmiP2PSockConfig_h
#define roshambo_EmiP2PSockConfig_h

#include "EmiTypes.h"

#include <netinet/in.h>

class EmiP2PSockConfig {
public:
    EmiP2PSockConfig() :
    connectionTimeout(EMI_DEFAULT_CONNECTION_TIMEOUT),
    rateLimit(0),
    port(0),
    address(),
    fabricatedPacketDropRate(0) {}
    
    EmiTimeInterval connectionTimeout;
    // The approximate maximum number of bytes that will be allowed
    // per connection. Packets beyond this limit will be dropped. The
    // current implementation allows at most 2*rateLimit bytes any
    // given second.
    size_t rateLimit;
    uint16_t port;
    sockaddr_storage address;
    float fabricatedPacketDropRate;
};

#endif
