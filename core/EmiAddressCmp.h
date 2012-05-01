//
//  EmiAddressCmp.h
//  roshambo
//
//  Created by Per Eckerdal on 2012-04-12.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#ifndef roshambo_EmiAddressCmp_h
#define roshambo_EmiAddressCmp_h

#include <netinet/in.h>
#include <cstring>

class EmiAddressCmp {
public:
    inline static int compare(const struct sockaddr_storage& a, const struct sockaddr_storage& b) {
        if (a.ss_family < b.ss_family) return -1;
        else if (a.ss_family > b.ss_family) return 1;
        else {
            uint16_t aPort;
            uint16_t bPort;
            
            struct sockaddr_in *aIn = (struct sockaddr_in *)&a;
            struct sockaddr_in *bIn = (struct sockaddr_in *)&b;
            
            struct sockaddr_in6 *aIn6 = (struct sockaddr_in6 *)&a;
            struct sockaddr_in6 *bIn6 = (struct sockaddr_in6 *)&b;
            
            if (AF_INET == a.ss_family) {
                aPort = aIn->sin_port;
                bPort = bIn->sin_port;
            }
            else { // Assume AF_INET6
                aPort = aIn6->sin6_port;
                bPort = bIn6->sin6_port;
            }
            
            if (aPort < bPort) return -1;
            else if (aPort > bPort) return 1;
            else {
                
                if (AF_INET == b.ss_family) {
                    return ntohl(aIn->sin_addr.s_addr) - ntohl(bIn->sin_addr.s_addr);
                }
                else { // Assume AF_INET6
                    return memcmp(aIn6->sin6_addr.s6_addr, bIn6->sin6_addr.s6_addr, sizeof(aIn6->sin6_addr.s6_addr));
                }
            }
        }
    }
    
    inline int operator()(const struct sockaddr_storage& a, const struct sockaddr_storage& b) const {
        return compare(a, b);
    }
};

#endif
