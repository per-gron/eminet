//
//  EmiSocket.m
//  roshambo
//
//  Created by Per Eckerdal on 2012-02-16.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#import "EmiSocketInternal.h"

#import "EmiConnectionInternal.h"
#import "EmiSocketConfigInternal.h"
#include "EmiSendQueue.h"
#include "EmiConnDelegate.h"

#import "GCDAsyncUdpSocket.h"
#include <map>
#include <set>


#pragma mark - Helper classes

@interface EmiConnectionOpenedBlockWrapper : NSObject {
    EmiConnectionOpenedBlock _block;
}

- (id)initWithBlock:(EmiConnectionOpenedBlock)block;
+ (EmiConnectionOpenedBlockWrapper *)wrapperWithBlock:(EmiConnectionOpenedBlock)block;
- (EmiConnectionOpenedBlock)block;

@end

@implementation EmiConnectionOpenedBlockWrapper

- (id)initWithBlock:(EmiConnectionOpenedBlock)block {
    if (self = [super init]) {
        _block = [block copy];
    }
    
    return self;
}

+ (EmiConnectionOpenedBlockWrapper *)wrapperWithBlock:(EmiConnectionOpenedBlock)block {
    return [[EmiConnectionOpenedBlockWrapper alloc] initWithBlock:block];
}

- (EmiConnectionOpenedBlock)block {
    return _block;
}

@end


#pragma mark - EmiSocket implementation

@implementation EmiSocket

@synthesize delegate = _delegate;


#pragma mark - Object lifecycle

- (id)init {
    if (self = [super init]) {
        _sock = nil;
    }
    
    return self;
}

- (void)dealloc {
    if (nil != _sock) {
        delete (S *)_sock;
        _sock = nil;
    }
    
    _delegate = nil;
}

- (BOOL)startWithError:(NSError **)error {
    return [self startWithConfig:[[EmiSocketConfig alloc] init] error:error];
}

- (BOOL)startWithConfig:(EmiSocketConfig *)config error:(NSError **)errPtr {
    EmiSockConfig<NSData *> *sc = [config sockConfig];
    
    _sock = new S(*sc, EmiSockDelegate(self));
    
    NSError *err = nil;
    ((S *)_sock)->desuspend(err);
    *errPtr = err;
    
    return !err;
}


#pragma mark - Public methods

- (BOOL)open {
    return ((S *)_sock)->isOpen();
}

- (void)suspend {
    ((S *)_sock)->suspend();
}

- (BOOL)desuspendWithError:(NSError **)errPtr {
    NSError *err = nil;
    BOOL retVal = ((S *)_sock)->desuspend(err);
    *errPtr = err;
    
    return retVal;
}

- (BOOL)connectToAddress:(NSData *)address block:(EmiConnectionOpenedBlock)block error:(NSError **)errPtr {
    NSError *err = nil;
    BOOL retVal = ((S *)_sock)->connect([NSDate timeIntervalSinceReferenceDate], address, block, err);
    *errPtr = err;
    
    return retVal;
}

- (BOOL)connectToHost:(NSString *)host onPort:(uint16_t)port block:(EmiConnectionOpenedBlock)block error:(NSError **)err {
    // We use the connect functionality of GCDAsyncUdpSocket only to get access to the DNS
    // resolve functions.
    _resolveSocket = [[GCDAsyncUdpSocket alloc] initWithDelegate:self delegateQueue:dispatch_get_current_queue()];
    _resolveSocket.userData = [EmiConnectionOpenedBlockWrapper wrapperWithBlock:block];
    return [_resolveSocket connectToHost:host onPort:port error:err];
}


#pragma mark - CGDAsyncUdpSocket delegate methods

- (void)udpSocket:(GCDAsyncUdpSocket *)sock
   didReceiveData:(NSData *)data
      fromAddress:(NSData *)address
withFilterContext:(id)filterContext {
    ((S *)_sock)->onMessage([NSDate timeIntervalSinceReferenceDate],
                            sock, [sock localPort], address,
                            data, /*offset:*/0, [data length]);
}

- (void)udpSocket:(GCDAsyncUdpSocket *)sock didNotSendDataWithTag:(long)tag dueToError:(NSError *)error {
    // NSLog(@"Failed to send packet!");
}

// We use the connect functionality of GCDAsyncUdpSocket only to get access to the DNS
// resolve functions.
- (void)udpSocket:(GCDAsyncUdpSocket *)sock didConnectToAddress:(NSData *)address {
    EmiConnectionOpenedBlock block = ((EmiConnectionOpenedBlockWrapper *)sock.userData).block;
    
    _resolveSocket = nil;
    
    NSError *err;
    if (![self connectToAddress:address block:block error:&err]) {
        block(err, nil);
    }
    else {
        // connectToAddress:block:error: returned YES, so we rely on it invoking the block
    }
}

// We use the connect functionality of GCDAsyncUdpSocket only to get access to the DNS
// resolve functions.
- (void)udpSocket:(GCDAsyncUdpSocket *)sock didNotConnect:(NSError *)error {
    EmiConnectionOpenedBlock block = sock.userData;
    
    _resolveSocket = nil;
    
    block(error, nil);
}


#pragma mark - Getters

- (NSData *)serverAddress {
    return ((S *)_sock)->config.address;
}

- (EmiTimeInterval)connectionTimeout {
    return ((S *)_sock)->config.connectionTimeout;
}

- (float)heartbeatsBeforeConnectionWarning {
    return ((S *)_sock)->config.heartbeatsBeforeConnectionWarning;
}

- (NSUInteger)receiverBufferSize {
    return ((S *)_sock)->config.receiverBufferSize;
}

- (NSUInteger)senderBufferSize {
    return ((S *)_sock)->config.senderBufferSize;
}

- (BOOL)acceptConnections {
    return ((S *)_sock)->config.acceptConnections;
}

- (uint16_t)serverPort {
    return ((S *)_sock)->config.port;
}

- (NSUInteger)MTU {
    return ((S *)_sock)->config.mtu;
}

- (float)tickFrequency {
    return ((S *)_sock)->config.tickFrequency;
}

- (float)heartbeatFrequency {
    return ((S *)_sock)->config.heartbeatFrequency;
}

- (S *)sock {
    return (S *)_sock;
}

@end
