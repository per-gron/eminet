//
//  EmiBinding.mm
//  roshambo
//
//  Created by Per Eckerdal on 2012-04-24.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#include <Security/Security.h>
#include <CommonCrypto/CommonHMAC.h>

void EmiBinding::hmacHash(const uint8_t *key, size_t keyLength,
                          const uint8_t *data, size_t dataLength,
                          uint8_t *buf, size_t bufLen) {
    if (bufLen < 256/8) {
        panic();
    }
    CCHmac(kCCHmacAlgSHA256, key, keyLength, data, dataLength, buf);
}

void EmiBinding::randomBytes(uint8_t *buf, size_t bufSize) {
    if (0 != SecRandomCopyBytes(kSecRandomDefault, bufSize, buf)) {
        panic();
    }
}
