//
//  EmiRC4.cc
//  rock
//
//  RC4 implementation derived from LibTomCrypt
//
//  Created by Per Eckerdal on 2012-06-22.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#include "EmiRC4.h"

#include "EmiNetUtil.h"

#include <string.h>

EmiRC4::EmiRC4() {
    // Set keysize to zero
    _x = 0;
    _y = 0;
    memset(_buf, sizeof(_buf), ENTROPY_SIZE);
}

void EmiRC4::reset() {
    _x = 0;
}

void EmiRC4::addEntropy(const unsigned char *in, unsigned long inlen) {
    ASSERT(in != NULL);
    
    // Trim as required
    if (_x + inlen > 256) {
        if (_x == 256) {
            // I can't possibly accept another byte,
            // ok maybe a mint wafer...
            return;
        } else {
            // Only accept part of it
            inlen = 256 - _x;
        }       
    }
    
    while (inlen--) {
        _buf[_x++] = *in++;
    }
}

void EmiRC4::makeReady() {
    unsigned char key[256], tmp, *s;
    int keylen, x, y, j;
    
    /* extract the key */
    s = _buf;
    memcpy(key, s, 256);
    keylen = _x;
    
    /* make LTC_RC4 perm and shuffle */
    for (x = 0; x < 256; x++) {
        s[x] = x;
    }
    
    for (j = x = y = 0; x < 256; x++) {
        y = (y + _buf[x] + key[j++]) & 255;
        if (j == keylen) {
            j = 0; 
        }
        tmp = s[x]; s[x] = s[y]; s[y] = tmp;
    }
    _x = 0;
    _y = 0;
}

void EmiRC4::read(unsigned char *out, size_t outlen) {
    unsigned char *s, tmp;
    
    ASSERT(out  != NULL);
    
    s = _buf;
    while (outlen--) {
        _x = (_x + 1) & 255;
        _y = (_y + s[_x]) & 255;
        tmp = s[_x]; s[_x] = s[_y]; s[_y] = tmp;
        tmp = (s[_x] + s[_y]) & 255;
        *out++ ^= s[tmp];
    }
}
