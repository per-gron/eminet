//
//  EmiDataArrivalRate.cc
//  eminet
//
//  Created by Per Eckerdal on 2012-05-22.
//  Copyright (c) 2012 Per Eckerdal. All rights reserved.
//

#include "EmiDataArrivalRate.h"

EmiDataArrivalRate::EmiDataArrivalRate() :
_lastPacketTime(-1),
_medianFilter(1) {}

EmiDataArrivalRate::~EmiDataArrivalRate() {}
