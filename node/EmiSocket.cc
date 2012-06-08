#define BUILDING_NODE_EXTENSION

#include "EmiSocket.h"

#include "EmiNodeUtil.h"
#include "EmiConnection.h"

#include <node.h>
#include <stdlib.h>

using namespace v8;

#define EXPAND_SYMS                                        \
  EXPAND_SYM(mtu);                                         \
  EXPAND_SYM(heartbeatFrequency);                          \
  EXPAND_SYM(heartbeatsBeforeConnectionWarning);           \
  EXPAND_SYM(connectionTimeout);                           \
  EXPAND_SYM(initialConnectionTimeout);                    \
  EXPAND_SYM(receiverBufferSize);                          \
  EXPAND_SYM(senderBufferSize);                            \
  EXPAND_SYM(acceptConnections);                           \
  EXPAND_SYM(type);                                        \
  EXPAND_SYM(port);                                        \
  EXPAND_SYM(address);                                     \
  EXPAND_SYM(fabricatedPacketDropRate);

#define EXPAND_SYM(sym) Persistent<String> EmiSocket::sym##Symbol;
EXPAND_SYMS
#undef EXPAND_SYM

Persistent<Function> EmiSocket::gotConnection;
Persistent<Function> EmiSocket::connectionMessage;
Persistent<Function> EmiSocket::connectionLost;
Persistent<Function> EmiSocket::connectionRegained;
Persistent<Function> EmiSocket::connectionDisconnect;
Persistent<Function> EmiSocket::natPunchthroughFinished;
Persistent<Function> EmiSocket::connectionError;

EmiSocket::EmiSocket(v8::Handle<v8::Object> jsHandle, const EmiSockConfig& sc) :
_sock(sc, EmiSockDelegate(*this)),
_jsHandle(v8::Persistent<v8::Object>::New(jsHandle)) {}

EmiSocket::~EmiSocket() {
    _jsHandle.Dispose();
}

void EmiSocket::Init(Handle<Object> target) {
    HandleScope scope;
    
    // Load symbols
#define EXPAND_SYM(sym) \
  sym##Symbol = Persistent<String>::New(String::NewSymbol(#sym));
    EXPAND_SYMS
#undef EXPAND_SYM
    
    // Prepare constructor template
    Local<FunctionTemplate> tpl = FunctionTemplate::New(New);
    tpl->SetClassName(String::NewSymbol("EmiSocket"));
    tpl->InstanceTemplate()->SetInternalFieldCount(1);
    
    // Prototype
#define X(sym, name)                                        \
  tpl->PrototypeTemplate()->Set(String::NewSymbol(name),    \
      FunctionTemplate::New(sym)->GetFunction());
    X(Connect4,  "connect4");
    X(Connect6,  "connect6");
#undef X
    
    Persistent<Function> constructor = Persistent<Function>::New(tpl->GetFunction());
    target->Set(String::NewSymbol("EmiSocket"), constructor);
    
    target->Set(String::NewSymbol("setCallbacks"),
                FunctionTemplate::New(SetCallbacks)->GetFunction());
}

