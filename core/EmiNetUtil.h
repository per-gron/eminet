//
//  EmiNetUtil.h
//  roshambo
//
//  Created by Per Eckerdal on 2012-04-12.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#ifndef emilir_EmiNetUtil_h
#define emilir_EmiNetUtil_h

inline int32_t emiCyclicDifference16(int32_t a, int32_t b) {
    return (a>=b ? a-b : 0xffff-b+a) & 0xffff;
}

inline int32_t emiCyclicDifference16Signed(int32_t a, int32_t b) {
    int32_t res = emiCyclicDifference16(a, b);
    return res > 0x7fff ? res-0xffff : res;
}

inline int32_t emiCyclicMax16(int32_t a, int32_t b) {
    int32_t res = emiCyclicDifference16Signed(a, b);
    return res > 0 ? a : b;
}

#endif
