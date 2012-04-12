//
//  EmiConnectionInternal.h
//  roshambo
//
//  Created by Per Eckerdal on 2012-02-20.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#import "EmiConnection.h"

#include "EmiConn.h"
#import "EmiSocket.h"

#import <Foundation/Foundation.h>

class EmiSockDelegate;
class EmiConnDelegate;
typedef EmiConn<EmiSockDelegate, EmiConnDelegate> EC;

@interface EmiConnection()

- (id)initWithSocket:(EmiSocket *)socket
             address:(NSData *)address 
         inboundPort:(uint16_t)inboundPort
           initiator:(BOOL)initiator;

- (EC*)conn;

@end
