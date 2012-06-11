//
//  EmiConnection.h
//  roshambo
//
//  Created by Per Eckerdal on 2012-02-16.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#include "EmiTypes.h"

#import <Foundation/Foundation.h>

@class EmiSocket;
@class EmiConnection;

// err is nil on no error
typedef void (^EmiConnectionSendFinishedBlock)(NSError *err);

@protocol EmiConnectionDelegate <NSObject>
- (void)emiConnectionLost:(EmiConnection *)connection;
- (void)emiConnectionRegained:(EmiConnection *)connection;
- (void)emiConnectionDisconnect:(EmiConnection *)connection forReason:(EmiDisconnectReason)reason;
- (void)emiConnectionMessage:(EmiConnection *)connection
            channelQualifier:(EmiChannelQualifier)channelQualifier
                        data:(NSData *)data;

@optional
- (void)emiP2PConnectionEstablished:(EmiConnection *)connection;
- (void)emiP2PConnectionNotEstablished:(EmiConnection *)connection;

@end

@interface EmiConnection : NSObject {
    EmiSocket *_emiSocket;
    
    void *_ec;
    
	dispatch_queue_t _delegateQueue;
    id<EmiConnectionDelegate> __unsafe_unretained _delegate;
	dispatch_queue_t _connectionQueue;
}

- (BOOL)closeWithError:(NSError **)errPtr;
// Immediately closes the connection without notifying the other host.
//
// Note that if this method is used, there is no guarantee that you can
// reconnect to the remote host immediately; this host immediately forgets
// that it has been connected, but the other host does not. When
// reconnecting, this host's OS might pick the same inbound port, and that
// will confuse the remote host so the connection won't be established.
- (void)forceClose;
// Attempts to close the connection. If the normal close operation fails,
// force close.
- (void)close;

// Convenience alias for sendWithData:channelQualifier:priority:error:
- (BOOL)send:(NSData *)data error:(NSError **)errPtr;
// Convenience alias for sendWithData:channelQualifier:priority:error:
- (BOOL)send:(NSData *)data channelQualifier:(EmiChannelQualifier)channelQualifier error:(NSError **)errPtr;
// Convenience alias for sendWithData:channelQualifier:priority:error:
- (BOOL)send:(NSData *)data priority:(EmiPriority)priority error:(NSError **)errPtr;
// Synchronous send
- (BOOL)send:(NSData *)data channelQualifier:(EmiChannelQualifier)channelQualifier
    priority:(EmiPriority)priority error:(NSError **)errPtr;

// Convenience alias for sendWithData:channelQualifier:priority:finished:
- (void)send:(NSData *)data finished:(EmiConnectionSendFinishedBlock)block;
// Convenience alias for sendWithData:channelQualifier:priority:finished:
- (void)send:(NSData *)data channelQualifier:(EmiChannelQualifier)channelQualifier
    finished:(EmiConnectionSendFinishedBlock)block;
// Convenience alias for sendWithData:channelQualifier:priority:finished:
- (void)send:(NSData *)data priority:(EmiPriority)priority
    finished:(EmiConnectionSendFinishedBlock)block;
// Asynchronous send. When the send operation has returned (NOTE: This happens
// before the actual message is sent to the network!), invokes block on the same
// queue as the method was invoked on.
- (void)send:(NSData *)data channelQualifier:(EmiChannelQualifier)channelQualifier
    priority:(EmiPriority)priority finished:(EmiConnectionSendFinishedBlock)block;


@property (nonatomic, unsafe_unretained) id<EmiConnectionDelegate> delegate;
@property (nonatomic, assign) dispatch_queue_t delegateQueue;
@property (nonatomic, readonly, assign) dispatch_queue_t connectionQueue;

@property (nonatomic, readonly, assign) BOOL issuedConnectionWarning;
@property (nonatomic, readonly, unsafe_unretained) EmiSocket *emiSocket;
@property (nonatomic, readonly, unsafe_unretained) NSData *localAddress;
@property (nonatomic, readonly, unsafe_unretained) NSData *remoteAddress;
@property (nonatomic, readonly, assign) uint16_t inboundPort;
@property (nonatomic, readonly, assign) BOOL open;
@property (nonatomic, readonly, assign) BOOL opening;
@property (nonatomic, readonly, assign) EmiP2PState p2pState;

@end
