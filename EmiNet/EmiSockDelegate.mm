//
//  EmiSockDelegate.mm
//  eminet
//
//  Created by Per Eckerdal on 2012-04-12.
//  Copyright (c) 2012 Per Eckerdal. All rights reserved.
//

#include "EmiSockDelegate.h"

#include "EmiBinding.h"
#include "EmiNetUtil.h"
#import "EmiSocketInternal.h"
#import "EmiConnectionInternal.h"
#import "EmiConnectionOpenedBlockWrapper.h"
#include "EmiObjCBindingHelper.h"

EmiSockDelegate::EmiSockDelegate(EmiSocket *socket) :
_socket(socket),
_dispatchGroup(dispatch_group_create()) {}

EmiSockDelegate::~EmiSockDelegate() {
    dispatch_group_wait(_dispatchGroup, DISPATCH_TIME_FOREVER);
    dispatch_release(_dispatchGroup);
}

EmiSockDelegate::EmiSockDelegate(const EmiSockDelegate& other) :
_socket(other._socket),
_dispatchGroup(other._dispatchGroup) {
    dispatch_retain(_dispatchGroup);
}

EmiSockDelegate& EmiSockDelegate::operator=(const EmiSockDelegate& other) {
    if (_dispatchGroup) {
        dispatch_release(_dispatchGroup);
    }
    
    _socket = other._socket;
    _dispatchGroup = other._dispatchGroup;
    dispatch_retain(_dispatchGroup);
    return *this;
}

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
        dispatch_group_async(_dispatchGroup, sockQueue, ^{
            [sockDelegate emiSocket:_socket gotConnection:blockConn.getDelegate().getConn()];
        });
    }
}

void EmiSockDelegate::connectionOpened(ConnectionOpenedCallbackCookie& cookie, bool error, EmiDisconnectReason reason, EC& ec) {
    // This code is thread safe, because the ConnectionOpenedCallbackCookie that we
    // get is a wrapper that invokes the real cookie in the correct queue. See
    // EmiSocket's connectToAddress:cookie:sharedSecret:block:blockQueue:error:
    // method.
    
    EmiConnectionOpenedBlockWrapper *wrapper = cookie;
    cookie = nil; // Release the block memory
    
    EmiConnection *conn = ec.getDelegate().getConn();
    
    // It is crucial that delegate and delegateQueue are set here, before
    // anything else can happen, and in particular before we do any async
    // thing. Otherwise we'll have a race condition where the connection
    // might receive messages before the delegate is set.
    [conn setDelegate:wrapper.delegate delegateQueue:wrapper.delegateQueue];
    
    dispatch_async(wrapper.delegateQueue, ^{
        if (error) {
            [wrapper.delegate emiConnectionFailedToConnect:conn.emiSocket
                                                     error:EmiBinding::makeError("com.emilir.eminet.disconnect", reason)
                                                  userData:wrapper.userData];
        }
        else {
            [wrapper.delegate emiConnectionOpened:conn
                                         userData:wrapper.userData];
        }
    });
}

void EmiSockDelegate::connectionGotMessage(EC *conn,
                                           EmiUdpSocket<EmiBinding> *socket,
                                           EmiTimeInterval now,
                                           sockaddr_storage inboundAddress,
                                           sockaddr_storage remoteAddress,
                                           NSData *data,
                                           size_t offset,
                                           size_t len) {
    // This method is invoked in the EmiSocket's socketQueue. We want to
    // invoke onMessage on the EmiConnection's connectionQueue.
    dispatch_queue_t connectionQueue = conn->getDelegate().getConn().connectionQueue;
    dispatch_group_async(_dispatchGroup, connectionQueue, ^{
        conn->onMessage(now, socket,
                        inboundAddress, remoteAddress,
                        data, offset, len);
    });
}
