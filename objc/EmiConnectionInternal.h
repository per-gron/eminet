//
//  EmiConnectionInternal.h
//  eminet
//
//  Created by Per Eckerdal on 2012-02-20.
//  Copyright (c) 2012 Per Eckerdal. All rights reserved.
//

#import "EmiConnection.h"

#include "EmiBinding.h"
#include "EmiConn.h"
#import "EmiSocket.h"

#import <Foundation/Foundation.h>

class EmiSockDelegate;
class EmiConnDelegate;
template<class Binding>
class EmiConnParams;
typedef EmiConn<EmiSockDelegate, EmiConnDelegate> EC;

@interface EmiConnection()

- (id)initWithSocket:(EmiSocket *)socket
     connectionQueue:(dispatch_queue_t)connectionQueue
              params:(const EmiConnParams<EmiBinding> *)params;

- (EC*)conn;

@end
