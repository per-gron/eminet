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

Persistent<Function> EmiP2PSocket::connectionError;

EmiP2PSocket::EmiP2PSocket(v8::Handle<v8::Object> jsHandle, const EmiP2PSockConfig<EmiBinding::Address>& sc) :
_sock(sc, EmiP2PSockDelegate(*this)),
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
    
    target->Set(String::NewSymbol("setP2PCallbacks"),
                FunctionTemplate::New(SetCallbacks)->GetFunction());
}

Handle<Value> EmiP2PSocket::SetCallbacks(const Arguments& args) {
    HandleScope scope;
    
    ENSURE_NUM_ARGS(1, args);
    
    if (!args[0]->IsFunction()) {
        THROW_TYPE_ERROR("Wrong arguments");
    }
    
#define X(name, num)                                            \
    if (!name.IsEmpty()) name.Dispose();                        \
    Local<Function> name##Fn(Local<Function>::Cast(args[num])); \
    name = Persistent<Function>::New(name##Fn);
    
    X(connectionError, 0);
    
#undef X
    
    return scope.Close(Undefined());
}

Handle<Value> EmiP2PSocket::New(const Arguments& args) {
    HandleScope scope;
    
    EmiP2PSockConfig<EmiBinding::Address> sc;
    
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
    
    READ_CONFIG(sc, connectionTimeout,                 IsNumber,  EmiTimeInterval, NumberValue);
    READ_CONFIG(sc, rateLimit,                         IsNumber,  size_t,          Uint32Value);
    READ_CONFIG(sc, port,                              IsNumber,  uint16_t,        Uint32Value);
    READ_CONFIG(sc, fabricatedPacketDropRate,          IsNumber,  EmiTimeInterval, NumberValue);
    
    int family;
    READ_FAMILY_CONFIG(family, type, scope);
    READ_ADDRESS_CONFIG(sc, family, address);
    
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
