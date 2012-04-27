//
//  EmiNetUtil.h
//  roshambo
//
//  Created by Per Eckerdal on 2012-04-12.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#ifndef emilir_EmiNetUtil_h
#define emilir_EmiNetUtil_h

#include <arpa/inet.h>
#include <cstdlib>
#include <cstdio>

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
    inline static void addrSetPort(sockaddr_storage& ss, uint16_t port) {
        if (AF_INET == ss.ss_family) {
            struct sockaddr_in& addr(*((struct sockaddr_in *)&ss));
            addr.sin_port = htons(port);
        }
        else if (AF_INET6 == ss.ss_family) {
            struct sockaddr_in6& addr6(*((struct sockaddr_in6 *)&ss));
            addr6.sin6_port = htons(port);
        }
        else {
            ASSERT(0 && "unexpected address family");
            abort();
        }
    }
public:
};

#endif
