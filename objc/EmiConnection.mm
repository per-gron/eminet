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

@implementation EmiConnection

@synthesize delegate = _delegate;

- (id)initWithSocket:(EmiSocket *)socket
             params:(const EmiConnParams<NSData *> *)params {
    if (self = [super init]) {
        _emiSocket = socket;
        _ec = new EC(EmiConnDelegate(self), *socket.sock, *params);
    }
    
    return self;
}

- (void)dealloc {
    // Just to be sure, since the ivar is __unsafe_unretained
    _delegate = nil;
    
    ((EC *)_ec)->getDelegate().invalidate();
    delete (EC *)_ec;
}

- (EmiTimeInterval)_now {
    return [NSDate timeIntervalSinceReferenceDate];
}

- (BOOL)issuedConnectionWarning {
    return ((EC *)_ec)->issuedConnectionWarning();
}

- (void)_resetConnectionTimeout {
    ((EC *)_ec)->resetConnectionTimeout();
}

- (EmiSocket *)emiSocket {
    return _emiSocket;
}

- (NSData *)address {
    return ((EC *)_ec)->getAddress();
}

- (uint16_t)inboundPort {
    return ((EC *)_ec)->getInboundPort();
}

- (BOOL)closeWithError:(NSError **)errPtr {
    NSError *err;
    BOOL retVal = ((EC *)_ec)->close([self _now], err);
    *errPtr = err;
    
    return retVal;
}

- (void)forceClose {
    ((EC *)_ec)->forceClose();
}

- (void)close {
    NSError *err;
    if (![self closeWithError:&err]) {
        [self forceClose];
    }
}

- (BOOL)flush {
    return ((EC *)_ec)->flush([self _now]);
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
    NSError *err;
    BOOL retVal = ((EC *)_ec)->send([self _now], data, channelQualifier, priority, err);
    *errPtr = err;
    
    return retVal;
}

- (BOOL)open {
    return ((EC *)_ec)->isOpen();
}

- (BOOL)opening {
    return ((EC *)_ec)->isOpening();
}

- (EC *)conn {
    return (EC *)_ec;
}

@end
