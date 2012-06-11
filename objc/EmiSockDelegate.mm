//
//  EmiSockDelegate.mm
//  roshambo
//
//  Created by Per Eckerdal on 2012-04-12.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#include "EmiSockDelegate.h"

#include "EmiBinding.h"
#include "EmiNetUtil.h"
#import "EmiSocketInternal.h"
#import "EmiConnectionInternal.h"

EmiSockDelegate::EmiSockDelegate(EmiSocket *socket) : _socket(socket) {}

EC *EmiSockDelegate::makeConnection(const EmiConnParams<EmiBinding>& params) {
    dispatch_queue_t connQueue = NULL;
    
    id<EmiSocketDelegate> delegate = _socket.delegate;
    
    if (delegate &&
        [delegate respondsToSelector:@selector(newConnectionQueueForConnectionFromAddress:onEmiSocket:)]) {
        const sockaddr_storage& ss(params.address);
        NSData *address = [NSData dataWithBytes:&ss
                                         length:EmiNetUtil::addrSize(ss)];
        connQueue = [delegate newConnectionQueueForConnectionFromAddress:address
                                                             onEmiSocket:_socket];
    }
    
    return [[EmiConnection alloc] initWithSocket:_socket
                                 connectionQueue:connQueue
                                          params:&params].conn;
}

void EmiSockDelegate::gotServerConnection(EC& conn) {
    __block id<EmiSocketDelegate> sockDelegate = _socket.delegate;
    dispatch_queue_t sockQueue = _socket.delegateQueue;
    
    __block EC& blockConn = conn;
    
    if (sockQueue) {
        dispatch_async(sockQueue, ^{
            [sockDelegate emiSocket:_socket gotConnection:blockConn.getDelegate().getConn()];
        });
    }
}

void EmiSockDelegate::connectionOpened(ConnectionOpenedCallbackCookie& cookie, bool error, EmiDisconnectReason reason, EC& ec) {
    // This code is thread safe, because the ConnectionOpenedCallbackCookie that we
    // get is a wrapper that invokes the real cookie in the correct queue. See
    // EmiSocket's connectToAddress:cookie:sharedSecret:block:blockQueue:error:
    // method.
    if (cookie) {
        if (error) {
            cookie(EmiBinding::makeError("com.emilir.eminet.disconnect", reason), nil);
        }
        else {
            cookie(nil, ec.getDelegate().getConn());
        }
        cookie = nil; // Release the block memory
    }
}

void EmiSockDelegate::connectionGotMessage(EC *conn,
                                           EmiUdpSocket<EmiBinding> *socket,
                                           EmiTimeInterval now,
                                           const sockaddr_storage& inboundAddress,
                                           const sockaddr_storage& remoteAddress,
                                           NSData *data,
                                           size_t offset,
                                           size_t len) {
    // This method is invoked in the EmiSocket's socketQueue. We want to
    // invoke onMessage on the EmiConnection's connectionQueue.
    dispatch_queue_t connectionQueue = conn->getDelegate().getConn().connectionQueue;
    dispatch_async(connectionQueue, ^{
        conn->onMessage(now, socket,
                        inboundAddress, remoteAddress,
                        data, offset, len);
    });
}
