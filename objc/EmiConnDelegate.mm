//
//  EmiConnDelegate.mm
//  roshambo
//
//  Created by Per Eckerdal on 2012-04-11.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#import "EmiConnDelegate.h"

#import "EmiSocketInternal.h"
#import "EmiConnectionInternal.h"

EmiConnDelegate::EmiConnDelegate(EmiConnection *conn) :
_conn(conn),
_queueWrapper([[EmiDispatchQueueWrapper alloc] initWithQueue:conn.connectionQueue]) {}

void EmiConnDelegate::invalidate() {
    if (!_conn) {
        // invalidate has already been called
        return;
    }
    
    if (EMI_CONNECTION_TYPE_SERVER == _conn.conn->getType()) {
        dispatch_sync(_conn.emiSocket.socketQueue, ^{
            _conn.emiSocket.sock->deregisterServerConnection(_conn.conn);
        });
    }
    
    // Just to be sure, since the ivar is __unsafe_unretained
    // Note that this code would be incorrect if connections supported reconnecting; It's correct only because after a forceClose, the delegate will never be called again.
    _conn.delegate = nil;
    
    // Because this is a strong reference, we need to nil it out to let ARC deallocate this connection for us
    _conn = nil;
}

void EmiConnDelegate::emiConnMessage(EmiChannelQualifier channelQualifier, NSData *data, NSUInteger offset, NSUInteger size) {
    id<EmiConnectionDelegate> connDelegate = _conn.delegate;
    dispatch_queue_t connDelegateQueue = _conn.delegateQueue;
    
    if (connDelegateQueue) {
        dispatch_async(connDelegateQueue, ^{
            [connDelegate emiConnectionMessage:_conn
                              channelQualifier:channelQualifier
                                          data:[data subdataWithRange:NSMakeRange(offset, size)]];
        });
    }
}

void EmiConnDelegate::emiConnLost() {
    id<EmiConnectionDelegate> connDelegate = _conn.delegate;
    dispatch_queue_t connDelegateQueue = _conn.delegateQueue;
    
    if (connDelegateQueue) {
        dispatch_async(connDelegateQueue, ^{
            [connDelegate emiConnectionLost:_conn];
        });
    }
}

void EmiConnDelegate::emiConnRegained() {
    id<EmiConnectionDelegate> connDelegate = _conn.delegate;
    dispatch_queue_t connDelegateQueue = _conn.delegateQueue;
    
    if (connDelegateQueue) {
        dispatch_async(connDelegateQueue, ^{
            [connDelegate emiConnectionRegained:_conn];
        });
    }
}

void EmiConnDelegate::emiConnDisconnect(EmiDisconnectReason reason) {
    id<EmiConnectionDelegate> connDelegate = _conn.delegate;
    dispatch_queue_t connDelegateQueue = _conn.delegateQueue;
    
    if (connDelegateQueue) {
        dispatch_async(connDelegateQueue, ^{
            [connDelegate emiConnectionDisconnect:_conn forReason:reason];
        });
    }
}

void EmiConnDelegate::emiNatPunchthroughFinished(bool success) {
    id<EmiConnectionDelegate> connDelegate = _conn.delegate;
    dispatch_queue_t connDelegateQueue = _conn.delegateQueue;
    
    if (connDelegateQueue) {
        dispatch_async(connDelegateQueue, ^{
            if (success) {
                if ([connDelegate respondsToSelector:@selector(emiP2PConnectionEstablished:)]) {
                    [connDelegate emiP2PConnectionEstablished:_conn];
                }
            }
            else {
                if ([connDelegate respondsToSelector:@selector(emiP2PConnectionNotEstablished:)]) {
                    [connDelegate emiP2PConnectionNotEstablished:_conn];
                }
            }
        });
    }
}
