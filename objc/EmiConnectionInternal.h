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
template<class Address>
class EmiConnParams;
typedef EmiConn<EmiSockDelegate, EmiConnDelegate> EC;

@interface EmiConnection()

- (id)initWithSocket:(EmiSocket *)socket
              params:(const EmiConnParams<NSData *> *)params;

- (EC*)conn;

@end
