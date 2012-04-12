//
//  EmiSocketInternal.h
//  roshambo
//
//  Created by Per Eckerdal on 2012-02-20.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#import "EmiSocket.h"

#include "EmiSock.h"
#include "EmiConn.h"
#include "EmiAddressCmp.h"
#include "EmiSockDelegate.h"

typedef EmiSock<EmiSockDelegate, EmiConnDelegate> S;
typedef EmiConn<EmiSockDelegate, EmiConnDelegate> EC;

@interface EmiSocket()

- (S *)sock;

@end
