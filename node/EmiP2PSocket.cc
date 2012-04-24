#define BUILDING_NODE_EXTENSION

#include "EmiSocket.h"

#include "EmiNodeUtil.h"
#include "EmiConnection.h"

#include <node.h>
#include <stdlib.h>

using namespace v8;

#define EXPAND_SYMS                                        \
  EXPAND_SYM(connectionTimeout);                           \
  EXPAND_SYM(rateLimit);                                   \
  EXPAND_SYM(type);                                        \
  EXPAND_SYM(port);                                        \
  EXPAND_SYM(address);                                     \
  EXPAND_SYM(fabricatedPacketDropRate);

#define EXPAND_SYM(sym) Persistent<String> EmiSocket::sym##Symbol;
EXPAND_SYMS
#undef EXPAND_SYM

EmiSocket::EmiSocket(v8::Handle<v8::Object> jsHandle, const EmiSockConfig<EmiSockDelegate::Address>& sc) :
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
    X(Suspend,   "suspend");
    X(Desuspend, "desuspend");
    X(Connect4,  "connect4");
    X(Connect6,  "connect6");
#undef X
    
    Persistent<Function> constructor = Persistent<Function>::New(tpl->GetFunction());
    target->Set(String::NewSymbol("EmiSocket"), constructor);
    
    target->Set(String::NewSymbol("setCallbacks"),
                FunctionTemplate::New(SetCallbacks)->GetFunction());
}

#define THROW_TYPE_ERROR(err)                                 \
  do {                                                        \
    ThrowException(Exception::TypeError(String::New(err)));   \
    return scope.Close(Undefined());                          \
  } while (0)

Handle<Value> EmiSocket::SetCallbacks(const Arguments& args) {
    HandleScope scope;
    
    size_t numArgs = args.Length();
    if (6 != numArgs) {
        THROW_TYPE_ERROR("Wrong number of arguments");
    }
    
    if (!args[0]->IsFunction() ||
        !args[1]->IsFunction() ||
        !args[2]->IsFunction() ||
        !args[3]->IsFunction() ||
        !args[4]->IsFunction() ||
        !args[5]->IsFunction()) {
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
    X(connectionError, 5);
    
#undef X
    
    return scope.Close(Undefined());
}

Handle<Value> EmiSocket::New(const Arguments& args) {
    HandleScope scope;
    
    EmiSockConfig<EmiSockDelegate::Address> sc;
    
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

#define CHECK_PARAM(sym, pred)                                           \
    do {                                                                 \
        if (sym.IsEmpty() || !sym->pred()) {                             \
            THROW_TYPE_ERROR("Invalid socket configuration parameters"); \
        }                                                                \
    } while (0)
#define HAS_PARAM(sym) (!sym.IsEmpty() && !sym->IsUndefined())
#define X(sym, pred, type, cast)                                 \
    do {                                                         \
        if (HAS_PARAM(sym)) {                                    \
            CHECK_PARAM(sym, pred);                              \
            sc.sym = (type) sym->cast();                         \
        }                                                        \
    } while (0)
    
    X(mtu,                               IsNumber,  size_t,          Uint32Value);
    X(heartbeatFrequency,                IsNumber,  float,           NumberValue);
    X(tickFrequency,                     IsNumber,  float,           NumberValue);
    X(heartbeatsBeforeConnectionWarning, IsNumber,  float,           NumberValue);
    X(connectionTimeout,                 IsNumber,  EmiTimeInterval, NumberValue);
    X(senderBufferSize,                  IsNumber,  size_t,          Uint32Value);
    X(acceptConnections,                 IsBoolean, bool,            BooleanValue);
    X(port,                              IsNumber,  uint16_t,        Uint32Value);
    X(fabricatedPacketDropRate,          IsNumber,  EmiTimeInterval, NumberValue);
    
    int family;
    if (HAS_PARAM(type)) {
        CHECK_PARAM(type, IsString);
        
        String::Utf8Value typeStr(type);
        if (!parseAddressFamily(*typeStr, &family)) {
            ThrowException(Exception::Error(String::New("Unknown address family")));
            return scope.Close(Undefined());
        }
    }
    else {
        family = AF_INET;
    }
    
    if (HAS_PARAM(address)) {
        CHECK_PARAM(address, IsString);
        String::Utf8Value host(address);
        parseIp(*host, sc.port, family, &sc.address);
    }
    else {
        anyIp(sc.port, family, &sc.address);
    }
    
#undef CHECK_PARAM
#undef HAS_PARAM
#undef X
    
    EmiSocket* obj = new EmiSocket(jsHandle, sc);
    // We need to Wrap the object now, or failing to desuspend
    // would result in a memory leak. (We rely on Wrap to deallocate
    // obj when it's no longer used.)
    obj->Wrap(args.This());
    
    EmiError err;
    if (!obj->_sock.desuspend(err)) {
        delete obj;
        
        return err.raise("Failed to open socket");
    }
    
    return args.This();
}

#define UNWRAP(name, args) EmiSocket *name(ObjectWrap::Unwrap<EmiSocket>(args.This()))

Handle<Value> EmiSocket::Suspend(const Arguments& args) {
    HandleScope scope;
    
    if (0 != args.Length()) {
        THROW_TYPE_ERROR("Wrong number of arguments");
    }
    
    UNWRAP(es, args);
    es->_sock.suspend();
    
    return scope.Close(Undefined());
}

Handle<Value> EmiSocket::Desuspend(const Arguments& args) {
    HandleScope scope;
    
    if (0 != args.Length()) {
        THROW_TYPE_ERROR("Wrong number of arguments");
    }
    
    UNWRAP(es, args);
    EmiError err;
    if (!es->_sock.desuspend(err)) {
        return err.raise("Failed to desuspend socket");
    }
    
    return scope.Close(Undefined());
}

Handle<Value> EmiSocket::DoConnect(const Arguments& args, int family) {
    HandleScope scope;
    
    
    /// Extract arguments
    
    if (3 != args.Length()) {
        THROW_TYPE_ERROR("Wrong number of arguments");
    }
    
    if (!args[0]->IsString() ||
        !args[1]->IsNumber() ||
        !args[2]->IsFunction()) {
        THROW_TYPE_ERROR("Wrong arguments");
    }
    
    String::Utf8Value host(args[0]);
    uint16_t          port(args[1]->Uint32Value());
    Local<Object>     callback(args[2]->ToObject());
    
    
    /// Lookup IP
    
    sockaddr_storage address;
    parseIp(*host, port, family, &address);
    
    
    /// Do the actual connect
    
    UNWRAP(es, args);
    EmiError err;
    
    // Create a new Persistent handle. EmiSockDelegate::connectionOpened
    // is responsible for disposing the handle, except for when
    // EmiSock::connect returns false, in which case we'll do it here.
    Persistent<Object> cookie(Persistent<Object>::New(callback));
    
    if (!es->_sock.connect(EmiConnection::Now(), address, cookie, err)) {
        // Since the connect operation failed, we need to dispose of the
        // cookie.  (If it succeeds, EmiSockDelegate::connectionOpened
        // will take care of the cookie disposal.)
        cookie.Dispose();
        cookie.Clear();
        
        return err.raise("Failed to connect");
    }
    
    return scope.Close(Undefined());
}

Handle<Value> EmiSocket::Connect4(const Arguments& args) {
    return DoConnect(args, AF_INET);
}

Handle<Value> EmiSocket::Connect6(const Arguments& args) {
    return DoConnect(args, AF_INET6);
}
