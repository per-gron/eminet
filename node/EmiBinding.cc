#define BUILDING_NODE_EXTENSION

#include "EmiBinding.h"

#include "../core/EmiNetUtil.h"
#include "EmiNodeUtil.h"
#include "EmiConnection.h"
#include "EmiSocket.h"
#include "EmiObjectWrap.h"

#include <node.h>
#include <openssl/rand.h>
#include <openssl/hmac.h>

using namespace v8;

void EmiBinding::fillNilAddress(int family, Address& address) {
    EmiNetUtil::anyAddr(/*port:*/0, family, &address);
}

bool EmiBinding::isNilAddress(const Address& address) {
    return 0 != ntohs(EmiNetUtil::addrPortN(address));
}

Persistent<Object> EmiBinding::makePersistentData(const uint8_t *data, size_t length) {
    HandleScope scope;
    
    // TODO It would probably be better to use the slab allocator here
    
    // Copy the buffer
    node::Buffer *buf(node::Buffer::New((char *)data, length));
    
    // Make a new persistent handle (do not just reuse the persistent buf->handle_ handle)
    return Persistent<Object>::New(buf->handle_);
}

void EmiBinding::hmacHash(const uint8_t *key, size_t keyLength,
                          const uint8_t *data, size_t dataLength,
                          uint8_t *buf, size_t bufLen) {
    unsigned int bufLenInt = bufLen;
    ASSERT(HMAC(EVP_sha256(), key, keyLength, data, dataLength, buf, &bufLenInt));
}

void EmiBinding::randomBytes(uint8_t *buf, size_t bufSize) {
    // TODO I'm not sure this is actually secure. Double check this.
    ASSERT(RAND_bytes(buf, bufSize));
}

static void close_cb(uv_handle_t* handle) {
    free(handle);
}

static void timer_cb(uv_timer_t *handle, int status) {
    EmiBinding::TimerCb *timerCb = *(reinterpret_cast<EmiBinding::TimerCb**>(handle+1));
    timerCb(EmiConnection::Now(), handle, handle->data);
}

EmiBinding::Timer *EmiBinding::makeTimer() {
    EmiBinding::Timer *timer = (uv_timer_t *)malloc(sizeof(uv_timer_t)+sizeof(TimerCb));
    
    uv_timer_init(uv_default_loop(), timer);
    
    return timer;
}

void EmiBinding::freeTimer(Timer *timer) {
    uv_timer_stop(timer);
    uv_close((uv_handle_t *)timer, close_cb);
}

void EmiBinding::scheduleTimer(Timer *timer, TimerCb *timerCb, void *data, EmiTimeInterval interval, bool repeating) {
    uv_timer_stop(timer);
    
    *(reinterpret_cast<TimerCb**>(timer+1)) = timerCb;
    
    uint64_t timeout = interval*EmiNodeUtil::MSECS_PER_SEC;
    timer->data = data;
    uv_timer_start(timer,
                   timer_cb,
                   timeout,
                   repeating ? timeout : 0);
}

void EmiBinding::descheduleTimer(Timer *timer) {
    uv_timer_stop(timer);
}

bool EmiBinding::timerIsActive(Timer *timer) {
    return uv_is_active((uv_handle_t *)timer);
}

// TODO Begin to use this code once stable node has libuv with uv_interface_address_t
#if NODES_LIBV_HAS_UV_INTERFACE_ADDRESS_T

bool EmiBinding::getNetworkInterfaces(NetworkInterfaces& ni, Error& err) {
    uv_err_t ret = uv_interface_addresses(&ni.first, &ni.second.first);
    if (0 != ret.code) {
        err = makeError("com.emilir.eminet.networkifaces", 0);
        return false;
    }
    
    ni.second.second = 0;
    
    return true;
}

bool EmiBinding::nextNetworkInterface(NetworkInterfaces& ni, const char*& name, struct sockaddr_storage& addr) {
    if (/*interface iterator index*/ni.second.second >= /*interface count*/ni.second.first) {
        return false;
    }
    
    uv_interface_addresses& addr(ni.first[ni.second.second]);
    
    ni.second.second += 1;
    
    if (addr.is_internal) {
        // This is a loopback interface. Continue the search.
        return nextNetworkInterface(ni, name, addr);
    }
    
    int family = ((sockaddr *)&addr.address)->sa_family;
    if (AF_INET == family) {
        memcpy(&addr, addr.address.address4, sizeof(sockaddr_in));
    }
    else if (AF_INET6 == family) {
        memcpy(&addr, addr.address.address6, sizeof(sockaddr_in6));
    }
    else {
        ASSERT(0 && "unexpected address family");
        abort();
    }
    
    name = ifa->ifa_name;
    
    return true;
}

