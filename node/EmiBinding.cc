#define BUILDING_NODE_EXTENSION

#include "EmiBinding.h"

#include "EmiNodeUtil.h"
#include "EmiConnection.h"

#include <node.h>
#include <openssl/rand.h>
#include <openssl/hmac.h>

using namespace v8;

void EmiBinding::fillNilAddress(int family, Address& address) {
    EmiNodeUtil::anyIp(/*port:*/0, family, &address);
}

bool EmiBinding::isNilAddress(const Address& address) {
    return 0 != ntohs(EmiNodeUtil::extractPort(address));
}

EmiBinding::Address EmiBinding::makeAddress(int family, const uint8_t *ip, size_t ipLen, uint16_t port) {
    Address addr;
    EmiNodeUtil::makeAddress(family, ip, ipLen, port, &addr);
    return addr;
}

int EmiBinding::extractFamily(const Address& address) {
    return address.ss_family;
}

size_t EmiBinding::ipLength(const Address& address) {
    int family = extractFamily(address);
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

size_t EmiBinding::extractIp(const Address& address, uint8_t *buf, size_t bufSize) {
    int family = extractFamily(address);
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

uint16_t EmiBinding::extractPort(const Address& address) {
    return EmiNodeUtil::extractPort(address);
}

Persistent<Object> EmiBinding::makePersistentData(const Local<Object>& data,
                                                  size_t offset,
                                                  size_t length) {
    HandleScope scope;
    
    // Copy the buffer
    node::Buffer *buf(node::Buffer::New(node::Buffer::Data(data)+offset,
                                        length));
    
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
