#define BUILDING_NODE_EXTENSION
#ifndef emilir_EmiConnection_h
#define emilir_EmiConnection_h

#include "EmiSockDelegate.h"
#include "EmiConnDelegate.h"
#include "EmiAddressCmp.h"

#include "../core/EmiConn.h"
#include <node.h>

class EmiConnection : public node::ObjectWrap {
    friend class EmiConnDelegate;
    typedef EmiConn<EmiSockDelegate, EmiConnDelegate> EC;
    
private:
    EC _conn;
    v8::Persistent<v8::Object> _jsHandle;
    
    static v8::Persistent<v8::String>   channelQualifierSymbol;
    static v8::Persistent<v8::String>   prioritySymbol;
    static v8::Persistent<v8::Function> constructor;
    
    // Private copy constructor and assignment operator
    inline EmiConnection(const EmiConnection& other);
    inline EmiConnection& operator=(const EmiConnection& other);
    
    EmiConnection(EmiSocket& es, const struct sockaddr_storage& address, uint16_t inboundPort, bool initiator);
    virtual ~EmiConnection();
    
public:
    static EmiTimeInterval Now();
    
    static void Init(v8::Handle<v8::Object> target);
    
    static v8::Handle<v8::Object> NewInstance(EmiSocket& es,
                                              const struct sockaddr_storage& address,
                                              uint16_t inboundPort,
                                              bool initiator);
    
    inline EC& getConn() { return _conn; }
    inline const EC& getConn() const { return _conn; }
    
    inline void setJsHandle(v8::Handle<v8::Object> jsHandle) {
        _jsHandle.Dispose();
        _jsHandle = v8::Persistent<v8::Object>::New(jsHandle);
    }
    
private:
    static v8::Handle<v8::Value> New(const v8::Arguments& args);
    static v8::Handle<v8::Value> Close(const v8::Arguments& args);
    static v8::Handle<v8::Value> ForceClose(const v8::Arguments& args);
    static v8::Handle<v8::Value> CloseOrForceClose(const v8::Arguments& args);
    static v8::Handle<v8::Value> Flush(const v8::Arguments& args);
    static v8::Handle<v8::Value> Send(const v8::Arguments& args);
    
    static v8::Handle<v8::Value> HasIssuedConnectionWarning(const v8::Arguments& args);
    static v8::Handle<v8::Value> GetSocket(const v8::Arguments& args);
    static v8::Handle<v8::Value> GetAddressType(const v8::Arguments& args);
    static v8::Handle<v8::Value> GetPort(const v8::Arguments& args);
    static v8::Handle<v8::Value> GetAddress(const v8::Arguments& args);
    static v8::Handle<v8::Value> GetInboundPort(const v8::Arguments& args);
    static v8::Handle<v8::Value> IsOpen(const v8::Arguments& args);
    static v8::Handle<v8::Value> IsOpening(const v8::Arguments& args);
};

#endif
