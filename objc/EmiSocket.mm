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
#include "EmiSendQueue.h"
#include "EmiConnDelegate.h"

#import "GCDAsyncUdpSocket.h"
#include <map>
#include <set>

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

#pragma mark - Helper classes

@interface EmiConnectionOpenedBlockWrapper : NSObject {
    EmiConnectionOpenedBlock _block;
    dispatch_queue_t _blockQueue;
    NSData *_cookie;
    NSData *_sharedSecret;
}

- (id)initWithBlock:(EmiConnectionOpenedBlock)block
         blockQueue:(dispatch_queue_t)blockQueue
             cookie:(NSData *)cookie
       sharedSecret:(NSData *)sharedSecret;

+ (EmiConnectionOpenedBlockWrapper *)wrapperWithBlock:(EmiConnectionOpenedBlock)block
                                           blockQueue:(dispatch_queue_t)blockQueue
                                               cookie:(NSData *)cookie
                                         sharedSecret:(NSData *)sharedSecret;

@property (nonatomic, retain, readonly) EmiConnectionOpenedBlock block;
@property (nonatomic, assign, readonly) dispatch_queue_t blockQueue;
@property (nonatomic, retain, readonly) NSData *cookie;
@property (nonatomic, retain, readonly) NSData *sharedSecret;

@end

@implementation EmiConnectionOpenedBlockWrapper

@synthesize block = _block;
@synthesize blockQueue = _blockQueue;
@synthesize cookie = _cookie;
@synthesize sharedSecret = _sharedSecret;

- (id)initWithBlock:(EmiConnectionOpenedBlock)block
         blockQueue:(dispatch_queue_t)blockQueue
             cookie:(NSData *)cookie
       sharedSecret:(NSData *)sharedSecret {
    if (self = [super init]) {
        _block = [block copy];
        _blockQueue = blockQueue;
        if (_blockQueue) {
            dispatch_retain(_blockQueue);
        }
        _cookie = cookie;
        _sharedSecret = sharedSecret;
    }
    
    return self;
}

- (void)dealloc {
    if (_blockQueue) {
        dispatch_release(_blockQueue);
    }
}

+ (EmiConnectionOpenedBlockWrapper *)wrapperWithBlock:(EmiConnectionOpenedBlock)block
                                           blockQueue:(dispatch_queue_t)blockQueue
                                               cookie:(NSData *)cookie
                                         sharedSecret:(NSData *)sharedSecret {
    return [[EmiConnectionOpenedBlockWrapper alloc] initWithBlock:block
                                                       blockQueue:blockQueue
                                                           cookie:cookie
                                                     sharedSecret:sharedSecret];
}

@end


#pragma mark - EmiSocket implementation

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
    self.delegate = nil;
    if (_delegateQueue) {
        dispatch_release(_delegateQueue);
        _delegateQueue = NULL;
    }
    
    if (_socketQueue) {
        dispatch_release(_socketQueue);
        _socketQueue = NULL;
    }
    
    if (_sock) {
        delete (S *)_sock;
        _sock = NULL;
    }
    
    _delegate = nil;
}

