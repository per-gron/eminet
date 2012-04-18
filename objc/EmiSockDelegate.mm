//
//  EmiSockDelegate.mm
//  roshambo
//
//  Created by Per Eckerdal on 2012-04-12.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#include "EmiSockDelegate.h"

#import "EmiSocketInternal.h"
#import "EmiConnectionInternal.h"

#import "GCDAsyncUdpSocket.h"

EmiSockDelegate::EmiSockDelegate(EmiSocket *socket) : _socket(socket) {}

void EmiSockDelegate::closeSocket(GCDAsyncUdpSocket *socket) {
    [socket close];
}

GCDAsyncUdpSocket *EmiSockDelegate::openSocket(uint16_t port, Error& err) {
    GCDAsyncUdpSocket *socket = [[GCDAsyncUdpSocket alloc] initWithDelegate:_socket delegateQueue:dispatch_get_current_queue()];
    
    if (![socket bindToPort:port error:&err]) {
        return nil;
    }
    
    if (![socket beginReceiving:&err]) {
        return nil;
    }
    
    return socket;
}

EC *EmiSockDelegate::makeConnection(NSData *address, uint16_t inboundPort, bool initiator) {
    return [[EmiConnection alloc] initWithSocket:_socket address:address inboundPort:inboundPort initiator:initiator].conn;
}

void EmiSockDelegate::sendData(GCDAsyncUdpSocket *socket, NSData *address, const uint8_t *data, size_t size) {
    [socket sendData:[NSData dataWithBytesNoCopy:(void *)data length:size freeWhenDone:NO]
           toAddress:address
         withTimeout:-1 tag:0];
}

void EmiSockDelegate::gotConnection(EC *conn) {
    [_socket.delegate emiSocketGotConnection:conn->getDelegate().conn];
}