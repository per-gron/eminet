//
//  EmiSockDelegate.h
//  roshambo
//
//  Created by Per Eckerdal on 2012-04-12.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#ifndef emilir_EmiSockDelegate_h
#define emilir_EmiSockDelegate_h

#include "EmiBinding.h"
#import "EmiDispatchQueueWrapper.h"

#include "EmiTypes.h"
#import <Foundation/Foundation.h>

struct sockaddr_storage;

@class EmiSocket;
@class EmiConnection;
@class GCDAsyncUdpSocket;
class EmiSockDelegate;
class EmiConnDelegate;
template<class SockDelegate, class ConnDelegate>
class EmiSock;
template<class SockDelegate, class ConnDelegate>
class EmiConn;
template<class SockDelegate>
class EmiConnParams;
template<class Binding>
class EmiUdpSocket;

class EmiSockDelegate {
    typedef EmiSock<EmiSockDelegate, EmiConnDelegate> ES;
    typedef EmiConn<EmiSockDelegate, EmiConnDelegate> EC;
    
    EmiSocket *_socket;
    dispatch_group_t _dispatchGroup;
public:
    
    typedef EmiBinding Binding;
    typedef void (^__strong ConnectionOpenedCallbackCookie)(NSError *err, EmiConnection *connection);
    
    EmiSockDelegate(EmiSocket *socket);
    virtual ~EmiSockDelegate();
    
    EmiSockDelegate(const EmiSockDelegate& other);
    EmiSockDelegate& operator=(const EmiSockDelegate& other);
        
    EC *makeConnection(const EmiConnParams<EmiBinding>& params);
    
    void gotServerConnection(EC& conn);
    
    static void connectionOpened(ConnectionOpenedCallbackCookie& cookie, bool error, EmiDisconnectReason reason, EC& ec);
    
    void connectionGotMessage(EC *conn,
                              EmiUdpSocket<EmiBinding> *socket,
                              EmiTimeInterval now,
                              const sockaddr_storage& inboundAddress,
                              const sockaddr_storage& remoteAddress,
                              NSData *data,
                              size_t offset,
                              size_t len);
    
    // This method will always be called from the socketqueue
    inline EmiDispatchQueueWrapper *getSocketCookie() {
        return [[EmiDispatchQueueWrapper alloc] initWithQueue:dispatch_get_current_queue()];
    }
};

#endif
