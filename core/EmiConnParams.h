//
//  EmiConnParams.h
//  roshambo
//
//  Created by Per Eckerdal on 2012-04-23.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#ifndef roshambo_EmiConnParams_h
#define roshambo_EmiConnParams_h

// The main purpose of this class is to be able to
// change the number of parameters to EmiConn's
// constructor without requiring to change all bindings.
//
// With this class, one can simply add a new instance
// variable to it instead.
template<class Address>
struct EmiConnParams {
    EmiConnParams(const Address& address_, uint16_t inboundPort_, bool initiator_) :
    address(address_),
    inboundPort(inboundPort_),
    initiator(initiator_) {}
    
    const Address& address;
    const uint16_t inboundPort;
    const bool initiator;
};

#endif
