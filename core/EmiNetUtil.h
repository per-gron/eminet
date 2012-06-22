//
//  EmiNetUtil.h
//  roshambo
//
//  Created by Per Eckerdal on 2012-04-12.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#ifndef emilir_EmiNetUtil_h
#define emilir_EmiNetUtil_h

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
    
    inline static int32_t cyclicDifference24(int32_t a, int32_t b) {
        return (a>=b ? a-b : 0xffffff-b+a) & 0xffffff;
    }
    
    inline static int32_t cyclicDifference24Signed(int32_t a, int32_t b) {
        int32_t res = cyclicDifference24(a, b);
        return res > 0x7fffff ? res-0xffffff : res;
    }
    
    inline static int32_t cyclicMax24(int32_t a, int32_t b) {
        int32_t res = cyclicDifference24Signed(a, b);
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
