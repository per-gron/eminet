//
//  EmiSocketConfigInternal.h
//  roshambo
//
//  Created by Per Eckerdal on 2012-04-11.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#import "EmiSocketConfig.h"

#include "EmiSock.h"

typedef EmiSockConfig<NSData *> EmiSocketConfigSC;

@interface EmiSocketConfig()

@property (nonatomic, assign, readonly) EmiSocketConfigSC *sockConfig;

@end
