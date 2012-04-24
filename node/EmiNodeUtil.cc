#define BUILDING_NODE_EXTENSION

#include "EmiNodeUtil.h"

#include "../core/EmiNetUtil.h"
#include <netinet/in.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <uv.h>

void EmiNodeUtil::parseIp(const char* host,
                          uint16_t port,
                          int family,
                          sockaddr_storage *out) {
    if (AF_INET == family) {
        struct sockaddr_in address4(uv_ip4_addr(host, port));
        memcpy(out, &address4, sizeof(struct sockaddr_in));
    }
    else if (AF_INET6 == family) {
        struct sockaddr_in6 address6(uv_ip6_addr(host, port));
        memcpy(out, &address6, sizeof(struct sockaddr_in));
    }
    else {
        ASSERT(0 && "unexpected address family");
        abort();
    }
}

void EmiNodeUtil::anyIp(uint16_t port,
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
		addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		memset(&(addr.sin_zero), 0, sizeof(addr.sin_zero));
    }
    else {
        ASSERT(0 && "unexpected address family");
        abort();
    }
}

bool EmiNodeUtil::parseAddressFamily(const char* typeStr, int *family) {
    if (0 == strcmp(typeStr, "udp4")) {
        *family = AF_INET;
        return true;
    }
    else if (0 == strcmp(typeStr, "udp6")) {
        *family = AF_INET6;
        return true;
    }
    else {
        return false;
    }
}
