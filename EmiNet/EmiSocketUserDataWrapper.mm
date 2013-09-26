//
//  EmiSocketUserDataWrapper.mm
//  eminet
//
//  Created by Per Eckerdal on 2012-05-11.
//  Copyright (c) 2012 Per Eckerdal. All rights reserved.
//

#import "EmiSocketUserDataWrapper.h"

@implementation EmiSocketUserDataWrapper

@synthesize userData = _userData;
@synthesize callback = _callback;

- (id)initWithUserData:(void *)userData callback:(EmiOnMessage *)callback {
    if (self = [super init]) {
        _userData = userData;
        _callback = callback;
    }
    
    return self;
}

@end
