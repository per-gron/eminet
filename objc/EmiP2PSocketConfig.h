//
//  EmiP2PSocketConfig.h
//  roshambo
//
//  Created by Per Eckerdal on 2012-04-23.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#import <Foundation/Foundation.h>

#include "EmiTypes.h"

@interface EmiP2PSocketConfig : NSObject {
    void *_sc;
}

@property (nonatomic, strong) NSData *serverAddress;
@property (nonatomic, assign) EmiTimeInterval connectionTimeout;
@property (nonatomic, assign) NSUInteger rateLimit;
@property (nonatomic, assign) uint16_t serverPort;

@end
