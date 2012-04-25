#define BUILDING_NODE_EXTENSION

#include "EmiBinding.h"

#include "EmiNodeUtil.h"
#include "EmiConnection.h"

#include <stdexcept>
#include <node.h>
#include <openssl/rand.h>
#include <openssl/hmac.h>

using namespace v8;

void EmiBinding::fillNilAddress(int family, Address& address) {
    EmiNodeUtil::anyIp(/*port:*/0, family, &address);
}

bool EmiBinding::isNilAddress(const Address& address) {
    return 0 != EmiNodeUtil::extractPort(address);
}

int EmiBinding::extractFamily(const Address& address) {
    return address.ss_family;
}

void EmiBinding::panic() {
    throw std::runtime_error("EmiNet internal error");
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
    if (!HMAC(EVP_sha256(), key, keyLength, data, dataLength, buf, &bufLenInt)) {
        panic();
    }
}

void EmiBinding::randomBytes(uint8_t *buf, size_t bufSize) {
    // TODO I'm not sure this is actually secure. Double check this.
    if (!RAND_bytes(buf, bufSize)) {
        panic();
    }
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
