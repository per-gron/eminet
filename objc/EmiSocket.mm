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
#import "EmiSocketUserDataWrapper.h"
#import "EmiConnectionOpenedBlockWrapper.h"
#include "EmiSendQueue.h"
#include "EmiConnDelegate.h"
#include "EmiObjCBindingHelper.h"

#import "GCDAsyncUdpSocket.h"
#include <map>
#include <set>

@implementation EmiSocket

#pragma mark - Object lifecycle

- (id)init {
    return [self initWithDelegate:nil delegateQueue:NULL socketQueue:NULL];
}

- (id)initWithSocketQueue:(dispatch_queue_t)sq {
    return [self initWithDelegate:nil delegateQueue:NULL socketQueue:sq];
}

- (id)initWithDelegate:(id)delegate delegateQueue:(dispatch_queue_t)dq {
    return [self initWithDelegate:delegate delegateQueue:dq socketQueue:NULL];
}

- (id)initWithDelegate:(id)delegate delegateQueue:(dispatch_queue_t)dq socketQueue:(dispatch_queue_t)sq {
    if (self = [super init]) {
        _sock = NULL;
        
        _delegate = delegate;
        
        if (dq) {
            dispatch_retain(dq);
            _delegateQueue = dq;
        }
        else {
            _delegateQueue = NULL;
        }
        
        if (sq) {
            NSAssert(sq != dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_LOW, 0),
                     @"The given socketQueue parameter must not be a concurrent queue.");
            NSAssert(sq != dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_HIGH, 0),
                     @"The given socketQueue parameter must not be a concurrent queue.");
            NSAssert(sq != dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0),
                     @"The given socketQueue parameter must not be a concurrent queue.");
            
            dispatch_retain(sq);
            _socketQueue = sq;
        }
        else {
            _socketQueue = dispatch_queue_create("EmiSocket", NULL);
        }
    }
    return self;
}

- (void)dealloc {
    // This releases _delegateQueue
    [self setDelegate:nil delegateQueue:NULL];
    
    if (_socketQueue) {
        dispatch_release(_socketQueue);
        _socketQueue = NULL;
    }
    
    if (_sock) {
        delete (S *)_sock;
        _sock = NULL;
    }
}


#pragma mark - Public methods

- (BOOL)startWithError:(NSError **)error {
    return [self startWithConfig:[[EmiSocketConfig alloc] init] error:error];
}

- (BOOL)startWithConfig:(EmiSocketConfig *)config error:(NSError **)errPtr {
    __block NSError *err = nil;
    
    DISPATCH_SYNC(_socketQueue, ^{
        EmiSockConfig *sc = [config sockConfig];
        
        _sock = new S(*sc, EmiSockDelegate(self));
        
        ((S *)_sock)->open(err);
    });
    
    *errPtr = err;
    return !err;
}

- (BOOL)connectToAddress:(NSData *)address
                delegate:(__unsafe_unretained id<EmiConnectionDelegate>)delegate
           delegateQueue:(dispatch_queue_t)delegateQueue
                userData:(id)userData
                   error:(NSError **)errPtr {
    return [self connectToAddress:address
                           cookie:nil sharedSecret:nil
                         delegate:delegate delegateQueue:delegateQueue
                         userData:userData
                            error:errPtr];
}

- (BOOL)connectToAddress:(NSData *)address
                  cookie:(NSData *)cookie
            sharedSecret:(NSData *)sharedSecret
                delegate:(__unsafe_unretained id<EmiConnectionDelegate>)delegate
           delegateQueue:(dispatch_queue_t)delegateQueue
                userData:(id)userData
                   error:(NSError **)errPtr {
    if ((!cookie || !sharedSecret) &&
        ( cookie ||  sharedSecret)) {
        [NSException raise:@"EmiSocketException" format:@"Invalid arguments"];
    }
    
    EmiConnectionOpenedBlockWrapper *wrapper =
        [EmiConnectionOpenedBlockWrapper wrapperWithDelegate:delegate
                                               delegateQueue:delegateQueue
                                                      cookie:cookie
                                                sharedSecret:sharedSecret
                                                    userData:userData];
    
    sockaddr_storage ss;
    memcpy(&ss, [address bytes], MIN([address length], sizeof(sockaddr_storage)));
    
    __block NSError *err = nil;
    __block BOOL retVal;
    
    DISPATCH_SYNC(_socketQueue, ^{
        retVal = ((S *)_sock)->connect([NSDate timeIntervalSinceReferenceDate], ss,
                                       (const uint8_t *)[cookie bytes], [cookie length],
                                       (const uint8_t *)[sharedSecret bytes], [sharedSecret length],
                                       wrapper, err);
        *errPtr = err;
    });
    
    return retVal;
}

- (BOOL)connectToHost:(NSString *)host
               onPort:(uint16_t)port
             delegate:(__unsafe_unretained id<EmiConnectionDelegate>)delegate
        delegateQueue:(dispatch_queue_t)delegateQueue
             userData:(id)userData
                error:(NSError **)err {
    return [self connectToHost:host onPort:port
                        cookie:nil sharedSecret:nil
                      delegate:delegate delegateQueue:delegateQueue
                      userData:userData
                         error:err];
}

