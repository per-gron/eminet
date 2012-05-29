#define BUILDING_NODE_EXTENSION
#ifndef emilir_EmiP2PSocket_h
#define emilir_EmiP2PSocket_h

#include "EmiObjectWrap.h"
#include "EmiBinding.h"

#include "../core/EmiP2PSock.h"
#include <node.h>

class EmiP2PSocket : public EmiObjectWrap {
    typedef EmiP2PSock<EmiBinding> EPS;
    
private:
    EPS _sock;
    v8::Persistent<v8::Object> _jsHandle;
    
    static v8::Persistent<v8::String> connectionTimeoutSymbol;
    static v8::Persistent<v8::String> initialConnectionTimeoutSymbol;
    static v8::Persistent<v8::String> rateLimitSymbol;
    static v8::Persistent<v8::String> typeSymbol;
    static v8::Persistent<v8::String> portSymbol;
    static v8::Persistent<v8::String> addressSymbol;
    static v8::Persistent<v8::String> fabricatedPacketDropRateSymbol;
    
    // Private copy constructor and assignment operator
    inline EmiP2PSocket(const EmiP2PSocket& other);
    inline EmiP2PSocket& operator=(const EmiP2PSocket& other);
    
    EmiP2PSocket(v8::Handle<v8::Object> jsHandle, const EmiP2PSockConfig& sc);
    virtual ~EmiP2PSocket();
    
    static v8::Handle<v8::Value> SetCallbacks(const v8::Arguments& args);
    static v8::Handle<v8::Value> New(const v8::Arguments& args);
    
public:
    static void Init(v8::Handle<v8::Object> target);
    
    static v8::Persistent<v8::Function> connectionError;
    
    inline EPS& getP2PSock() { return _sock; }
    inline const EPS& getP2PSock() const { return _sock; }
    inline v8::Handle<v8::Object> getJsHandle() const { return _jsHandle; }
    
private:
    static v8::Handle<v8::Value> GetAddressType(const v8::Arguments& args);
    static v8::Handle<v8::Value> GetPort(const v8::Arguments& args);
    static v8::Handle<v8::Value> GetAddress(const v8::Arguments& args);
    static v8::Handle<v8::Value> GetRemotePort(const v8::Arguments& args);
    static v8::Handle<v8::Value> GenerateCookiePair(const v8::Arguments& args);
    static v8::Handle<v8::Value> GenerateSharedSecret(const v8::Arguments& args);
};

#endif
