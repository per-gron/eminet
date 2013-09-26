//
//  EmiSockDelegate.h
//  eminet
//
//  Created by Per Eckerdal on 2012-04-12.
//  Copyright (c) 2012 Per Eckerdal. All rights reserved.
//

#ifndef eminet_EmiSockDelegate_h
#define eminet_EmiSockDelegate_h

#include "EmiBinding.h"

#include "EmiTypes.h"
#import <Foundation/Foundation.h>

struct sockaddr_storage;

@class EmiSocket;
@class EmiConnection;
@class GCDAsyncUdpSocket;
@class EmiConnectionOpenedBlockWrapper;
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
    typedef EmiConnectionOpenedBlockWrapper *__strong ConnectionOpenedCallbackCookie;
    
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
                              sockaddr_storage inboundAddress,
                              sockaddr_storage remoteAddress,
                              NSData *data,
                              size_t offset,
                              size_t len);
    
    // This method will always be called from the socketqueue
    dispatch_queue_t getSocketCookie();
};

#endif
