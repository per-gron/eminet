//
//  EmiConnDelegate.mm
//  roshambo
//
//  Created by Per Eckerdal on 2012-04-11.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#import "EmiConnDelegate.h"

EmiConnDelegate::EmiConnDelegate(EmiConnection *conn) :
_conn(conn) {}

void EmiConnDelegate::invalidate() {
    // Just to be sure, since the ivar is __unsafe_unretained
    // Note that this code would be incorrect if connections supported reconnecting; It's correct only because after a forceClose, the delegate will never be called again.
    _conn.delegate = nil;
    
    // Because this is a strong reference, we need to nil it out to let ARC deallocate this connection for us
    _conn = nil;
}

void EmiConnDelegate::emiConnMessage(EmiChannelQualifier channelQualifier, NSData *data, NSUInteger offset, NSUInteger size) {
    [_conn.delegate emiConnectionMessage:_conn
                        channelQualifier:channelQualifier 
                                    data:[data subdataWithRange:NSMakeRange(offset, size)]];
}

void EmiConnDelegate::emiConnLost() {
    [_conn.delegate emiConnectionLost:_conn];
}

void EmiConnDelegate::emiConnRegained() {
    [_conn.delegate emiConnectionRegained:_conn];
}

void EmiConnDelegate::emiConnDisconnect(EmiDisconnectReason reason) {
    [_conn.delegate emiConnectionDisconnect:_conn forReason:reason];
}
