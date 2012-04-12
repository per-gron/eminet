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
    
    id<EmiSocketDelegate> __unsafe_unretained _delegate;
}

- (id)init;
// Returns YES on success
- (BOOL)startWithError:(NSError **)error;
// Returns YES on success
- (BOOL)startWithConfig:(EmiSocketConfig *)config error:(NSError **)error;

- (void)suspend;
// Returns YES on success
- (BOOL)desuspendWithError:(NSError **)err;

- (BOOL)connectToAddress:(NSData *)address block:(EmiConnectionOpenedBlock)block error:(NSError **)err;
/**
 * If an obvious error is detected, this method immediately returns NO and sets err.
 * If you don't care about the error, you can pass nil for errPtr.
 * Otherwise, this method returns YES and begins the asynchronous connection process.
 **/
- (BOOL)connectToHost:(NSString *)host onPort:(uint16_t)port block:(EmiConnectionOpenedBlock)block error:(NSError **)err;

@property (nonatomic, unsafe_unretained) id<EmiSocketDelegate> delegate;
@property (nonatomic, readonly, strong) NSData *serverAddress;
@property (nonatomic, readonly, assign) BOOL open;
@property (nonatomic, readonly, assign) EmiTimeInterval connectionTimeout;
@property (nonatomic, readonly, assign) float heartbeatsBeforeConnectionWarning;
@property (nonatomic, readonly, assign) NSUInteger receiverBufferSize;
@property (nonatomic, readonly, assign) NSUInteger senderBufferSize;
@property (nonatomic, readonly, assign) BOOL acceptConnections;
@property (nonatomic, readonly, assign) uint16_t serverPort;
@property (nonatomic, readonly, assign) NSUInteger MTU;
@property (nonatomic, readonly, assign) float tickFrequency;
@property (nonatomic, readonly, assign) float heartbeatFrequency;

@end