Handle<Value> EmiSocket::SetCallbacks(const Arguments& args) {
    HandleScope scope;
    
    ENSURE_NUM_ARGS(7, args);
    
    if (!args[0]->IsFunction() ||
        !args[1]->IsFunction() ||
        !args[2]->IsFunction() ||
        !args[3]->IsFunction() ||
        !args[4]->IsFunction() ||
        !args[5]->IsFunction() ||
        !args[6]->IsFunction()) {
        THROW_TYPE_ERROR("Wrong arguments");
    }
  
#define X(name, num)                                            \
    if (!name.IsEmpty()) name.Dispose();                        \
    Local<Function> name##Fn(Local<Function>::Cast(args[num])); \
    name = Persistent<Function>::New(name##Fn);
    
    X(gotConnection, 0);
    X(connectionMessage, 1);
    X(connectionLost, 2);
    X(connectionRegained, 3);
    X(connectionDisconnect, 4);
    X(natPunchthroughFinished, 5);
    X(connectionError, 6);
    
#undef X
    
    return scope.Close(Undefined());
}

Handle<Value> EmiSocket::New(const Arguments& args) {
    HandleScope scope;
    
    EmiSockConfig sc;
    
    size_t numArgs = args.Length();
    if (1 != numArgs && 2 != numArgs) {
        THROW_TYPE_ERROR("Wrong number of arguments");
    }
    
    if (args[0].IsEmpty() || !args[0]->IsObject()) {
        THROW_TYPE_ERROR("Wrong arguments");
    }
    Local<Object> jsHandle(args[0]->ToObject());
    
    Local<Object> conf;
    
    if (2 == numArgs && !args[1].IsEmpty() && !args[1]->IsUndefined()) {
        if (!args[0]->IsObject()) {
            THROW_TYPE_ERROR("Wrong arguments");
        }
        
        conf = args[1]->ToObject();
    }
        
#define EXPAND_SYM(sym)                                \
    Local<Value> sym;                                  \
    if (!conf.IsEmpty()) sym = conf->Get(sym##Symbol)
    EXPAND_SYMS
#undef EXPAND_SYM
    
    READ_CONFIG(sc, mtu,                               IsNumber,  size_t,          Uint32Value);
    READ_CONFIG(sc, heartbeatFrequency,                IsNumber,  float,           NumberValue);
    READ_CONFIG(sc, heartbeatsBeforeConnectionWarning, IsNumber,  float,           NumberValue);
    READ_CONFIG(sc, connectionTimeout,                 IsNumber,  EmiTimeInterval, NumberValue);
    READ_CONFIG(sc, initialConnectionTimeout,          IsNumber,  EmiTimeInterval, NumberValue);
    READ_CONFIG(sc, senderBufferSize,                  IsNumber,  size_t,          Uint32Value);
    READ_CONFIG(sc, acceptConnections,                 IsBoolean, bool,            BooleanValue);
    READ_CONFIG(sc, port,                              IsNumber,  uint16_t,        Uint32Value);
    READ_CONFIG(sc, fabricatedPacketDropRate,          IsNumber,  EmiTimeInterval, NumberValue);
    
    // If initialConnectionTimeout is not set, it should be
    // the value of connectionTimeout.
    if (!HAS_CONFIG_PARAM(initialConnectionTimeout)) {
        sc.initialConnectionTimeout = sc.connectionTimeout;
    }
    
    int family;
    READ_FAMILY_CONFIG(family, type, scope);
    READ_ADDRESS_CONFIG(sc, family, address);
    
    EmiSocket* obj = new EmiSocket(jsHandle, sc);
    // We need to Wrap the object now, or failing to open
    // would result in a memory leak. (We rely on Wrap to deallocate
    // obj when it's no longer used.)
    obj->Wrap(args.This());
    
    EmiError err;
    if (!obj->_sock.open(err)) {
        delete obj;
        
        return err.raise("Failed to open socket");
    }
    
    return args.This();
}

Handle<Value> EmiSocket::DoConnect(const Arguments& args, int family) {
    HandleScope scope;
    
    
    /// Extract arguments
    
    if (3 != args.Length() && 5 != args.Length()) {
        THROW_TYPE_ERROR("Wrong number of arguments");
    }
    
    bool p2p = (5 == args.Length());
    
    if (!args[0]->IsString() ||
        !args[1]->IsNumber() ||
        !args[2]->IsFunction()) {
        THROW_TYPE_ERROR("Wrong arguments");
    }
    
    // Note that we do not verify that the arguments are Buffer objects;
    // we assume that the JS binding code ensures that.
    if (p2p && (!args[3]->IsObject() || !args[4]->IsObject())) {
        THROW_TYPE_ERROR("Wrong arguments");
    }
    
    String::Utf8Value host(args[0]);
    uint16_t          port(args[1]->Uint32Value());
    Local<Object>     callback(args[2]->ToObject());
    
    
    /// Lookup IP
    
    sockaddr_storage address;
    EmiNodeUtil::parseIp(*host, port, family, &address);
    
    
    /// Do the actual connect
    
    UNWRAP(EmiSocket, es, args);
    EmiError err;
    
    // Create a new Persistent handle. EmiSockDelegate::connectionOpened
    // is responsible for disposing the handle, except for when
    // EmiSock::connect returns false, in which case we'll do it here.
    Persistent<Object> cookie(Persistent<Object>::New(callback));
    
    if (p2p) {
        Local<Object>    p2pCookie(args[3]->ToObject());
        Local<Object> sharedSecret(args[4]->ToObject());
        
        if (!es->_sock.connect(EmiNodeUtil::now(), address,
                               (uint8_t *)node::Buffer::Data(p2pCookie),
                               node::Buffer::Length(p2pCookie),
                               (uint8_t *)node::Buffer::Data(sharedSecret),
                               node::Buffer::Length(sharedSecret),
                               cookie, err)) {
            goto error;
        }
    }
    else {
        if (!es->_sock.connect(EmiNodeUtil::now(), address, cookie, err)) {
            goto error;
        }
    }
    
    return scope.Close(Undefined());
    
error:
    // Since the connect operation failed, we need to dispose of the
    // cookie.  (If it succeeds, EmiSockDelegate::connectionOpened
    // will take care of the cookie disposal.)
    cookie.Dispose();
    cookie.Clear();
    
    return err.raise("Failed to connect");
}

Handle<Value> EmiSocket::Connect4(const Arguments& args) {
    return DoConnect(args, AF_INET);
}

Handle<Value> EmiSocket::Connect6(const Arguments& args) {
    return DoConnect(args, AF_INET6);
}
