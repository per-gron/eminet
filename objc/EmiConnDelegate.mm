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
_queueWrapper([[EmiDispatchQueueWrapper alloc] initWithQueue:conn.connectionQueue]),
_dispatchGroup(dispatch_group_create()) {
    
}

EmiConnDelegate::~EmiConnDelegate() {
    dispatch_group_wait(_dispatchGroup, DISPATCH_TIME_FOREVER);
    dispatch_release(_dispatchGroup);
}

EmiConnDelegate::EmiConnDelegate(const EmiConnDelegate& other) :
_conn(other._conn),
_queueWrapper(other._queueWrapper),
_dispatchGroup(other._dispatchGroup) {
    dispatch_retain(_dispatchGroup);
}

EmiConnDelegate& EmiConnDelegate::operator=(const EmiConnDelegate& other) {
    if (_dispatchGroup) {
        dispatch_release(_dispatchGroup);
    }
    
    _conn = other._conn;
    _queueWrapper = other._queueWrapper;
    _dispatchGroup = other._dispatchGroup;
    dispatch_retain(_dispatchGroup);
    return *this;
}

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
    
    // Because this is a strong reference, we need to nil it out to let ARC deallocate this connection for us
    _conn = nil;
}

void EmiConnDelegate::emiConnPacketLoss(EmiChannelQualifier channelQualifier,
                                        EmiSequenceNumber packetsLost) {
    if (_conn.delegateQueue) {
        id<EmiConnectionDelegate> connDelegate = _conn.delegate;
        EmiConnection *conn = _conn;
        dispatch_group_async(_dispatchGroup, _conn.delegateQueue, ^{
            if (connDelegate && [connDelegate respondsToSelector:@selector(emiConnectionPacketLoss:channelQualifier:packetsLost:)]) {
                [connDelegate emiConnectionPacketLoss:conn
                                     channelQualifier:channelQualifier
                                          packetsLost:packetsLost];
            }
        });
    }
}

void EmiConnDelegate::emiConnMessage(EmiChannelQualifier channelQualifier, NSData *data, NSUInteger offset, NSUInteger size) {
    if (_conn.delegateQueue) {
        id<EmiConnectionDelegate> connDelegate = _conn.delegate;
        EmiConnection *conn = _conn;
        dispatch_group_async(_dispatchGroup, _conn.delegateQueue, ^{
            [connDelegate emiConnectionMessage:conn
                              channelQualifier:channelQualifier
                                          data:[data subdataWithRange:NSMakeRange(offset, size)]];
        });
    }
}

void EmiConnDelegate::emiConnLost() {
    if (_conn.delegateQueue) {
        id<EmiConnectionDelegate> connDelegate = _conn.delegate;
        EmiConnection *conn = _conn;
        dispatch_group_async(_dispatchGroup, _conn.delegateQueue, ^{
            if ([connDelegate respondsToSelector:@selector(emiConnectionLost:)]) {
                [connDelegate emiConnectionLost:conn];
            }
        });
    }
}

void EmiConnDelegate::emiConnRegained() {
    if (_conn.delegateQueue) {
        id<EmiConnectionDelegate> connDelegate = _conn.delegate;
        EmiConnection *conn = _conn;
        dispatch_group_async(_dispatchGroup, _conn.delegateQueue, ^{
            if ([connDelegate respondsToSelector:@selector(emiConnectionRegained:)]) {
                [connDelegate emiConnectionRegained:conn];
            }
        });
    }
}

void EmiConnDelegate::emiConnDisconnect(EmiDisconnectReason reason) {
    if (_conn.delegateQueue) {
        id<EmiConnectionDelegate> connDelegate = _conn.delegate;
        EmiConnection *conn = _conn;
        dispatch_group_async(_dispatchGroup, _conn.delegateQueue, ^{
            [connDelegate emiConnectionDisconnect:conn forReason:reason];
        });
    }
}

void EmiConnDelegate::emiNatPunchthroughFinished(bool success) {
    if (_conn.delegateQueue) {
        id<EmiConnectionDelegate> connDelegate = _conn.delegate;
        EmiConnection *conn = _conn;
        dispatch_group_async(_dispatchGroup, _conn.delegateQueue, ^{
            if (success) {
                if ([connDelegate respondsToSelector:@selector(emiP2PConnectionEstablished:)]) {
                    [connDelegate emiP2PConnectionEstablished:conn];
                }
            }
            else {
                if ([connDelegate respondsToSelector:@selector(emiP2PConnectionNotEstablished:)]) {
                    [connDelegate emiP2PConnectionNotEstablished:conn];
                }
            }
        });
    }
}

void EmiConnDelegate::waitForDelegateBlocks() {
    dispatch_group_wait(_dispatchGroup, DISPATCH_TIME_FOREVER);
}
