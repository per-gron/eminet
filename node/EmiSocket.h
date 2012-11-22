#define BUILDING_NODE_EXTENSION
#ifndef eminet_EmiSocket_h
#define eminet_EmiSocket_h

#include "EmiBinding.h"
#include "EmiSockDelegate.h"
#include "EmiConnDelegate.h"
#include "EmiObjectWrap.h"

#include "../core/EmiSock.h"
#include "../core/EmiConn.h"
#include <node.h>

class EmiSocket : public EmiObjectWrap {
    typedef EmiSock<EmiSockDelegate, EmiConnDelegate> EmiS;
    
    friend class EmiConnDelegate;
    friend class EmiSockDelegate;
    friend class EmiBinding;
    
private:
    EmiS _sock;
    v8::Persistent<v8::Object> _jsHandle;
    
    static v8::Persistent<v8::String> mtuSymbol;
    static v8::Persistent<v8::String> heartbeatFrequencySymbol;
    static v8::Persistent<v8::String> heartbeatsBeforeConnectionWarningSymbol;
    static v8::Persistent<v8::String> connectionTimeoutSymbol;
    static v8::Persistent<v8::String> initialConnectionTimeoutSymbol;
    static v8::Persistent<v8::String> receiverBufferSizeSymbol;
    static v8::Persistent<v8::String> senderBufferSizeSymbol;
    static v8::Persistent<v8::String> acceptConnectionsSymbol;
    static v8::Persistent<v8::String> typeSymbol;
    static v8::Persistent<v8::String> portSymbol;
    static v8::Persistent<v8::String> addressSymbol;
    static v8::Persistent<v8::String> fabricatedPacketDropRateSymbol;
    
    // Private copy constructor and assignment operator
    inline EmiSocket(const EmiSocket& other);
    inline EmiSocket& operator=(const EmiSocket& other);
    
    EmiSocket(v8::Handle<v8::Object> jsHandle, const EmiSockConfig& sc);
    virtual ~EmiSocket();
    
    static v8::Handle<v8::Value> SetCallbacks(const v8::Arguments& args);
    static v8::Handle<v8::Value> New(const v8::Arguments& args);
    static v8::Handle<v8::Value> DoConnect(const v8::Arguments& args, int family);
    static v8::Handle<v8::Value> Connect4(const v8::Arguments& args);
    static v8::Handle<v8::Value> Connect6(const v8::Arguments& args);
    
public:
    static void Init(v8::Handle<v8::Object> target);
    
    static v8::Persistent<v8::Function> gotConnection;
    static v8::Persistent<v8::Function> connectionPacketLoss;
    static v8::Persistent<v8::Function> connectionMessage;
    static v8::Persistent<v8::Function> connectionLost;
    static v8::Persistent<v8::Function> connectionRegained;
    static v8::Persistent<v8::Function> connectionDisconnect;
    static v8::Persistent<v8::Function> natPunchthroughFinished;
    static v8::Persistent<v8::Function> connectionError;
    
    inline EmiS& getSock() { return _sock; }
    inline const EmiS& getSock() const { return _sock; }
    inline v8::Handle<v8::Object> getJsHandle() const { return _jsHandle; }
};

#endif
