//
//  EmiConnectionOpenedBlockWrapper.h
//  eminet
//
//  Created by Per Eckerdal on 2012-06-13.
//  Copyright (c) 2012 Per Eckerdal. All rights reserved.
//

#import <Foundation/Foundation.h>

@protocol EmiConnectionDelegate;

@interface EmiConnectionOpenedBlockWrapper : NSObject {
    __unsafe_unretained id<EmiConnectionDelegate> _delegate;
    dispatch_queue_t _delegateQueue;
    NSData *_cookie;
    NSData *_sharedSecret;
    id _userData;
}

- (id)initWithDelegate:(__unsafe_unretained id<EmiConnectionDelegate>)delegate
         delegateQueue:(dispatch_queue_t)delegateQueue
                cookie:(NSData *)cookie
          sharedSecret:(NSData *)sharedSecret
              userData:(id)userData;

+ (EmiConnectionOpenedBlockWrapper *)wrapperWithDelegate:(__unsafe_unretained id<EmiConnectionDelegate>)delegate
                                           delegateQueue:(dispatch_queue_t)delegateQueue
                                                  cookie:(NSData *)cookie
                                            sharedSecret:(NSData *)sharedSecret
                                                userData:(id)userData;

@property (nonatomic, unsafe_unretained, readonly) id<EmiConnectionDelegate> delegate;
@property (nonatomic, assign, readonly) dispatch_queue_t delegateQueue;
@property (nonatomic, retain, readonly) NSData *cookie;
@property (nonatomic, retain, readonly) NSData *sharedSecret;
@property (nonatomic, retain, readonly) id userData;

@end
