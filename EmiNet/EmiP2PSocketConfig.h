//
//  EmiP2PSocketConfig.h
//  eminet
//
//  Created by Per Eckerdal on 2012-04-23.
//  Copyright (c) 2012 Per Eckerdal. All rights reserved.
//

#import <Foundation/Foundation.h>

#include "EmiTypes.h"

@interface EmiP2PSocketConfig : NSObject

@property (nonatomic, strong) NSData *serverAddress;
@property (nonatomic, assign) EmiTimeInterval connectionTimeout;
@property (nonatomic, assign) NSUInteger rateLimit;
@property (nonatomic, assign) uint16_t serverPort;

@end
