//
//  EmiP2PSocketConfigInternal.h
//  eminet
//
//  Created by Per Eckerdal on 2012-04-23.
//  Copyright (c) 2012 Per Eckerdal. All rights reserved.
//

#import "EmiP2PSocketConfig.h"

#include "EmiP2PSockConfig.h"

typedef EmiP2PSockConfig EmiP2PSocketConfigSC;

@interface EmiP2PSocketConfig ()

@property (nonatomic, assign, readonly) EmiP2PSocketConfigSC *sockConfig;

@end
