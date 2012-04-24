#define BUILDING_NODE_EXTENSION

#include "EmiP2PSocket.h"

#include "EmiNodeUtil.h"

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

#define EXPAND_SYM(sym) Persistent<String> EmiP2PSocket::sym##Symbol;
EXPAND_SYMS
#undef EXPAND_SYM

EmiP2PSocket::EmiP2PSocket(v8::Handle<v8::Object> jsHandle, const EmiP2PSockConfig<EmiSockDelegate::Address>& sc) :
_sock(sc, EmiSockDelegate(*this)),
_jsHandle(v8::Persistent<v8::Object>::New(jsHandle)) {}

EmiP2PSocket::~EmiP2PSocket() {
    _jsHandle.Dispose();
}

void EmiP2PSocket::Init(Handle<Object> target) {
    HandleScope scope;
    
    // Load symbols
#define EXPAND_SYM(sym) \
  sym##Symbol = Persistent<String>::New(String::NewSymbol(#sym));
    EXPAND_SYMS
#undef EXPAND_SYM
    
    // Prepare constructor template
    Local<FunctionTemplate> tpl = FunctionTemplate::New(New);
    tpl->SetClassName(String::NewSymbol("EmiP2PSocket"));
    tpl->InstanceTemplate()->SetInternalFieldCount(1);
    
    // Prototype
#define X(sym, name)                                        \
  tpl->PrototypeTemplate()->Set(String::NewSymbol(name),    \
      FunctionTemplate::New(sym)->GetFunction());
    X(Suspend,   "suspend");
    X(Desuspend, "desuspend");
#undef X
    
    Persistent<Function> constructor = Persistent<Function>::New(tpl->GetFunction());
    target->Set(String::NewSymbol("EmiP2PSocket"), constructor);
}

Handle<Value> EmiP2PSocket::New(const Arguments& args) {
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
    
    EmiP2PSocket* obj = new EmiP2PSocket(jsHandle, sc);
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

Handle<Value> EmiP2PSocket::Suspend(const Arguments& args) {
    HandleScope scope;
    
    ENSURE_ZERO_ARGS(args);
    UNWRAP(EmiP2PSocket, es, args);
    es->_sock.suspend();
    
    return scope.Close(Undefined());
}

Handle<Value> EmiP2PSocket::Desuspend(const Arguments& args) {
    HandleScope scope;
    
    ENSURE_ZERO_ARGS(args);
    UNWRAP(EmiP2PSocket, es, args);
    EmiError err;
    if (!es->_sock.desuspend(err)) {
        return err.raise("Failed to desuspend socket");
    }
    
    return scope.Close(Undefined());
}