- (BOOL)connectToHost:(NSString *)host
               onPort:(uint16_t)port
               cookie:(NSData *)cookie
         sharedSecret:(NSData *)sharedSecret
             delegate:(__unsafe_unretained id<EmiConnectionDelegate>)delegate
        delegateQueue:(dispatch_queue_t)delegateQueue
             userData:(id)userData
                error:(NSError **)err {
    if ((!cookie || !sharedSecret) &&
        ( cookie ||  sharedSecret)) {
        [NSException raise:@"EmiSocketException" format:@"Invalid arguments"];
    }
    
    __block BOOL ret;
    
	DISPATCH_SYNC(_socketQueue, ^{
        // We use the connect functionality of GCDAsyncUdpSocket only to get access to the DNS
        // resolve functions.
        _resolveSocket = [[GCDAsyncUdpSocket alloc] initWithDelegate:self
                                                       delegateQueue:_socketQueue
                                                         socketQueue:_socketQueue];
        _resolveSocket.userData = [EmiConnectionOpenedBlockWrapper wrapperWithDelegate:delegate
                                                                         delegateQueue:delegateQueue
                                                                                cookie:cookie
                                                                          sharedSecret:sharedSecret
                                                                              userData:userData];
        ret = [_resolveSocket connectToHost:host onPort:port error:err];
	});
    
    return ret;
}


#pragma mark - CGDAsyncUdpSocket delegate methods

+ (void)udpSocket:(GCDAsyncUdpSocket *)sock
   didReceiveData:(NSData *)data
      fromAddress:(NSData *)address
withFilterContext:(id)filterContext {
    sockaddr_storage ss;
    memcpy(&ss, [address bytes], MIN([address length], sizeof(ss)));
    
    EmiSocketUserDataWrapper *wrap = sock.userData;
    if (wrap) {
        (wrap.callback)(sock,
                        wrap.userData,
                        [NSDate timeIntervalSinceReferenceDate],
                        ss,
                        data, /*offset:*/0, [data length]);
    }
}

+ (void)udpSocket:(GCDAsyncUdpSocket *)sock didNotSendDataWithTag:(long)tag dueToError:(NSError *)error {
    // NSLog(@"Failed to send packet!");
}

// We use the connect functionality of GCDAsyncUdpSocket only to get access to the DNS
// resolve functions.
- (void)udpSocket:(GCDAsyncUdpSocket *)sock didConnectToAddress:(NSData *)address {
    ASSERT(dispatch_get_current_queue() == _socketQueue);
    
    EmiConnectionOpenedBlockWrapper *wrapper = sock.userData;
    
    _resolveSocket = nil;
    
    NSError *err;
    
    BOOL result;
    if (wrapper.cookie && wrapper.sharedSecret) {
        result = [self connectToAddress:address
                                 cookie:wrapper.cookie
                           sharedSecret:wrapper.sharedSecret
                               delegate:wrapper.delegate delegateQueue:wrapper.delegateQueue
                               userData:wrapper.userData
                                  error:&err];
    }
    else {
        result = [self connectToAddress:address
                                 cookie:nil sharedSecret:nil
                               delegate:wrapper.delegate delegateQueue:wrapper.delegateQueue
                               userData:wrapper.userData
                                  error:&err];
    }
    
    if (!result) {
        DISPATCH_ASYNC(wrapper.delegateQueue, ^{
            [wrapper.delegate emiConnectionFailedToConnect:self error:err userData:wrapper.userData];
        });
    }
    else {
        // connectToAddress:block:error: returned YES, so we rely on it invoking the block
    }
}

// We use the connect functionality of GCDAsyncUdpSocket only to get access to the DNS
// resolve functions.
- (void)udpSocket:(GCDAsyncUdpSocket *)sock didNotConnect:(NSError *)error {
    ASSERT(dispatch_get_current_queue() == _socketQueue);
    
    EmiConnectionOpenedBlockWrapper *wrapper = sock.userData;
    
    _resolveSocket = nil;
    
    DISPATCH_ASYNC(wrapper.delegateQueue, ^{
        [wrapper.delegate emiConnectionFailedToConnect:self error:error userData:wrapper.userData];
    });
}


#pragma mark - Getters

- (NSData *)serverAddress {
    const sockaddr_storage& ss(((S *)_sock)->config.address);
    return [NSData dataWithBytes:&ss length:EmiNetUtil::addrSize(ss)];
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

- (float)heartbeatFrequency {
    return ((S *)_sock)->config.heartbeatFrequency;
}

- (S *)sock {
    return (S *)_sock;
}

#pragma mark - Properties

- (id<EmiSocketDelegate>)delegate {
    __block id<EmiSocketDelegate> result;
    
    DISPATCH_SYNC(_socketQueue, ^{
        result = _delegate;
    });
    
    return result;
}

- (void)setDelegate:(id<EmiSocketDelegate>)delegate delegateQueue:(dispatch_queue_t)delegateQueue {
	DISPATCH_SYNC(_socketQueue, ^{
		if (_delegateQueue) {
			dispatch_release(_delegateQueue);
        }
		
		if (delegateQueue) {
			dispatch_retain(delegateQueue);
        }
		
		_delegateQueue = delegateQueue;
        _delegate = delegate;
	});
}

- (dispatch_queue_t)delegateQueue {
    __block dispatch_queue_t result;
    
    DISPATCH_SYNC(_socketQueue, ^{
        result = _delegateQueue;
    });
    
    return result;
}

- (dispatch_queue_t)socketQueue {
    return _socketQueue;
}

@end
