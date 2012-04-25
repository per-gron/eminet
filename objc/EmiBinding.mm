//
//  EmiBinding.mm
//  roshambo
//
//  Created by Per Eckerdal on 2012-04-24.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#include "EmiBinding.h"

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


@interface EmiBindingTimer : NSObject {
    EmiBinding::Timer *_timer;
}

- (id)initWithTimer:(EmiBinding::Timer *)timer;

- (void)timerFired:(NSTimer *)timer;

@end

@implementation EmiBindingTimer

- (id)initWithTimer:(EmiBinding::Timer *)timer {
    if (self = [super init]) {
        _timer = timer;
    }
    
    return self;
}

- (void)timerFired:(NSTimer *)nsTimer {
    _timer->timerCb([NSDate timeIntervalSinceReferenceDate], _timer, _timer->data);
}

@end

EmiBinding::Timer *EmiBinding::makeTimer(TimerCb *timerCb) {
    return new Timer(timerCb);
}

void EmiBinding::freeTimer(Timer *timer) {
    delete timer;
}

void EmiBinding::scheduleTimer(Timer *timer, void *data, EmiTimeInterval interval, bool repeating) {
    [timer->timer invalidate];
    timer->data = data;
    
    EmiBindingTimer *ebt = [[EmiBindingTimer alloc] initWithTimer:timer];
    
    timer->timer = [NSTimer scheduledTimerWithTimeInterval:interval
                                                    target:ebt
                                                  selector:@selector(timerFired:)
                                                  userInfo:nil
                                                   repeats:repeating];
}

void EmiBinding::descheduleTimer(Timer *timer) {
    [timer->timer invalidate];
    timer->timer = nil;
}

bool EmiBinding::timerIsActive(Timer *timer) {
    return timer->timer && [timer->timer isValid];
}
