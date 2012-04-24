#define BUILDING_NODE_EXTENSION
#ifndef emilir_EmiSocket_h
#define emilir_EmiSocket_h

#include "EmiSockDelegate.h"
#include "EmiConnDelegate.h"
#include "EmiAddressCmp.h"

#include "../core/EmiP2PSock.h"
#include <node.h>

class EmiP2PSocket : public node::ObjectWrap {
    typedef EmiP2PSock<EmiSockDelegate> EPS;
    
private:
    EPS _sock;
    v8::Persistent<v8::Object> _jsHandle;
    
    static v8::Persistent<v8::String> connectionTimeoutSymbol;
    static v8::Persistent<v8::String> rateLimitSymbol;
    static v8::Persistent<v8::String> typeSymbol;
    static v8::Persistent<v8::String> portSymbol;
    static v8::Persistent<v8::String> addressSymbol;
    static v8::Persistent<v8::String> fabricatedPacketDropRateSymbol;
    
    // Private copy constructor and assignment operator
    inline EmiSocket(const EmiSocket& other);
    inline EmiSocket& operator=(const EmiSocket& other);
    
    EmiP2PSocket(v8::Handle<v8::Object> jsHandle, const EmiP2PSockConfig<EmiSockDelegate::Address>& sc);
    virtual ~EmiP2PSocket();
    
    static v8::Handle<v8::Value> New(const v8::Arguments& args);
    static v8::Handle<v8::Value> Suspend(const v8::Arguments& args);
    static v8::Handle<v8::Value> Desuspend(const v8::Arguments& args);
    
public:
    static void Init(v8::Handle<v8::Object> target);
    
    inline EPS& getP2PSock() { return _sock; }
    inline const EPS& getP2PSock() const { return _sock; }
    inline v8::Handle<v8::Object> getJsHandle() const { return _jsHandle; }
};

#endif
