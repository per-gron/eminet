#define BUILDING_NODE_EXTENSION
#ifndef emilir_EmiNodeUtil_h
#define emilir_EmiNodeUtil_h

#include <stdint.h>
#include <uv.h>
#include <node.h>

struct sockaddr_storage;
class EmiError;

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

#define CHECK_CONFIG_PARAM(sym, pred)                                    \
    do {                                                                 \
        if (sym.IsEmpty() || !sym->pred()) {                             \
            THROW_TYPE_ERROR("Invalid socket configuration parameters"); \
        }                                                                \
    } while (0)

#define HAS_CONFIG_PARAM(sym) (!sym.IsEmpty() && !sym->IsUndefined())

#define READ_CONFIG(sc, sym, pred, type, cast)                   \
    do {                                                         \
        if (HAS_CONFIG_PARAM(sym)) {                             \
            CHECK_CONFIG_PARAM(sym, pred);                       \
            sc.sym = (type) sym->cast();                         \
        }                                                        \
    } while (0)


#define READ_FAMILY_CONFIG(family, type, scope)                                           \
    do {                                                                                  \
        if (HAS_CONFIG_PARAM(type)) {                                                     \
            CHECK_CONFIG_PARAM(type, IsString);                                           \
                                                                                          \
            String::Utf8Value typeStr(type);                                              \
            if (!EmiNodeUtil::parseAddressFamily(*typeStr, &family)) {                    \
                ThrowException(Exception::Error(String::New("Unknown address family")));  \
                return scope.Close(Undefined());                                          \
            }                                                                             \
        }                                                                                 \
        else {                                                                            \
            family = AF_INET;                                                             \
        }                                                                                 \
    } while (0)


#define READ_ADDRESS_CONFIG(sc, family, addr)                           \
    do {                                                                \
        if (HAS_CONFIG_PARAM(addr)) {                                   \
            CHECK_CONFIG_PARAM(addr, IsString);                         \
            String::Utf8Value host(addr);                               \
            EmiNodeUtil::parseIp(*host, sc.port, family, &sc.address);  \
        }                                                               \
        else {                                                          \
            EmiNodeUtil::anyIp(sc.port, family, &sc.address);           \
        }                                                               \
    } while (0)

class EmiNodeUtil {
private:
    // Private default constructor; this class only has static
    // methods and is not intended to have any instances.
    inline EmiNodeUtil();
public:
    typedef void (EmiNodeUtilRecvCb)(uv_udp_t *socket,
                                     const struct sockaddr_storage& addr,
                                     ssize_t nread,
                                     const v8::Local<v8::Object>& slab,
                                     size_t offset);
    
    static const uint64_t NSECS_PER_SEC = 1000*1000*1000;
    static const uint64_t MSECS_PER_SEC = 1000;
    
    static void parseIp(const char* host,
                        uint16_t port,
                        int family,
                        sockaddr_storage *out);
    // The IP and port number should be in network byte order
    static void makeAddress(int family, const uint8_t *ip, size_t ipLen, uint16_t port, sockaddr_storage *out);
    // Returns the port number in network byte order
    static uint16_t extractPort(const sockaddr_storage& addr);
    static void anyIp(uint16_t port,
                      int family,
                      sockaddr_storage *out);
    
    static bool parseAddressFamily(const char* typeStr, int *family);
    
    static v8::Handle<v8::String> errStr(uv_err_t err);
    
    static void closeSocket(uv_udp_t *socket);
    static uv_udp_t *openSocket(const sockaddr_storage& address,
                                uint16_t port,
                                EmiNodeUtilRecvCb *recvCb,
                                void *data,
                                EmiError& error);
    static void sendData(uv_udp_t *socket,
                         const sockaddr_storage& address,
                         const uint8_t *data,
                         size_t size);

};

#endif
