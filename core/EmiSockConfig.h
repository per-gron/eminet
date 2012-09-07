//
//  EmiSockConfig.h
//  roshambo
//
//  Created by Per Eckerdal on 2012-04-23.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#ifndef roshambo_EmiSockConfig_h
#define roshambo_EmiSockConfig_h

#include "EmiTypes.h"
#include "EmiNetUtil.h"

#include <netinet/in.h>

class EmiSockConfig {
public:
    EmiSockConfig() :
    mtu(EMI_MINIMAL_MTU),
    heartbeatFrequency(EMI_DEFAULT_HEARTBEAT_FREQUENCY),
    connectionTimeout(EMI_DEFAULT_CONNECTION_TIMEOUT),
    initialConnectionTimeout(EMI_DEFAULT_CONNECTION_TIMEOUT),
    heartbeatsBeforeConnectionWarning(EMI_DEFAULT_HEARTBEATS_BEFORE_CONNECTION_WARNING),
    receiverBufferSize(EMI_DEFAULT_RECEIVER_BUFFER_SIZE),
    senderBufferSize(EMI_DEFAULT_SENDER_BUFFER_SIZE),
    acceptConnections(false),
    port(0),
    fabricatedPacketDropRate(0) {
        EmiNetUtil::anyAddr(0, AF_INET, &address);
    }
    
    size_t mtu;
    float heartbeatFrequency;
    EmiTimeInterval connectionTimeout;
    EmiTimeInterval initialConnectionTimeout;
    float heartbeatsBeforeConnectionWarning;
    size_t receiverBufferSize;
    size_t senderBufferSize;
    bool acceptConnections;
    uint16_t port;
    sockaddr_storage address;
    float fabricatedPacketDropRate;
};

#endif
