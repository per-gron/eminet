//
//  EmiNetUtil.h
//  eminet
//
//  Created by Per Eckerdal on 2012-04-12.
//  Copyright (c) 2012 Per Eckerdal. All rights reserved.
//

#ifndef eminet_EmiNetUtil_h
#define eminet_EmiNetUtil_h

#include <netinet/in.h>
#include <stdlib.h>
#include <stdio.h>

// Have our own assert, so we are sure it does not get optimized away in
// a release build.
#define ASSERT(expr)                                      \
 do {                                                     \
  if (!(expr)) {                                          \
    fprintf(stderr,                                       \
            "Assertion failed in %s on line %d: %s\n",    \
            __FILE__,                                     \
            __LINE__,                                     \
            #expr);                                       \
    abort();                                              \
  }                                                       \
 } while (0)

class EmiNetUtil {
private:
    inline EmiNetUtil();
    
public:
    
    template<int NUM_BYTES>
    inline static int32_t cyclicDifference(int32_t a, int32_t b) {
        return (a>=b ? a-b : ((1 << (8*NUM_BYTES))-1)-b+a) & ((1 << (8*NUM_BYTES))-1);
    }
    
    template<int NUM_BYTES>
    inline static int32_t cyclicDifferenceSigned(int32_t a, int32_t b) {
        int32_t res = cyclicDifference<NUM_BYTES>(a, b);
        return res > ((1 << (8*NUM_BYTES))-1)/2 ? res-((1 << (8*NUM_BYTES))-1) : res;
    }
    
    template<int NUM_BYTES>
    inline static int32_t cyclicMax(int32_t a, int32_t b) {
        int32_t res = cyclicDifferenceSigned<NUM_BYTES>(a, b);
        return res > 0 ? a : b;
    }
    
    // buf is assumed to be >= 3 bytes
    inline static int32_t read24(const uint8_t *buf) {
        return ((buf[0] << 0) +
                (buf[1] << 8) +
                (buf[2] << 16));
    }
    
    // buf is assumed to be >= 3 bytes
    inline static void write24(uint8_t *buf, int32_t num) {
        buf[0] = (num >> 0);
        buf[1] = (num >> 8);
        buf[2] = (num >> 16);
    }
    
    // port should be in host byte order
    static void addrSetPort(sockaddr_storage& ss, uint16_t port);
    
    static void anyAddr(uint16_t port,
                        int family,
                        sockaddr_storage *out);
    static bool isAnyAddr(const sockaddr_storage& addr);
    
    // Returns the port number in network byte order
    static uint16_t addrPortN(const sockaddr_storage& address);
    
    // Returns the port number in host byte order
    static uint16_t addrPortH(const sockaddr_storage& address);
    
    static size_t addrSize(const sockaddr_storage& address);
    
    static size_t familyIpLength(int family);
    static size_t ipLength(const sockaddr_storage& address);
    
    // Saves the IP address in buf, in network byte order. Returns the length of the IP address.
    static size_t extractIp(const sockaddr_storage& address, uint8_t *buf, size_t bufSize);
    
    // The IP and port number should be in network byte order
    static void makeAddress(int family, const uint8_t *ip, size_t ipLen, uint16_t port, sockaddr_storage *out);
    
    // Will fill address with an address that cannot be the receiver of a packet
    static void fillNilAddress(int family, sockaddr_storage& address);
    static bool isNilAddress(const sockaddr_storage& address);
};

#endif
