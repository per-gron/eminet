//
//  EmiSocketConfigInternal.h
//  eminet
//
//  Created by Per Eckerdal on 2012-04-11.
//  Copyright (c) 2012 Per Eckerdal. All rights reserved.
//

#import "EmiSocketConfig.h"

#include "EmiSockConfig.h"

typedef EmiSockConfig EmiSocketConfigSC;

@interface EmiSocketConfig()

@property (nonatomic, assign, readonly) EmiSocketConfigSC *sockConfig;

@end
