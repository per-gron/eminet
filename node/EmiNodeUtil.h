#define BUILDING_NODE_EXTENSION
#ifndef emilir_EmiNodeUtil_h
#define emilir_EmiNodeUtil_h

#include <stdint.h>

struct sockaddr_storage;

#define THROW_TYPE_ERROR(err)                                 \
  do {                                                        \
    ThrowException(Exception::TypeError(String::New(err)));   \
    return scope.Close(Undefined());                          \
  } while (0)

#define ENSURE_NUM_ARGS(num, args)                            \
  do {                                                        \
    if (num != args.Length()) {                               \
      THROW_TYPE_ERROR("Wrong number of arguments");          \
    }                                                         \
  } while (0)

#define ENSURE_ZERO_ARGS(args) ENSURE_NUM_ARGS(0, args)

#define UNWRAP(type, name, args) type *name(ObjectWrap::Unwrap<type>(args.This()))

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
