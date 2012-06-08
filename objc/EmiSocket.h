//
//  EmiSocket.h
//  roshambo
//
//  Created by Per Eckerdal on 2012-02-16.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#include "EmiTypes.h"

#import <Foundation/Foundation.h>

@class EmiConnection;
@class GCDAsyncUdpSocket;
@class EmiSocketConfig;

typedef void (^EmiConnectionOpenedBlock)(NSError *err, EmiConnection *connection);

@protocol EmiSocketDelegate <NSObject>
- (void)emiSocketGotConnection:(EmiConnection *)connection;
@end

@interface EmiSocket : NSObject {
    GCDAsyncUdpSocket *_resolveSocket;
    
    void *_sock;
    
	dispatch_queue_t _delegateQueue;
    id<EmiSocketDelegate> __unsafe_unretained _delegate;
	dispatch_queue_t _socketQueue;
}

// EmiNet uses the standard delegate paradigm, but executes all delegate
// callbacks on a given delegate dispatch queue. This allows for maximum
// concurrency, while at the same time providing easy thread safety.
// 
// The socket queue is optional.
// If you pass NULL, EmiNet will automatically create its own socket queue.
// If you choose to provide a socket queue, the socket queue must not be a
// concurrent queue.
// 
// The delegate queue and socket queue can optionally be the same.
- (id)init;
- (id)initWithSocketQueue:(dispatch_queue_t)sq;
- (id)initWithDelegate:(id<EmiSocketDelegate>)delegate delegateQueue:(dispatch_queue_t)dq;
- (id)initWithDelegate:(id<EmiSocketDelegate>)delegate delegateQueue:(dispatch_queue_t)dq socketQueue:(dispatch_queue_t)sq;

// Returns YES on success
- (BOOL)startWithError:(NSError **)error;
// Returns YES on success
- (BOOL)startWithConfig:(EmiSocketConfig *)config error:(NSError **)error;

// block is invoked in the same GCD queue as the method is called
- (BOOL)connectToAddress:(NSData *)address
                   block:(EmiConnectionOpenedBlock)block
                   error:(NSError **)errPtr;
// P2P connect.
//
// block is invoked in the same GCD queue as the method is called
- (BOOL)connectToAddress:(NSData *)address
                  cookie:(NSData *)cookie
            sharedSecret:(NSData *)sharedSecret
                   block:(EmiConnectionOpenedBlock)block
                   error:(NSError **)errPtr;
// If an obvious error is detected, this method immediately returns NO and sets err.
// If you don't care about the error, you can pass nil for errPtr.
// Otherwise, this method returns YES and begins the asynchronous connection process.
//
// block is invoked in the same GCD queue as the method is called
- (BOOL)connectToHost:(NSString *)host
               onPort:(uint16_t)port
                block:(EmiConnectionOpenedBlock)block
                error:(NSError **)errPtr;
// P2P connect.
// 
// If an obvious error is detected, this method immediately returns NO and sets err.
// If you don't care about the error, you can pass nil for errPtr.
// Otherwise, this method returns YES and begins the asynchronous connection process.
//
// block is invoked in the same GCD queue as the method is called
- (BOOL)connectToHost:(NSString *)host
               onPort:(uint16_t)port
               cookie:(NSData *)cookie
         sharedSecret:(NSData *)sharedSecret
                block:(EmiConnectionOpenedBlock)block
                error:(NSError **)errPtr;

@property (nonatomic, unsafe_unretained) id<EmiSocketDelegate> delegate;
@property (nonatomic, assign) dispatch_queue_t delegateQueue;
@property (nonatomic, readonly, strong) NSData *serverAddress;
@property (nonatomic, readonly, assign) EmiTimeInterval connectionTimeout;
@property (nonatomic, readonly, assign) float heartbeatsBeforeConnectionWarning;
@property (nonatomic, readonly, assign) NSUInteger receiverBufferSize;
@property (nonatomic, readonly, assign) NSUInteger senderBufferSize;
@property (nonatomic, readonly, assign) BOOL acceptConnections;
@property (nonatomic, readonly, assign) uint16_t serverPort;
@property (nonatomic, readonly, assign) NSUInteger MTU;
@property (nonatomic, readonly, assign) float heartbeatFrequency;

@end
