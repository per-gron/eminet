//
//  EmiConnectionOpenedBlockWrapper.m
//  rock
//
//  Created by Per Eckerdal on 2012-06-13.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#import "EmiConnectionOpenedBlockWrapper.h"

@implementation EmiConnectionOpenedBlockWrapper

@synthesize delegate = _delegate;
@synthesize delegateQueue = _delegateQueue;
@synthesize cookie = _cookie;
@synthesize sharedSecret = _sharedSecret;
@synthesize userData = _userData;

- (id)initWithDelegate:(__unsafe_unretained id<EmiConnectionDelegate>)delegate
         delegateQueue:(dispatch_queue_t)delegateQueue
                cookie:(NSData *)cookie
          sharedSecret:(NSData *)sharedSecret
              userData:(id)userData {
    if (self = [super init]) {
        _delegate = delegate;
        _delegateQueue = delegateQueue;
        if (_delegateQueue) {
            dispatch_retain(_delegateQueue);
        }
        _cookie = cookie;
        _sharedSecret = sharedSecret;
        _userData = userData;
    }
    
    return self;
}

- (void)dealloc {
    if (_delegateQueue) {
        dispatch_release(_delegateQueue);
    }
}

+ (EmiConnectionOpenedBlockWrapper *)wrapperWithDelegate:(__unsafe_unretained id<EmiConnectionDelegate>)delegate
                                           delegateQueue:(dispatch_queue_t)delegateQueue
                                                  cookie:(NSData *)cookie
                                            sharedSecret:(NSData *)sharedSecret
                                                userData:(id)userData {
    return [[EmiConnectionOpenedBlockWrapper alloc] initWithDelegate:delegate
                                                       delegateQueue:delegateQueue
                                                              cookie:cookie
                                                        sharedSecret:sharedSecret
                                                            userData:userData];
}

@end
