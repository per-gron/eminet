//
//  EmiBinding.mm
//  eminet
//
//  Created by Per Eckerdal on 2012-04-24.
//  Copyright (c) 2012 Per Eckerdal. All rights reserved.
//

#include "EmiBinding.h"

#import "EmiSocket.h"
#include "EmiNetUtil.h"
#import "EmiDispatchQueueWrapper.h"

#import "GCDAsyncUdpSocket.h"
#include <Security/SecRandom.h>
#include <CommonCrypto/CommonHMAC.h>
#include <arpa/inet.h>
#include <net/if.h>

void EmiBinding::hmacHash(const uint8_t *key, size_t keyLength,
                          const uint8_t *data, size_t dataLength,
                          uint8_t *buf, size_t bufLen) {
    ASSERT(bufLen >= 256/8);
    CCHmac(kCCHmacAlgSHA256, key, keyLength, data, dataLength, buf);
}

void EmiBinding::randomBytes(uint8_t *buf, size_t bufSize) {
    ASSERT(0 == SecRandomCopyBytes(kSecRandomDefault, bufSize, buf));
}

bool EmiBinding::getNetworkInterfaces(NetworkInterfaces& ni, Error& err) {
    int ret = getifaddrs(&ni.first);
    if (-1 == ret) {
        err = makeError("com.emilir.eminet.networkifaces", 0);
        return false;
    }
    
    ni.second = ni.first;
    return true;
}

bool EmiBinding::nextNetworkInterface(NetworkInterfaces& ni, const char*& name, struct sockaddr_storage& addr) {
    ifaddrs *ifa = ni.second;
    if (!ifa) {
        return false;
    }
    
    ni.second = ifa->ifa_next;
    
    bool loopback = !!(ifa->ifa_flags & IFF_LOOPBACK);
    // TODO(peck): I don't remember why I skipped loopback interfaces.
    // I'm disabling that for now.
    static const bool skipLoopback = false;
    
    int family = ifa->ifa_addr->sa_family;
    if (!(skipLoopback && loopback) && AF_INET == family) {
        memcpy(&addr, ifa->ifa_addr, sizeof(sockaddr));
    }
    else if (!(skipLoopback && loopback) && AF_INET6 == family) {
        memcpy(&addr, ifa->ifa_addr, sizeof(sockaddr_storage));
    }
    else {
        // Some other address family that we don't support or care about. Continue the search.
        return nextNetworkInterface(ni, name, addr);
    }
    
    name = ifa->ifa_name;
    
    return true;
}

void EmiBinding::freeNetworkInterfaces(const NetworkInterfaces& ni) {
    freeifaddrs(ni.first);
}


void EmiBinding::closeSocket(GCDAsyncUdpSocket *socket) {
    // This ensures that we don't send onMessage events to
    // deallocated objects.
    socket.userData = nil;
    
    [socket close];
}

GCDAsyncUdpSocket *EmiBinding::openSocket(EmiDispatchQueueWrapper *socketCookie,
                                          EmiOnMessage *callback,
                                          void *userData,
                                          const sockaddr_storage& address,
                                          __strong NSError*& err) {
    GCDAsyncUdpSocket *socket = [[GCDAsyncUdpSocket alloc] initWithDelegate:[EmiSocket class]
                                                              delegateQueue:socketCookie->queue
                                                                socketQueue:socketCookie->queue];
    socket.userData = [[EmiSocketUserDataWrapper alloc] initWithUserData:userData callback:callback];
    
    if (![socket bindToAddress:[NSData dataWithBytes:&address
                                              length:EmiNetUtil::addrSize(address)]
                         error:&err]) {
        return nil;
    }
    
    if (![socket beginReceiving:&err]) {
        return nil;
    }
    
    return socket;
}

void EmiBinding::extractLocalAddress(GCDAsyncUdpSocket *socket, sockaddr_storage& address) {
    NSData *a = [socket localAddress];
    // If there is no address, a can have length 0.
    // To ensure that we don't return garbage, begin
    // with filling out address with something that is
    // at least valid.
    EmiNetUtil::anyAddr(0, AF_INET, &address);
    if (a) {
        memcpy(&address, [a bytes], MIN([a length], sizeof(sockaddr_storage)));
    }
}

void EmiBinding::sendData(GCDAsyncUdpSocket *socket, const sockaddr_storage& address, const uint8_t *data, size_t size) {
    // TODO This copies the packet data. We might want to redesign
    // this part of the code so that this is not required.
    [socket sendData:[NSData dataWithBytes:data     length:size]
           toAddress:[NSData dataWithBytes:&address length:EmiNetUtil::addrSize(address)]
         withTimeout:-1 tag:0];
}
