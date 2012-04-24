#define BUILDING_NODE_EXTENSION

#include "EmiBinding.h"

#include "EmiNodeUtil.h"

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
