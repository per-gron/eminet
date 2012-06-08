//
//  EmiDispatchQueueWrapper.m
//  rock
//
//  Created by Per Eckerdal on 2012-06-08.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#import "EmiDispatchQueueWrapper.h"

@implementation EmiDispatchQueueWrapper

- (id)initWithQueue:(dispatch_queue_t)queue_ {
    if (self = [super init]) {
        queue = queue_;
        if (queue) {
            dispatch_retain(queue);
        }
    }
    
    return self;
}

- (void)dealloc {
    if (queue) {
        dispatch_release(queue);
    }
}

@end
