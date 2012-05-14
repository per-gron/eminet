#define BUILDING_NODE_EXTENSION
#ifndef emilir_EmiBinding_h
#define emilir_EmiBinding_h

#include "EmiError.h"

#include "../core/EmiTypes.h"
#include <node.h>
#include <node_buffer.h>
#include <uv.h>

#define NODES_LIBV_HAS_UV_INTERFACE_ADDRESS_T 0

#if !NODES_LIBV_HAS_UV_INTERFACE_ADDRESS_T
#include <ifaddrs.h>
#include <net/if.h>
#endif

class EmiSockDelegate;
class EmiObjectWrap;

class EmiBinding {
private:
    inline EmiBinding();
    
public:
    
    typedef EmiError                   Error;
    typedef uv_udp_t                   SocketHandle;
    typedef struct sockaddr_storage    Address;
    typedef v8::Local<v8::Object>      TemporaryData;
    typedef v8::Persistent<v8::Object> PersistentData;
    typedef uv_timer_t                 Timer;
    typedef void (TimerCb)(EmiTimeInterval now, Timer *timer, void *data);
    
    typedef void (EmiOnMessage)(uv_udp_t *socket,
                                void *userData,
                                EmiTimeInterval now,
                                const sockaddr_storage& address,
                                const TemporaryData& data,
                                size_t offset,
                                size_t len);
    
    // Will fill address with an address that cannot be the receiver of a packet
    static void fillNilAddress(int family, Address& address);
    static bool isNilAddress(const Address& address);
    
    inline static EmiError makeError(const char *domain, int32_t code) {
        return EmiError(domain, code);
    }
    
    static v8::Persistent<v8::Object> makePersistentData(const uint8_t *data, size_t length);
    inline static void releasePersistentData(v8::Persistent<v8::Object> buf) {
        buf.Dispose();
    }
    inline static v8::Local<v8::Object> castToTemporary(const v8::Persistent<v8::Object>& data) {
        return v8::Local<v8::Object>::New(data);
    }
    
    inline static const uint8_t *extractData(v8::Handle<v8::Object> data) {
        return (const uint8_t *)(data.IsEmpty() ? NULL : node::Buffer::Data(data));
    }
    inline static size_t extractLength(v8::Handle<v8::Object> data) {
        return data.IsEmpty() ? 0 : node::Buffer::Length(data);
    }
    
    static const size_t HMAC_HASH_SIZE = 32;
    static void hmacHash(const uint8_t *key, size_t keyLength,
                         const uint8_t *data, size_t dataLength,
                         uint8_t *buf, size_t bufLen);
    static void randomBytes(uint8_t *buf, size_t bufSize);
    
    static Timer *makeTimer();
    static void freeTimer(Timer *timer);
    static void scheduleTimer(Timer *timer, TimerCb *timerCb, void *data, EmiTimeInterval interval, bool repeating);
    static void descheduleTimer(Timer *timer);
    static bool timerIsActive(Timer *timer);
    
    // TODO Begin to use this code once stable node has libuv with uv_interface_address_t
#if NODES_LIBV_HAS_UV_INTERFACE_ADDRESS_T
    typedef std::pair<uv_interface_address_t*, std::pair</*count*/int, /*idx*/int> > NetworkInterfaces;
    static bool getNetworkInterfaces(NetworkInterfaces& ni, Error& err);
    static bool nextNetworkInterface(NetworkInterfaces& ni, const char*& name, struct sockaddr_storage& addr);
    static void freeNetworkInterfaces(const NetworkInterfaces& ni);
#else // ... until then, use less platform independent code
    typedef std::pair<ifaddrs*, ifaddrs*> NetworkInterfaces;
    static bool getNetworkInterfaces(NetworkInterfaces& ni, Error& err);
    static bool nextNetworkInterface(NetworkInterfaces& ni, const char*& name, struct sockaddr_storage& addr);
    static void freeNetworkInterfaces(const NetworkInterfaces& ni);
#endif
    
    static void closeSocket(uv_udp_t *socket);
    static uv_udp_t *openSocket(EmiObjectWrap *jsObj,
                                EmiOnMessage *callback,
                                void *userData,
                                const sockaddr_storage& address,
                                Error& err);
    static void extractLocalAddress(uv_udp_t *socket, sockaddr_storage& address);
    static void sendData(uv_udp_t *socket,
                         const sockaddr_storage& address,
                         const uint8_t *data,
                         size_t size);
};

#endif
