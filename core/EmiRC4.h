//
//  EmiRC4.h
//  eminet
//
//  RC4 implementation derived from LibTomCrypt
//
//  Created by Per Eckerdal on 2012-06-22.
//  Copyright (c) 2012 Per Eckerdal. All rights reserved.
//

#ifndef eminet_EmiRC4_h
#define eminet_EmiRC4_h

#include <stddef.h>

class EmiRC4 {
public:
    static const size_t ENTROPY_SIZE = 256;
    
private:
    int _x, _y;
    unsigned char _buf[ENTROPY_SIZE];
    
    // Private copy constructor and assignment operator
    inline EmiRC4(const EmiRC4& other);
    inline EmiRC4& operator=(const EmiRC4& other);
    
public:
    
    EmiRC4();
    
    void reset();
    
    /**
     Add entropy to the PRNG state
     @param in       The data to add
     @param inlen    Length of the data to add
     @param prng     PRNG state to update
     */  
    void addEntropy(const unsigned char *in, unsigned long inlen);
    
    /**
     Make the PRNG ready to read from
     @param prng   The PRNG to make active
     */  
    void makeReady();
    
    /**
     Read from the PRNG
     @param out      Destination
     @param outlen   Length of output
     @param prng     The active PRNG to read from
     */  
    void read(unsigned char *out, size_t outlen);
};

#endif
