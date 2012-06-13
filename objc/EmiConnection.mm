//
//  EmiConnection.m
//  roshambo
//
//  Created by Per Eckerdal on 2012-02-16.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#import "EmiConnectionInternal.h"

#include "EmiConnDelegate.h"
#import "EmiSocketInternal.h"
#import "GCDAsyncUdpSocket.h"
#include "EmiObjCBindingHelper.h"

@implementation EmiConnection

- (id)initWithSocket:(EmiSocket *)socket
     connectionQueue:(dispatch_queue_t)connectionQueue
              params:(const EmiConnParams<EmiBinding> *)params {
    if (self = [super init]) {
        _emiSocket = socket;
        
        if (connectionQueue) {
            NSAssert(connectionQueue != dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_LOW, 0),
                     @"The given socketQueue parameter must not be a concurrent queue.");
            NSAssert(connectionQueue != dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_HIGH, 0),
                     @"The given socketQueue parameter must not be a concurrent queue.");
            NSAssert(connectionQueue != dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0),
                     @"The given socketQueue parameter must not be a concurrent queue.");
            
            dispatch_retain(connectionQueue);
            _connectionQueue = connectionQueue;
        }
        else {
            _connectionQueue = dispatch_queue_create("EmiConnection", NULL);
        }
        
        _delegateQueue = NULL;
        
        _ec = new EC(EmiConnDelegate(self), socket.sock->config, *params);
    }
    
    return self;
}

- (void)dealloc {
	if (_delegateQueue) {
		dispatch_release(_delegateQueue);
    }
    
    dispatch_queue_t connQueue = _connectionQueue;
    EC *ec = ((EC *)_ec);
    
    dispatch_async(connQueue, ^{
        dispatch_release(connQueue);
        
        ec->getDelegate().invalidate();
        delete (EC *)ec;
    });
}

- (EmiTimeInterval)_now {
    return [NSDate timeIntervalSinceReferenceDate];
}

#define SYNC_RETURN(type, expr)                                 \
    do {                                                        \
        __block type SYNC_RETURN__result;                       \
        DISPATCH_SYNC(_connectionQueue, ^{                      \
            SYNC_RETURN__result = (expr);                       \
        });                                                     \
        return SYNC_RETURN__result;                             \
    } while (0)

- (BOOL)issuedConnectionWarning {
    SYNC_RETURN(BOOL, ((EC *)_ec)->issuedConnectionWarning());
}

- (EmiSocket *)emiSocket {
    return _emiSocket;
}

- (NSData *)localAddress {
    const sockaddr_storage& ss(((EC *)_ec)->getLocalAddress());
    SYNC_RETURN(NSData*, [NSData dataWithBytes:&ss length:EmiNetUtil::addrSize(ss)]);
}

- (NSData *)remoteAddress {
    const sockaddr_storage& ss(((EC *)_ec)->getRemoteAddress());
    SYNC_RETURN(NSData*, [NSData dataWithBytes:&ss length:EmiNetUtil::addrSize(ss)]);
}

- (uint16_t)inboundPort {
    SYNC_RETURN(uint16_t, ((EC *)_ec)->getInboundPort());
}

- (BOOL)closeWithError:(NSError **)errPtr {
    NSError *err;
    BOOL retVal = ((EC *)_ec)->close([self _now], err);
    *errPtr = err;
    
    return retVal;
}

- (void)forceClose {
    DISPATCH_SYNC(_connectionQueue, ^{
        ((EC *)_ec)->forceClose();
    });
}

- (void)close {
    DISPATCH_SYNC(_connectionQueue, ^{
        NSError *err;
        if (![self closeWithError:&err]) {
            [self forceClose];
        }
    });
}

- (BOOL)send:(NSData *)data error:(NSError **)errPtr {
    return [self send:data channelQualifier:EMI_CHANNEL_QUALIFIER_DEFAULT priority:EMI_PRIORITY_DEFAULT error:errPtr];
}

- (BOOL)send:(NSData *)data channelQualifier:(EmiChannelQualifier)channelQualifier error:(NSError **)errPtr {
    return [self send:data channelQualifier:channelQualifier priority:EMI_PRIORITY_DEFAULT error:errPtr];
}

