#define BUILDING_NODE_EXTENSION
#ifndef eminet_EmiSockDelegate_h
#define eminet_EmiSockDelegate_h

#include "EmiError.h"
#include "EmiBinding.h"

#include "../core/EmiTypes.h"
#include <node.h>
#include <uv.h>

class EmiObjectWrap;
class EmiSocket;
class EmiSockDelegate;
class EmiConnDelegate;
template<class SockDelegate, class ConnDelegate>
class EmiSock;
template<class SockDelegate, class ConnDelegate>
class EmiConn;
template<class Binding>
class EmiConnParams;
template<class Binding>
class EmiUdpSocket;

class EmiSockDelegate {
    typedef EmiConn<EmiSockDelegate, EmiConnDelegate> EC;
    
    typedef EmiBinding::Error   Error;
    
    EmiSocket& _es;
    
    friend class EmiBinding;
public:
    
    typedef EmiBinding                 Binding;
    typedef v8::Persistent<v8::Object> ConnectionOpenedCallbackCookie;
    
    EmiSockDelegate(EmiSocket& es);
    
    EC *makeConnection(const EmiConnParams<EmiBinding>& params);
    void gotServerConnection(EC& conn);
    
    static void connectionOpened(ConnectionOpenedCallbackCookie& cookie,
                                 bool error,
                                 EmiDisconnectReason reason,
                                 EC& ec);
    
    void connectionGotMessage(EC *conn,
                              EmiUdpSocket<EmiBinding> *socket,
                              EmiTimeInterval now,
                              const sockaddr_storage& inboundAddress,
                              const sockaddr_storage& remoteAddress,
                              const EmiBinding::TemporaryData& data,
                              size_t offset,
                              size_t len);
    
    inline EmiSocket& getEmiSocket() { return _es; }
    inline const EmiSocket& getEmiSocket() const { return _es; }
    
    inline EmiObjectWrap *getSocketCookie() { return (EmiObjectWrap *)&_es; }
};

#endif
