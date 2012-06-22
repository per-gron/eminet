//
//  EmiNetRandom.h
//  rock
//
//  Derived from bsd-arc4random.c from OpenBSD
//

/*
 * Copyright (c) 1999,2000,2004 Damien Miller <djm@mindrot.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef rock_EmiNetRandom_h
#define rock_EmiNetRandom_h

#include "EmiRC4.h"

#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

template<class Binding>
class EmiNetRandom {
    // Number of bytes to reseed after
    static const int REKEY_BYTES = (1 << 24);
    
    static int _rc4Ready;
    static EmiRC4 _rc4;
    
public:
    
    static const uint64_t RANDOM_MAX = 0x100000000ULL;
    
    static uint32_t random() {
        uint32_t r = 0;
        
        if (_rc4Ready <= 0) {
            randomStir();
        }
        
        _rc4.read((unsigned char *)&r, sizeof(r));
        
        _rc4Ready -= sizeof(r);
        
        return(r);
    }
    
    static void randomStir() {
        unsigned char randBuf[EmiRC4::ENTROPY_SIZE];
        int i;
        
        _rc4.reset();
        Binding::randomBytes(randBuf, sizeof(randBuf));
        _rc4.addEntropy(randBuf, sizeof(randBuf));
        _rc4.makeReady();
        
        /*
         * Discard early keystream, as per recommendations in:
         * http://www.wisdom.weizmann.ac.il/~itsik/RC4/Papers/Rc4_ksa.ps
         */
        for(i = 0; i <= 256; i += sizeof(randBuf)) {
            _rc4.read(randBuf, sizeof(randBuf));
        }
        
        memset(randBuf, 0, sizeof(randBuf));
        
        _rc4Ready = REKEY_BYTES;
    }
    
    void randomBuf(void *_buf, size_t n) {
        size_t i;
        uint32_t r = 0;
        char *buf = (char *)_buf;
        
        for (i = 0; i < n; i++) {
            if (i % sizeof(r) == 0)
                r = random();
            buf[i] = r & 0xff;
            r >>= 8;
        }
        i = r = 0;
    }
    
    /*
     * Calculate a uniformly distributed random number less than upper_bound
     * avoiding "modulo bias".
     *
     * Uniformity is achieved by generating new random numbers until the one
     * returned is outside the range [0, 2**32 % upper_bound).  This
     * guarantees the selected random number will be inside
     * [2**32 % upper_bound, 2**32) which maps back to [0, upper_bound)
     * after reduction modulo upper_bound.
     */
    static uint32_t randomUniform(uint32_t upper_bound) {
        uint32_t r, min;
        
        if (upper_bound < 2)
            return 0;
        
#if (ULONG_MAX > 0xffffffffUL)
        min = 0x100000000UL % upper_bound;
#else
        /* Calculate (2**32 % upper_bound) avoiding 64-bit math */
        if (upper_bound > 0x80000000)
            min = 1 + ~upper_bound;		/* 2**32 - upper_bound */
        else {
            /* (2**32 - (x * 2)) % x == 2**32 % x when x <= 2**31 */
            min = ((0xffffffff - (upper_bound * 2)) + 1) % upper_bound;
        }
#endif
        
        /*
         * This could theoretically loop forever but each retry has
         * p > 0.5 (worst case, usually far better) of selecting a
         * number inside the range we need, so it should rarely need
         * to re-roll.
         */
        for (;;) {
            r = random();
            if (r >= min)
                break;
        }
        
        return r % upper_bound;
    }
    
    /*
     * Calculate a uniformly distributed floating point random number
     * in the range [0, 1)
     */
    static float randomFloat() {
        return ((float)random() / RANDOM_MAX);
    }
};

template<class Binding>
int EmiNetRandom<Binding>::_rc4Ready = 0;

template<class Binding>
EmiRC4 EmiNetRandom<Binding>::_rc4;

#endif
