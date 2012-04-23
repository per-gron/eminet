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
#include <Security/Security.h>
#include <CommonCrypto/CommonHMAC.h>

EmiSockDelegate::EmiSockDelegate(EmiSocket *socket) : _socket(socket) {}

void EmiSockDelegate::closeSocket(EmiSockDelegate::ES& sock, GCDAsyncUdpSocket *socket) {
    [socket close];
}

GCDAsyncUdpSocket *EmiSockDelegate::openSocket(EmiSockDelegate::ES& sock, uint16_t port, Error& err) {
    GCDAsyncUdpSocket *socket = [[GCDAsyncUdpSocket alloc] initWithDelegate:_socket delegateQueue:dispatch_get_current_queue()];
    
    if (![socket bindToPort:port error:&err]) {
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

EC *EmiSockDelegate::makeConnection(NSData *address, uint16_t inboundPort, bool initiator) {
    return [[EmiConnection alloc] initWithSocket:_socket address:address inboundPort:inboundPort initiator:initiator].conn;
}

void EmiSockDelegate::sendData(GCDAsyncUdpSocket *socket, NSData *address, const uint8_t *data, size_t size) {
    // TODO This copies the packet data. We might want to redesign
    // this part of the code so that this is not required.
    [socket sendData:[NSData dataWithBytes:data length:size]
           toAddress:address
         withTimeout:-1 tag:0];
}

void EmiSockDelegate::gotConnection(EC& conn) {
    [_socket.delegate emiSocketGotConnection:conn.getDelegate().getConn()];
}

void EmiSockDelegate::connectionOpened(ConnectionOpenedCallbackCookie& cookie, bool error, EmiDisconnectReason reason, EC& ec) {
    if (cookie) {
        if (error) {
            cookie(makeError("com.emilir.eminet.disconnect", reason), nil);
        }
        else {
            cookie(nil, ec.getDelegate().getConn());
        }
        cookie = nil; // Release the block memory
    }
}

void EmiSockDelegate::hmacHash(const uint8_t *key, size_t keyLength,
                               const uint8_t *data, size_t dataLength,
                               uint8_t *buf, size_t bufLen) {
    if (bufLen < 256/8) {
        panic();
    }
    CCHmac(kCCHmacAlgSHA256, key, keyLength, data, dataLength, buf);
}

void EmiSockDelegate::randomBytes(uint8_t *buf, size_t bufSize) {
    if (0 != SecRandomCopyBytes(kSecRandomDefault, bufSize, buf)) {
        panic();
    }
}
