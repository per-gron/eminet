//
//  EmiP2PEndpoints.h
//  eminet
//
//  Created by Per Eckerdal on 2012-06-27.
//  Copyright (c) 2012 Per Eckerdal. All rights reserved.
//

#ifndef eminet_EmiP2PEndpoints_h
#define eminet_EmiP2PEndpoints_h

#include <cstring>

// The purpose of this class is to encapsulate the memory
// management of the P2P endpoint pairs
//
// FIXME This is pretty much a 100% duplicate of EmiP2PData.
// Deduplicate.
class EmiP2PEndpoints {
public:
    EmiP2PEndpoints() :
    family(0),
    myEndpointPair(NULL),
    myEndpointPairLength(0),
    peerEndpointPair(NULL),
    peerEndpointPairLength(0) {}
    
    EmiP2PEndpoints(int family_,
                    const uint8_t *myEndpointPair_, size_t myEndpointPairLength_,
                    const uint8_t *peerEndpointPair_, size_t peerEndpointPairLength_) :
    family(family_),
    myEndpointPair(myEndpointPair_ ? (uint8_t *)malloc(myEndpointPairLength_) : NULL),
    myEndpointPairLength(myEndpointPairLength_),
    peerEndpointPair(peerEndpointPair_ ? (uint8_t *)malloc(peerEndpointPairLength_) : NULL),
    peerEndpointPairLength(peerEndpointPairLength_) {
        if (myEndpointPair_) {
            std::memcpy(myEndpointPair, myEndpointPair_, myEndpointPairLength_);
        }
        if (peerEndpointPair_) {
            std::memcpy(peerEndpointPair, peerEndpointPair_, peerEndpointPairLength_);
        }
    }
    
    ~EmiP2PEndpoints() {
        if (myEndpointPair) {
            free(myEndpointPair);
        }
        if (peerEndpointPair) {
            free(peerEndpointPair);
        }
    }
    
    EmiP2PEndpoints(const EmiP2PEndpoints& other) :
    family(other.family),
    myEndpointPair(other.myEndpointPair ? (uint8_t *)malloc(other.myEndpointPairLength) : NULL),
    myEndpointPairLength(other.myEndpointPairLength),
    peerEndpointPair(other.peerEndpointPair ? (uint8_t *)malloc(other.peerEndpointPairLength) : NULL),
    peerEndpointPairLength(other.peerEndpointPairLength) {
        if (myEndpointPair) {
            std::memcpy(myEndpointPair, other.myEndpointPair, myEndpointPairLength);
        }
        
        if (peerEndpointPair) {
            std::memcpy(peerEndpointPair, other.peerEndpointPair, peerEndpointPairLength);
        }
    }
    EmiP2PEndpoints& operator=(const EmiP2PEndpoints& other) {
        if (myEndpointPair) {
            free(myEndpointPair);
        }
        if (peerEndpointPair) {
            free(peerEndpointPair);
        }
        
        family = other.family;
        
        myEndpointPairLength = other.myEndpointPairLength;
        myEndpointPair = other.myEndpointPair ? (uint8_t *)malloc(other.myEndpointPairLength) : NULL;
        if (myEndpointPair) {
            memcpy(myEndpointPair, other.myEndpointPair, other.myEndpointPairLength);
        }
        
        peerEndpointPairLength = other.peerEndpointPairLength;
        peerEndpointPair = other.peerEndpointPair ? (uint8_t *)malloc(other.peerEndpointPairLength) : NULL;
        if (peerEndpointPair) {
            memcpy(peerEndpointPair, other.peerEndpointPair, other.peerEndpointPairLength);
        }
        
        return *this;
    }
    
    int family;
    uint8_t *myEndpointPair;
    size_t myEndpointPairLength;
    uint8_t *peerEndpointPair;
    size_t peerEndpointPairLength;
    
    void extractAddress(bool me, bool inner, sockaddr_storage *addr) {
        
        const size_t ipLen = EmiNetUtil::familyIpLength(family);
        static const size_t portLen = sizeof(uint16_t);
        const size_t endpointPairLen = 2*(ipLen+portLen);
        
        ASSERT(endpointPairLen == (me ? myEndpointPairLength : peerEndpointPairLength));
        
        const uint8_t *dataPtr = (me ? myEndpointPair : peerEndpointPair)+(inner ? 0 : ipLen+portLen);
        
        EmiNetUtil::makeAddress(family,
                                dataPtr, ipLen,
                                *((uint16_t *)(dataPtr+ipLen)),
                                addr);
        
    }
    
    void extractMyInnerAddress(sockaddr_storage *addr) {
        extractAddress(/*me:*/true, /*inner:*/true, addr);
    }
    
    void extractMyOuterAddress(sockaddr_storage *addr) {
        extractAddress(/*me:*/true, /*inner:*/false, addr);
    }
    
    void extractPeerInnerAddress(sockaddr_storage *addr) {
        extractAddress(/*me:*/false, /*inner:*/true, addr);
    }
    
    void extractPeerOuterAddress(sockaddr_storage *addr) {
        extractAddress(/*me:*/false, /*inner:*/false, addr);
    }
};

#endif
