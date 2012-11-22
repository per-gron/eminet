//
//  EmiSocketUserDataWrapper.h
//  eminet
//
//  Created by Per Eckerdal on 2012-05-11.
//  Copyright (c) 2012 Per Eckerdal. All rights reserved.
//

#include "EmiTypes.h"
#import <Foundation/Foundation.h>
#include <netinet/in.h>

@class GCDAsyncUdpSocket;

typedef NSData* EmiOnMessageTemporaryData;
typedef void (EmiOnMessage)(GCDAsyncUdpSocket *socket,
                            void *userData,
                            EmiTimeInterval now,
                            const sockaddr_storage& address,
                            const __unsafe_unretained EmiOnMessageTemporaryData& data,
                            size_t offset,
                            size_t len);

@interface EmiSocketUserDataWrapper : NSObject {
    void         *_userData;
    EmiOnMessage *_callback;
}

- (id)initWithUserData:(void *)userData callback:(EmiOnMessage *)callback;

@property (nonatomic, assign) void *userData;
@property (nonatomic, assign) EmiOnMessage *callback;

@end
