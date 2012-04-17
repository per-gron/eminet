//
//  EmiSockDelegate.h
//  roshambo
//
//  Created by Per Eckerdal on 2012-04-12.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#ifndef emilir_EmiSockDelegate_h
#define emilir_EmiSockDelegate_h

#import <Foundation/Foundation.h>

@class EmiSocket;
@class EmiConnection;
@class GCDAsyncUdpSocket;
class EmiSockDelegate;
class EmiConnDelegate;
class EmiAddressCmp;
template<class SockDelegate, class ConnDelegate>
class EmiConn;

typedef EmiConn<EmiSockDelegate, EmiConnDelegate> EC;

class EmiSockDelegate {
    EmiSocket *_socket;
public:
    
    typedef __strong NSError* Error;
    typedef __strong EmiConnection* ConnectionHandle;
    typedef EmiAddressCmp AddressCmp;
    typedef GCDAsyncUdpSocket Socket;
    typedef NSData* Address;
    typedef NSData* Data;
    
    EmiSockDelegate(EmiSocket *socket);
    
    static void closeSocket(GCDAsyncUdpSocket *socket);
    GCDAsyncUdpSocket *openSocket(uint16_t port, Error& err);
    
    static EC *extractConn(EmiConnection *conn);
    EmiConnection *makeConnection(NSData *address, uint16_t inboundPort, bool initiator);
    
    void sendData(GCDAsyncUdpSocket *socket, NSData *address, const uint8_t *data, size_t size);
    void gotConnection(EmiConnection *conn);
    
    inline static void panic() {
        [NSException raise:@"EmiNetPanic" format:@"EmiNet internal error"];
    }
    
    inline static NSError *makeError(const char *domain, int32_t code) {
        return [NSError errorWithDomain:[NSString stringWithCString:domain encoding:NSUTF8StringEncoding]
                                   code:code
                               userInfo:nil];
    }
    
    inline static const uint8_t *extractData(NSData *data) {
        return (const uint8_t *)[data bytes];
    }
    inline static size_t extractLength(NSData *data) {
        return [data length];
    }
};

#endif
