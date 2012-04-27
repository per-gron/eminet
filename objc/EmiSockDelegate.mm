//
//  EmiSockDelegate.mm
//  roshambo
//
//  Created by Per Eckerdal on 2012-04-12.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#include "EmiSockDelegate.h"

#include "EmiBinding.h"
#import "EmiSocketInternal.h"
#import "EmiConnectionInternal.h"

#import "GCDAsyncUdpSocket.h"

EmiSockDelegate::EmiSockDelegate(EmiSocket *socket) : _socket(socket) {}

void EmiSockDelegate::closeSocket(EmiSockDelegate::ES& sock, GCDAsyncUdpSocket *socket) {
    [socket close];
}

GCDAsyncUdpSocket *EmiSockDelegate::openSocket(const sockaddr_storage& address, __strong NSError*& err) {
    GCDAsyncUdpSocket *socket = [[GCDAsyncUdpSocket alloc] initWithDelegate:_socket delegateQueue:dispatch_get_current_queue()];
    
    if (![socket bindToAddress:[NSData dataWithBytes:&address
                                              length:EmiNetUtil::addrSize(address)]
                         error:&err]) {
        return nil;
    }
    
    if (![socket beginReceiving:&err]) {
        return nil;
    }
    
    return socket;
}

uint16_t EmiSockDelegate::extractLocalPort(GCDAsyncUdpSocket *socket) {
    return [socket localPort];
}

EC *EmiSockDelegate::makeConnection(const EmiConnParams& params) {
    return [[EmiConnection alloc] initWithSocket:_socket
                                          params:&params].conn;
}

void EmiSockDelegate::sendData(GCDAsyncUdpSocket *socket, const sockaddr_storage& address, const uint8_t *data, size_t size) {
    // TODO This copies the packet data. We might want to redesign
    // this part of the code so that this is not required.
    [socket sendData:[NSData dataWithBytes:data     length:size]
           toAddress:[NSData dataWithBytes:&address length:EmiNetUtil::addrSize(address)]
         withTimeout:-1 tag:0];
}

void EmiSockDelegate::gotConnection(EC& conn) {
    [_socket.delegate emiSocketGotConnection:conn.getDelegate().getConn()];
}

void EmiSockDelegate::connectionOpened(ConnectionOpenedCallbackCookie& cookie, bool error, EmiDisconnectReason reason, EC& ec) {
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
