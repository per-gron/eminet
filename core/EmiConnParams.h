//
//  EmiConnParams.h
//  eminet
//
//  Created by Per Eckerdal on 2012-04-23.
//  Copyright (c) 2012 Per Eckerdal. All rights reserved.
//

#ifndef eminet_EmiConnParams_h
#define eminet_EmiConnParams_h

#include "EmiP2PData.h"
#include "EmiUdpSocket.h"
#include "EmiNetUtil.h"

#include <netinet/in.h>

// The main purpose of this class is to be able to
// change the number of parameters to EmiConn's
// constructor without requiring to change all bindings.
//
// With this class, one can simply add a new instance
// variable to it instead.
template<class Binding>
class EmiConnParams {
public:
    inline EmiConnParams(EmiUdpSocket<Binding> *socket_, const sockaddr_storage& address_, uint16_t inboundPort_) :
    socket(socket_),
    address(address_),
    inboundPort(inboundPort_),
    type(EMI_CONNECTION_TYPE_SERVER),
    p2p() {}
    
    inline EmiConnParams(const sockaddr_storage& address_,
                         const uint8_t *p2pCookie_, size_t p2pCookieLength_,
                         const uint8_t *sharedSecret_, size_t sharedSecretLength_) :
    socket(NULL),
    address(address_),
    inboundPort(0),
    type(p2pCookie_ && sharedSecret_ ? EMI_CONNECTION_TYPE_P2P : EMI_CONNECTION_TYPE_CLIENT),
    p2p(p2pCookie_, p2pCookieLength_, sharedSecret_, sharedSecretLength_) {}
    
    EmiUdpSocket<Binding>* const socket;
    const sockaddr_storage address;
    const uint16_t inboundPort; // This is set if socket != NULL
    const EmiConnectionType type;
    EmiP2PData p2p;
};

#endif
