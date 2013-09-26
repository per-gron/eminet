//
//  EmiBinding.h
//  eminet
//
//  Created by Per Eckerdal on 2012-04-24.
//  Copyright (c) 2012 Per Eckerdal. All rights reserved.
//

#ifndef eminet_EmiBinding_h
#define eminet_EmiBinding_h

#import "EmiSocketUserDataWrapper.h"
#include "EmiDispatchTimer.h"

#include "EmiTypes.h"
#import <Foundation/Foundation.h>
#include <utility>
#include <ifaddrs.h>

class EmiSockDelegate;
@class GCDAsyncUdpSocket;
@class EmiDispatchQueueWrapper;

class EmiBinding {
private:
    inline EmiBinding();
public:
    
    typedef EmiDispatchTimer         Timer;
    typedef EmiDispatchQueueWrapper* TimerCookie;
    
    typedef __strong NSError* Error;
    typedef GCDAsyncUdpSocket SocketHandle;
    // PersistentData is data that is assumed to be
    // stored until it is explicitly released with the
    // releasePersistentData method. PersistentData must
    // have indefinite extent; it must not be deallocated
    // until releasePersistentData is called on it.
    //
    // PersistentData objects must be copyable and
    // assignable, and these operations must not interfere
    // with the lifetime of the buffer.
    typedef NSData* PersistentData;
    // TemporaryData is data that is assumed to be
    // released after the duration of the call; it can
    // be stored on the stack, for instance. EmiNet core
    // will not explicitly release TempraryData objects.
    //
    // TemporaryData objects must be copyable and
    // assignable, and these operations must not interfere
    // with the lifetime of the buffer.
    typedef NSData* TemporaryData;
    
    inline static NSError *makeError(const char *domain, int32_t code) {
        return [NSError errorWithDomain:[NSString stringWithCString:domain
                                                           encoding:NSUTF8StringEncoding]
                                   code:code
                               userInfo:nil];
    }
    
    inline static NSData *makePersistentData(const uint8_t *data, size_t length) {
        // The Objective-C EmiNet bindings don't make any distinction
        // between temporary and persistent buffers, because all buffers
        // are persistent.
        return [NSData dataWithBytes:data length:length];
    }
    inline static NSData *makeTemporaryData(size_t size, uint8_t **outData) {
        NSMutableData *result = [NSMutableData dataWithLength:size];
        *outData = (uint8_t *)[result bytes];
        return result;
    }
    inline static void releasePersistentData(NSData *data) {
        // Because of ARC, we can leave this as a no-op
    }
    inline static NSData *castToTemporary(NSData *data) {
        return data;
    }
    
    inline static const uint8_t *extractData(NSData *data) {
        return (const uint8_t *)[data bytes];
    }
    inline static size_t extractLength(NSData *data) {
        return [data length];
    }
    
    static const size_t HMAC_HASH_SIZE = 32;
    static void hmacHash(const uint8_t *key, size_t keyLength,
                         const uint8_t *data, size_t dataLength,
                         uint8_t *buf, size_t bufLen);
    static void randomBytes(uint8_t *buf, size_t bufSize);
    
    inline static Timer *makeTimer(EmiDispatchQueueWrapper *timerCookie) {
        return new Timer(timerCookie);
    }
    inline static void freeTimer(Timer *timer) {
        delete timer;
    }
    inline static void scheduleTimer(Timer *timer, TimerCb *timerCb, void *data, EmiTimeInterval interval,
                                     bool repeating, bool reschedule) {
        timer->schedule(timerCb, data, interval, repeating, reschedule);
    }
    static void descheduleTimer(Timer *timer) {
        timer->deschedule();
    }
    
    typedef std::pair<ifaddrs*, ifaddrs*> NetworkInterfaces;
    static bool getNetworkInterfaces(NetworkInterfaces& ni, Error& err);
    static bool nextNetworkInterface(NetworkInterfaces& ni, const char*& name, struct sockaddr_storage& addr);
    static void freeNetworkInterfaces(const NetworkInterfaces& ni);
    
    static void closeSocket(GCDAsyncUdpSocket *socket);
    static GCDAsyncUdpSocket *openSocket(EmiDispatchQueueWrapper *socketCookie,
                                         EmiOnMessage *callback,
                                         void *userData,
                                         const sockaddr_storage& address,
                                         __strong NSError*& err);
    static void extractLocalAddress(GCDAsyncUdpSocket *socket, sockaddr_storage& address);
    static void sendData(GCDAsyncUdpSocket *socket, const sockaddr_storage& address, const uint8_t *data, size_t size);
};

#endif
