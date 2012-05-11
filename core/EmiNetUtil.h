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
    
    static const uint64_t ARC4RANDOM_MAX;
    
    inline static int32_t cyclicDifference16(int32_t a, int32_t b) {
        return (a>=b ? a-b : 0xffff-b+a) & 0xffff;
    }
    
    inline static int32_t cyclicDifference16Signed(int32_t a, int32_t b) {
        int32_t res = cyclicDifference16(a, b);
        return res > 0x7fff ? res-0xffff : res;
    }
    
    inline static int32_t cyclicMax16(int32_t a, int32_t b) {
        int32_t res = cyclicDifference16Signed(a, b);
        return res > 0 ? a : b;
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
};

#endif
