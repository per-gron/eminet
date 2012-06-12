//
//  EmiBinding.mm
//  roshambo
//
//  Created by Per Eckerdal on 2012-04-24.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#include "EmiBinding.h"

#import "EmiSocket.h"
#include "EmiNetUtil.h"
#import "EmiDispatchQueueWrapper.h"

#import "GCDAsyncUdpSocket.h"
#include <Security/Security.h>
#include <CommonCrypto/CommonHMAC.h>
#include <arpa/inet.h>
#include <net/if.h>

#define DISPATCH_SYNC_OR_ASYNC(queue, block, method)        \
    {                                                       \
        dispatch_block_t DISPATCH_SYNC__block = (block);    \
        dispatch_queue_t DISPATCH_SYNC__queue = (queue);    \
        if (dispatch_get_current_queue() ==                 \
            DISPATCH_SYNC__queue) {                         \
            DISPATCH_SYNC__block();                         \
        }                                                   \
        else {                                              \
            dispatch_##method(DISPATCH_SYNC__queue,         \
                              DISPATCH_SYNC__block);        \
        }                                                   \
    }

#define DISPATCH_SYNC(queue, block)                         \
    DISPATCH_SYNC_OR_ASYNC(queue, block, sync)

#define DISPATCH_ASYNC(queue, block)                        \
    DISPATCH_SYNC_OR_ASYNC(queue, block, async)

EmiBinding::Timer::Timer(EmiDispatchQueueWrapper *timerCookie) :
_timerQueue(timerCookie->queue), _timer(NULL) {
    ASSERT(_timerQueue);
    dispatch_retain(_timerQueue);
}

EmiBinding::Timer::~Timer() {
    deschedule_();
    dispatch_release(_timerQueue);
}

void EmiBinding::Timer::deschedule_() {
    if (_timer) {
        dispatch_source_cancel(_timer);
        dispatch_release(_timer);
        _timer = NULL;
    }
}

void EmiBinding::Timer::schedule(TimerCb *timerCb, void *data, EmiTimeInterval interval,
                                 bool repeating, bool reschedule) {
    DISPATCH_SYNC(_timerQueue, ^{
        if (!reschedule && _timer) {
            // We were told not to re-schedule the timer. 
            // The timer is already active, so do nothing.
            return;
        }
        
        if (!_timer) {
            _timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, NULL, /*mask:*/0, _timerQueue);
            dispatch_resume(_timer);
        }
        
        dispatch_source_set_timer(_timer, dispatch_time(DISPATCH_TIME_NOW, interval * NSEC_PER_SEC),
                                  interval * NSEC_PER_SEC, /*leeway:*/0);
        dispatch_source_set_event_handler(_timer, ^{
            timerCb([NSDate timeIntervalSinceReferenceDate], this, data);
            
            if (!repeating) {
                deschedule_();
            }
        });
    });
}

void EmiBinding::Timer::deschedule() {
    DISPATCH_SYNC(_timerQueue, ^{
        deschedule_();
    });
}

bool EmiBinding::Timer::isActive() const {
    __block BOOL active;
    DISPATCH_SYNC(_timerQueue, ^{
        active = !!_timer;
    });
    return active;
}

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
    
    int family = ifa->ifa_addr->sa_family;
    if (!loopback && AF_INET == family) {
        memcpy(&addr, ifa->ifa_addr, sizeof(sockaddr));
    }
    else if (!loopback && AF_INET6 == family) {
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
