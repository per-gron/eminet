//
//  EmiConnectionOpenedBlockWrapper.h
//  eminet
//
//  Created by Per Eckerdal on 2012-06-13.
//  Copyright (c) 2012 Per Eckerdal. All rights reserved.
//

#import <Foundation/Foundation.h>

@protocol EmiConnectionDelegate;

@interface EmiConnectionOpenedBlockWrapper : NSObject

- (id)initWithDelegate:(__weak id<EmiConnectionDelegate>)delegate
         delegateQueue:(dispatch_queue_t)delegateQueue
                cookie:(NSData *)cookie
          sharedSecret:(NSData *)sharedSecret
              userData:(id)userData;

+ (EmiConnectionOpenedBlockWrapper *)wrapperWithDelegate:(__weak id<EmiConnectionDelegate>)delegate
                                           delegateQueue:(dispatch_queue_t)delegateQueue
                                                  cookie:(NSData *)cookie
                                            sharedSecret:(NSData *)sharedSecret
                                                userData:(id)userData;

@property (nonatomic, weak, readonly) id<EmiConnectionDelegate> delegate;
@property (nonatomic, strong, readonly) dispatch_queue_t delegateQueue;
@property (nonatomic, strong, readonly) NSData *cookie;
@property (nonatomic, strong, readonly) NSData *sharedSecret;
@property (nonatomic, strong, readonly) id userData;

@end