- (BOOL)send:(NSData *)data priority:(EmiPriority)priority error:(NSError **)errPtr {
    return [self send:data channelQualifier:EMI_CHANNEL_QUALIFIER_DEFAULT priority:priority error:errPtr];
}

- (BOOL)send:(NSData *)data channelQualifier:(EmiChannelQualifier)channelQualifier priority:(EmiPriority)priority error:(NSError **)errPtr {
    __block NSError *err;
    __block BOOL retVal;
    
    DISPATCH_SYNC(_connectionQueue, ^{
        retVal = ((EC *)_ec)->send([self _now], data, channelQualifier, priority, err);
        *errPtr = err;
    });
    
    return retVal;
}

- (void)send:(NSData *)data finished:(EmiConnectionSendFinishedBlock)block {
    [self send:data channelQualifier:EMI_CHANNEL_QUALIFIER_DEFAULT priority:EMI_PRIORITY_DEFAULT finished:block];
}

- (void)send:(NSData *)data channelQualifier:(EmiChannelQualifier)channelQualifier
    finished:(EmiConnectionSendFinishedBlock)block {
    [self send:data channelQualifier:channelQualifier priority:EMI_PRIORITY_DEFAULT finished:block];
}

- (void)send:(NSData *)data priority:(EmiPriority)priority
    finished:(EmiConnectionSendFinishedBlock)block {
    [self send:data channelQualifier:EMI_CHANNEL_QUALIFIER_DEFAULT priority:priority finished:block];
}

- (void)send:(NSData *)data channelQualifier:(EmiChannelQualifier)channelQualifier
    priority:(EmiPriority)priority finished:(EmiConnectionSendFinishedBlock)block {
    
    dispatch_queue_t blockQueue = (block ? dispatch_get_current_queue() : NULL);
    if (blockQueue) {
        dispatch_retain(blockQueue);
    }
    
    DISPATCH_ASYNC(_connectionQueue, ^{
        NSError *err;
        BOOL retVal = ((EC *)_ec)->send([self _now], data, channelQualifier, priority, err);
        
        if (block) {
            dispatch_async(blockQueue, ^{
                block(retVal ? nil : err);
            });
        }
        
        if (blockQueue) {
            dispatch_release(blockQueue);
        }
    });
}

- (BOOL)open {
    SYNC_RETURN(BOOL, ((EC *)_ec)->isOpen());
}

- (BOOL)opening {
    SYNC_RETURN(BOOL, ((EC *)_ec)->isOpening());
}

- (EmiP2PState)p2pState {
    SYNC_RETURN(EmiP2PState, ((EC *)_ec)->getP2PState());
}

- (EmiConnectionType)type {
    SYNC_RETURN(EmiConnectionType, ((EC *)_ec)->getType());
}

- (EC *)conn {
    return (EC *)_ec;
}

#pragma mark - Properties

- (id<EmiConnectionDelegate>)delegate {
    __block id<EmiConnectionDelegate> result;
    
    DISPATCH_SYNC(_connectionQueue, ^{
        result = _delegate;
    });
    
    return result;
}

- (void)setDelegate:(id<EmiConnectionDelegate>)delegate {
	DISPATCH_SYNC(_connectionQueue, ^{
        ((EC *)_ec)->getDelegate().waitForDelegateBlocks();
        _delegate = delegate;
	});
}

- (dispatch_queue_t)delegateQueue {
    __block dispatch_queue_t result;
    
    DISPATCH_SYNC(_connectionQueue, ^{
        result = _delegateQueue;
    });
    
    return result;
}

- (void)setDelegateQueue:(dispatch_queue_t)delegateQueue {
	DISPATCH_SYNC(_connectionQueue, ^{
		if (_delegateQueue) {
            ((EC *)_ec)->getDelegate().waitForDelegateBlocks();
			dispatch_release(_delegateQueue);
        }
		
		if (delegateQueue) {
			dispatch_retain(delegateQueue);
        }
		
		_delegateQueue = delegateQueue;
	});
}

- (dispatch_queue_t)connectionQueue {
    return _connectionQueue;
}

@end