- (BOOL)connectToAddress:(NSData *)address
                  cookie:(NSData *)cookie
            sharedSecret:(NSData *)sharedSecret
                   block:(EmiConnectionOpenedBlock)block
              blockQueue:(dispatch_queue_t)blockQueue
                   error:(NSError **)errPtr {
    if ((!cookie || !sharedSecret) &&
        ( cookie ||  sharedSecret)) {
        [NSException raise:@"EmiSocketException" format:@"Invalid arguments"];
    }
    
    __block EmiConnectionOpenedBlock unwrappedBlock = block;
    if (blockQueue) {
        block = ^(NSError *err, EmiConnection *connection) {
            DISPATCH_ASYNC(blockQueue, ^{
                unwrappedBlock(err, connection);
            });
        };
    }
    
    sockaddr_storage ss;
    memcpy(&ss, [address bytes], MIN([address length], sizeof(sockaddr_storage)));
    
    __block NSError *err = nil;
    __block BOOL retVal;
    
    DISPATCH_SYNC(_socketQueue, ^{
        retVal = ((S *)_sock)->connect([NSDate timeIntervalSinceReferenceDate], ss,
                                       (const uint8_t *)[cookie bytes], [cookie length],
                                       (const uint8_t *)[sharedSecret bytes], [sharedSecret length],
                                       block, err);
        *errPtr = err;
    });
    
    return retVal;
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
                   block:(EmiConnectionOpenedBlock)block
                   error:(NSError **)errPtr {
    return [self connectToAddress:address
                           cookie:nil sharedSecret:nil
                            block:block blockQueue:dispatch_get_current_queue()
                            error:errPtr];
}

- (BOOL)connectToAddress:(NSData *)address
                  cookie:(NSData *)cookie
            sharedSecret:(NSData *)sharedSecret
                   block:(EmiConnectionOpenedBlock)block
                   error:(NSError **)errPtr {
    return [self connectToAddress:address
                           cookie:cookie sharedSecret:sharedSecret
                            block:block blockQueue:dispatch_get_current_queue()
                            error:errPtr];
}

- (BOOL)connectToHost:(NSString *)host
               onPort:(uint16_t)port
                block:(EmiConnectionOpenedBlock)block
                error:(NSError **)err {
    return [self connectToHost:host onPort:port
                        cookie:nil sharedSecret:nil
                         block:block error:err];
}

- (BOOL)connectToHost:(NSString *)host
               onPort:(uint16_t)port
               cookie:(NSData *)cookie
         sharedSecret:(NSData *)sharedSecret
                block:(EmiConnectionOpenedBlock)block
                error:(NSError **)err {
    if ((!cookie || !sharedSecret) &&
        ( cookie ||  sharedSecret)) {
        [NSException raise:@"EmiSocketException" format:@"Invalid arguments"];
    }
    
    __block BOOL ret;
    
    dispatch_queue_t blockQueue = dispatch_get_current_queue();
    
	DISPATCH_SYNC(_socketQueue, ^{
        // We use the connect functionality of GCDAsyncUdpSocket only to get access to the DNS
        // resolve functions.
        _resolveSocket = [[GCDAsyncUdpSocket alloc] initWithDelegate:self
                                                       delegateQueue:_socketQueue
                                                         socketQueue:_socketQueue];
        _resolveSocket.userData = [EmiConnectionOpenedBlockWrapper wrapperWithBlock:block
                                                                         blockQueue:blockQueue
                                                                             cookie:cookie
                                                                       sharedSecret:sharedSecret];
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
    
    (wrap.callback)(sock,
                    wrap.userData,
                    [NSDate timeIntervalSinceReferenceDate],
                    ss,
                    data, /*offset:*/0, [data length]);
}

+ (void)udpSocket:(GCDAsyncUdpSocket *)sock didNotSendDataWithTag:(long)tag dueToError:(NSError *)error {
    // NSLog(@"Failed to send packet!");
}

// We use the connect functionality of GCDAsyncUdpSocket only to get access to the DNS
// resolve functions.
- (void)udpSocket:(GCDAsyncUdpSocket *)sock didConnectToAddress:(NSData *)address {
    ASSERT(dispatch_get_current_queue() == _socketQueue);
    
    EmiConnectionOpenedBlockWrapper *wrapper = sock.userData;
    EmiConnectionOpenedBlock block = wrapper.block;
    
    _resolveSocket = nil;
    
    NSError *err;
    
    BOOL result;
    if (wrapper.cookie && wrapper.sharedSecret) {
        result = [self connectToAddress:address
                                 cookie:wrapper.cookie
                           sharedSecret:wrapper.sharedSecret
                                  block:block
                                  error:&err];
    }
    else {
        result = [self connectToAddress:address block:block error:&err];
    }
    
    if (!result) {
        DISPATCH_ASYNC(wrapper.blockQueue, ^{
            block(err, nil);
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
    EmiConnectionOpenedBlock block = wrapper.block;
    
    _resolveSocket = nil;
    
    DISPATCH_ASYNC(wrapper.blockQueue, ^{
        block(error, nil);
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

- (void)setDelegate:(id<EmiSocketDelegate>)delegate {
	DISPATCH_SYNC(_socketQueue, ^{
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

- (void)setDelegateQueue:(dispatch_queue_t)delegateQueue {
	DISPATCH_SYNC(_socketQueue, ^{
		if (_delegateQueue) {
			dispatch_release(_delegateQueue);
        }
		
		if (delegateQueue) {
			dispatch_retain(delegateQueue);
        }
		
		_delegateQueue = delegateQueue;
	});
}

- (dispatch_queue_t)socketQueue {
    return _socketQueue;
}

@end
