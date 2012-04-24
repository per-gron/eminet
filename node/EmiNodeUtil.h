#define BUILDING_NODE_EXTENSION
#ifndef emilir_EmiNodeUtil_h
#define emilir_EmiNodeUtil_h

#include <stdint.h>

struct sockaddr_storage;

class EmiNodeUtil {
public:
    static const uint64_t NSECS_PER_SEC = 1000*1000*1000;
    static const uint64_t MSECS_PER_SEC = 1000;
    
    static void parseIp(const char* host,
                        uint16_t port,
                        int family,
                        sockaddr_storage *out);
    
    static void anyIp(uint16_t port,
                      int family,
                      sockaddr_storage *out);
    
    static bool parseAddressFamily(const char* typeStr, int *family);
};

#endif
