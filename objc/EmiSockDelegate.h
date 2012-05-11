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

#include "EmiTypes.h"
#import <Foundation/Foundation.h>

struct sockaddr_storage;

@class EmiSocket;
@class EmiConnection;
@class GCDAsyncUdpSocket;
class EmiSockDelegate;
class EmiConnDelegate;
class EmiAddressCmp;
template<class SockDelegate, class ConnDelegate>
class EmiSock;
template<class SockDelegate, class ConnDelegate>
class EmiConn;
template<class SockDelegate>
class EmiConnParams;

class EmiSockDelegate {
    typedef EmiSock<EmiSockDelegate, EmiConnDelegate> ES;
    typedef EmiConn<EmiSockDelegate, EmiConnDelegate> EC;
    
    EmiSocket *_socket;
public:
    
    typedef EmiBinding Binding;
    typedef void (^__strong ConnectionOpenedCallbackCookie)(NSError *err, EmiConnection *connection);
    
    EmiSockDelegate(EmiSocket *socket);
        
    EC *makeConnection(const EmiConnParams<EmiBinding>& params);
    
    void gotConnection(EC& conn);
    
    static void connectionOpened(ConnectionOpenedCallbackCookie& cookie, bool error, EmiDisconnectReason reason, EC& ec);
};

#endif
