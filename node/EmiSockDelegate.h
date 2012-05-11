#define BUILDING_NODE_EXTENSION
#ifndef emilir_EmiSockDelegate_h
#define emilir_EmiSockDelegate_h

#include "EmiError.h"
#include "EmiBinding.h"

#include "../core/EmiTypes.h"
#include <node.h>
#include <uv.h>

class EmiSocket;
class EmiSockDelegate;
class EmiConnDelegate;
class EmiAddressCmp;
template<class SockDelegate, class ConnDelegate>
class EmiSock;
template<class SockDelegate, class ConnDelegate>
class EmiConn;
template<class Binding>
class EmiConnParams;

class EmiSockDelegate {
    typedef EmiSock<EmiSockDelegate, EmiConnDelegate> ES;
    typedef EmiConn<EmiSockDelegate, EmiConnDelegate> EC;
    
    typedef EmiBinding::Error   Error;
    
    EmiSocket& _es;
    
    friend class EmiBinding;
public:
    
    typedef EmiBinding                 Binding;
    typedef v8::Persistent<v8::Object> ConnectionOpenedCallbackCookie;
    
    EmiSockDelegate(EmiSocket& es);
    
    EC *makeConnection(const EmiConnParams<EmiBinding>& params);
    void gotConnection(EC& conn);
    
    static void connectionOpened(ConnectionOpenedCallbackCookie& cookie,
                                 bool error,
                                 EmiDisconnectReason reason,
                                 EC& ec);
    
    inline EmiSocket& getEmiSocket() { return _es; }
    inline const EmiSocket& getEmiSocket() const { return _es; }
};

#endif
