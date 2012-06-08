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
    return [[EmiConnection alloc] initWithSocket:_socket
                                          params:&params].conn;
}

void EmiSockDelegate::gotServerConnection(EC& conn) {
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
