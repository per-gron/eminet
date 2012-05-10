//
//  EmiNetUtil.cc
//  rock
//
//  Created by Per Eckerdal on 2012-05-10.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#include "EmiNetUtil.h"

#include <arpa/inet.h>
#include <cstdlib>
#include <cstdio>
#include <cstring>

void EmiNetUtil::addrSetPort(sockaddr_storage& ss, uint16_t port) {
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

void EmiNetUtil::anyAddr(uint16_t port,
                         int family,
                         sockaddr_storage *out) {
    if (AF_INET6 == family) {
        struct sockaddr_in6& addr6(*((struct sockaddr_in6 *)out));
        addr6.sin6_len       = sizeof(struct sockaddr_in6);
        addr6.sin6_family    = AF_INET6;
        addr6.sin6_port      = htons(port);
        addr6.sin6_flowinfo  = 0;
        addr6.sin6_addr      = in6addr_any;
        addr6.sin6_scope_id  = 0;
    }
    else if (AF_INET == family) {
        struct sockaddr_in& addr(*((struct sockaddr_in *)out));
        addr.sin_len         = sizeof(struct sockaddr_in);
        addr.sin_family      = AF_INET;
        addr.sin_port        = htons(port);
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        memset(&(addr.sin_zero), 0, sizeof(addr.sin_zero));
    }
    else {
        ASSERT(0 && "unexpected address family");
        abort();
    }
}

uint16_t EmiNetUtil::addrPortN(const sockaddr_storage& address) {
    if (AF_INET6 == address.ss_family) {
        const struct sockaddr_in6& addr6(*((const struct sockaddr_in6 *)&address));
        return addr6.sin6_port;
    }
    else if (AF_INET == address.ss_family) {
        const struct sockaddr_in& addr(*((const struct sockaddr_in *)&address));
        return addr.sin_port;
    }
    else {
        ASSERT(0 && "unexpected address family");
        abort();
    }
}

uint16_t EmiNetUtil::addrPortH(const sockaddr_storage& address) {
    return ntohs(addrPortN(address));
}

size_t EmiNetUtil::addrSize(const sockaddr_storage& address) {
    if (AF_INET == address.ss_family) {
        return sizeof(sockaddr_in);
    }
    else if (AF_INET6 == address.ss_family) {
        return sizeof(sockaddr_in6);
    }
    else {
        ASSERT(0 && "unexpected address family");
        abort();
    }
}

size_t EmiNetUtil::ipLength(const sockaddr_storage& address) {
    int family = address.ss_family;
    if (AF_INET == family) {
        return sizeof(in_addr);
    }
    else if (AF_INET6 == family) {
        return sizeof(in6_addr);
    }
    else {
        ASSERT(0 && "unexpected address family");
        abort();
    }
}

size_t EmiNetUtil::extractIp(const sockaddr_storage& address, uint8_t *buf, size_t bufSize) {
    int family = address.ss_family;
    if (AF_INET == family) {
        const struct sockaddr_in& addr(*((struct sockaddr_in *)&address));
        size_t addrSize = sizeof(in_addr);
        ASSERT(bufSize >= addrSize);
        memcpy(buf, &addr.sin_addr, addrSize);
        return addrSize;
    }
    else if (AF_INET6 == family) {
        const struct sockaddr_in6& addr6(*((struct sockaddr_in6 *)&address));
        size_t addrSize = sizeof(in6_addr);
        ASSERT(bufSize >= addrSize);
        memcpy(buf, &addr6.sin6_addr, addrSize);
        return addrSize;
    }
    else {
        ASSERT(0 && "unexpected address family");
        abort();
    }
}

void EmiNetUtil::makeAddress(int family, const uint8_t *ip, size_t ipLen, uint16_t port, sockaddr_storage *out) {
    if (AF_INET6 == family) {
        ASSERT(16 == ipLen);
        
        struct sockaddr_in6& addr6(*((struct sockaddr_in6 *)out));
        addr6.sin6_len       = sizeof(struct sockaddr_in6);
        addr6.sin6_family    = AF_INET6;
        addr6.sin6_port      = port;
        addr6.sin6_flowinfo  = 0;
        addr6.sin6_addr      = *((struct in6_addr *)ip);
        addr6.sin6_scope_id  = 0;
    }
    else if (AF_INET == family) {
        ASSERT(4 == ipLen);
        
        struct sockaddr_in& addr(*((struct sockaddr_in *)out));
        addr.sin_len         = sizeof(struct sockaddr_in);
        addr.sin_family      = AF_INET;
        addr.sin_port        = port;
        addr.sin_addr.s_addr = *((uint32_t *)ip);
        memset(&(addr.sin_zero), 0, sizeof(addr.sin_zero));
    }
    else {
        ASSERT(0 && "unexpected address family");
        abort();
    }
}