void EmiBinding::freeNetworkInterfaces(const NetworkInterfaces& ni) {
    freeifaddrs(ni.first);
}

#else // ... until then, use less platform independent code

bool EmiBinding::getNetworkInterfaces(NetworkInterfaces& ni, Error& err) {
    int ret = getifaddrs(&ni.first);
    if (-1 == ret) {
        err = makeError("com.emilir.eminet.networkifaces", 0);
        return false;
    }
    
    ni.second = ni.first;
    return true;
}

bool EmiBinding::nextNetworkInterface(NetworkInterfaces& ni, const char*& name, struct sockaddr_storage& addr) {
    ifaddrs *ifa = ni.second;
    if (!ifa) {
        return false;
    }
    
    ni.second = ifa->ifa_next;
    
    // Use this line to ignore loopback interfaces
    // bool loopback = !!(ifa->ifa_flags & IFF_LOOPBACK);
    // Use this line to not ignore looback interfaces
    bool loopback = false;
    
    int family = ifa->ifa_addr->sa_family;
    if (AF_INET == family) {
        memcpy(&addr, ifa->ifa_addr, sizeof(sockaddr));
    }
    else if (AF_INET6 == family) {
        memcpy(&addr, ifa->ifa_addr, sizeof(sockaddr_storage));
    }
    else {
        // Some other address family that we don't support or care about. Continue the search.
        return nextNetworkInterface(ni, name, addr);
    }
    
    name = ifa->ifa_name;
    
    return true;
}

void EmiBinding::freeNetworkInterfaces(const NetworkInterfaces& ni) {
    freeifaddrs(ni.first);
}

#endif


struct EmiBindingSockData {
    EmiBinding::EmiOnMessage *callback;
    void *userData;
    EmiObjectWrap *jsObj;
};

static void recv_cb(uv_udp_t *socket,
                    const struct sockaddr_storage& addr,
                    ssize_t nread,
                    const v8::Local<v8::Object>& slab,
                    size_t offset) {
    HandleScope scope;
    
    EmiBindingSockData *ebsd = reinterpret_cast<EmiBindingSockData *>(socket->data);
    
    if (nread < 0) {
        const unsigned argc = 2;
        Handle<Value> argv[argc] = {
            ebsd->jsObj->handle_,
            EmiNodeUtil::errStr(uv_last_error(uv_default_loop()))
        };
        EmiSocket::connectionError->Call(Context::GetCurrent()->Global(), argc, argv);
        return;
    }
    if (nread > 0) {
        ebsd->callback(socket,
                       ebsd->userData,
                       EmiConnection::Now(),
                       addr,
                       slab,
                       offset,
                       nread);
    }
}

void EmiBinding::closeSocket(uv_udp_t *socket) {
    EmiBindingSockData *ebsd = (EmiBindingSockData *)socket->data;
    
    // This allows V8's GC to reclaim the EmiSocket when no UDP sockets are open
    //
    // TODO Perhaps I should do this on the next uv tick, since this might dealloc
    // the whole socket, which will probably not end up well.
    //
    // TODO What happens when this method is actually called from _es's destructor?
    ebsd->jsObj->Unref();
    
    EmiNodeUtil::closeSocket(socket);
    
    free(ebsd);
}

uv_udp_t *EmiBinding::openSocket(EmiObjectWrap* jsObj,
                                 EmiOnMessage *callback,
                                 void *userData,
                                 const sockaddr_storage& address,
                                 Error& err) {
    EmiBindingSockData *ebsd = (EmiBindingSockData *)malloc(sizeof(EmiBindingSockData));
    ebsd->callback = callback;
    ebsd->userData = userData;
    ebsd->jsObj = jsObj;
    
    uv_udp_t *ret(EmiNodeUtil::openSocket(address,
                                          recv_cb, ebsd,
                                          err));
    
    if (ret) {
        // This prevents V8's GC to reclaim the EmiSocket while UDP sockets are open
        ebsd->jsObj->Ref();
    }
    
    return ret;
}

void EmiBinding::extractLocalAddress(uv_udp_t *socket, sockaddr_storage& address) {
    int len(sizeof(sockaddr_storage));
    uv_udp_getsockname(socket, (struct sockaddr *)&address, &len);
}

void EmiBinding::sendData(uv_udp_t *socket,
                               const sockaddr_storage& address,
                               const uint8_t *data,
                               size_t size) {
    EmiNodeUtil::sendData(socket, address, data, size);
}
