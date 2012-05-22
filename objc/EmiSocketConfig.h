//
//  EmiSocketConfig.h
//  roshambo
//
//  Created by Per Eckerdal on 2012-04-11.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#import <Foundation/Foundation.h>

#include "EmiTypes.h"

@interface EmiSocketConfig : NSObject {
    void *_sc;
}

@property (nonatomic, strong) NSData *serverAddress;
@property (nonatomic, assign) EmiTimeInterval connectionTimeout;
@property (nonatomic, assign) float heartbeatsBeforeConnectionWarning;
@property (nonatomic, assign) NSUInteger receiverBufferSize;
@property (nonatomic, assign) NSUInteger senderBufferSize;
@property (nonatomic, assign) BOOL acceptConnections;
@property (nonatomic, assign) uint16_t serverPort;
@property (nonatomic, assign) NSUInteger MTU;
@property (nonatomic, assign) float heartbeatFrequency;

@end
