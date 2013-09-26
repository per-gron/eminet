//
//  EmiDispatchQueueWrapper.m
//  eminet
//
//  Created by Per Eckerdal on 2012-06-08.
//  Copyright (c) 2012 Per Eckerdal. All rights reserved.
//

#import "EmiDispatchQueueWrapper.h"

@implementation EmiDispatchQueueWrapper

- (id)initWithQueue:(dispatch_queue_t)queue_ {
    if (self = [super init]) {
        queue = queue_;
    }
    
    return self;
}

@end
