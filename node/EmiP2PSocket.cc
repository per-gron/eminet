#define BUILDING_NODE_EXTENSION

#include "EmiP2PSocket.h"

#include "EmiNodeUtil.h"

#include <node.h>
#include <stdlib.h>

using namespace v8;

#define EXPAND_SYMS                                        \
  EXPAND_SYM(connectionTimeout);                           \
  EXPAND_SYM(initialConnectionTimeout);                    \
  EXPAND_SYM(rateLimit);                                   \
  EXPAND_SYM(type);                                        \
  EXPAND_SYM(port);                                        \
  EXPAND_SYM(address);                                     \
  EXPAND_SYM(fabricatedPacketDropRate);

#define EXPAND_SYM(sym) Persistent<String> EmiP2PSocket::sym##Symbol;
EXPAND_SYMS
#undef EXPAND_SYM

Persistent<Function> EmiP2PSocket::connectionError;

EmiP2PSocket::EmiP2PSocket(v8::Handle<v8::Object> jsHandle, const EmiP2PSockConfig& sc) :
_sock(sc),
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
    X(GetAddressType,       "getAddressType");
    X(GetPort,              "getPort");
    X(GetAddress,           "getAddress");
    X(GenerateCookiePair,   "generateCookiePair");
    X(GenerateSharedSecret, "generateSharedSecret");
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
    
    EmiP2PSockConfig sc;
    
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
    
    READ_CONFIG(sc, connectionTimeout,        IsNumber,  EmiTimeInterval, NumberValue);
    READ_CONFIG(sc, initialConnectionTimeout, IsNumber,  EmiTimeInterval, NumberValue);
    READ_CONFIG(sc, rateLimit,                IsNumber,  size_t,          Uint32Value);
    READ_CONFIG(sc, port,                     IsNumber,  uint16_t,        Uint32Value);
    READ_CONFIG(sc, fabricatedPacketDropRate, IsNumber,  EmiTimeInterval, NumberValue);
    
    // If initialConnectionTimeout is not set, it should be
    // the value of connectionTimeout.
    if (!HAS_CONFIG_PARAM(initialConnectionTimeout)) {
        sc.initialConnectionTimeout = sc.connectionTimeout;
    }
    
    int family;
    READ_FAMILY_CONFIG(family, type, scope);
    READ_ADDRESS_CONFIG(sc, family, address);
    
    EmiP2PSocket* obj = new EmiP2PSocket(jsHandle, sc);
    // We need to Wrap the object now, or failing to open
    // would result in a memory leak. (We rely on Wrap to deallocate
    // obj when it's no longer used.)
    obj->Wrap(args.This());
    
    EmiError err;
    if (!obj->_sock.open(obj, err)) {
        delete obj;
        
        return err.raise("Failed to open socket");
    }
    
    return args.This();
}

Handle<Value> EmiP2PSocket::GetAddressType(const Arguments& args) {
    HandleScope scope;
    
    ENSURE_ZERO_ARGS(args);
    UNWRAP(EmiP2PSocket, ec, args);
    
    const char *type;
    
    // We use getRemoteAddress, because it is valid even before we
    // have received the first packet from the other host, unlike
    // getLocalAddress.
    const struct sockaddr_storage& addr(ec->_sock.getAddress());
    if (AF_INET == addr.ss_family) {
        type = "udp4";
    }
    else if (AF_INET6 == addr.ss_family) {
        type = "udp6";
    }
    else {
        ASSERT(0 && "unexpected address family");
    }
    
    return scope.Close(String::New(type));
}

Handle<Value> EmiP2PSocket::GetPort(const Arguments& args) {
    HandleScope scope;
    
    ENSURE_ZERO_ARGS(args);
    UNWRAP(EmiP2PSocket, ec, args);
    
    return scope.Close(Number::New(EmiNetUtil::addrPortH(ec->_sock.getAddress())));
}

Handle<Value> EmiP2PSocket::GetAddress(const Arguments& args) {
    HandleScope scope;
    
    ENSURE_ZERO_ARGS(args);
    UNWRAP(EmiP2PSocket, ec, args);
    
    char buf[256];
    EmiNodeUtil::ipName(buf, sizeof(buf), ec->_sock.getAddress());
    return scope.Close(String::New(buf));
}

Handle<Value> EmiP2PSocket::GenerateCookiePair(const Arguments& args) {
    HandleScope scope;
    
    ENSURE_ZERO_ARGS(args);
    UNWRAP(EmiP2PSocket, ec, args);
    
    // Allocate a buffer, one that will contain both cookies
    static const size_t bufLen(EPS::EMI_P2P_COOKIE_SIZE*2);
    node::Buffer *buf(node::Buffer::New(bufLen));
    uint8_t *bufData((uint8_t *)node::Buffer::Data(buf));
    
    ec->_sock.generateCookiePair(EmiNodeUtil::now(),
                                 bufData,          bufLen/2,
                                 bufData+bufLen/2, bufLen/2);
    
    // Make a new persistent handle (do not just reuse the persistent buf->handle_ handle)
    return scope.Close(buf->handle_);
}

Handle<Value> EmiP2PSocket::GenerateSharedSecret(const Arguments& args) {
    HandleScope scope;
    
    ENSURE_ZERO_ARGS(args);
    UNWRAP(EmiP2PSocket, ec, args);
    
    // Allocate a buffer
    node::Buffer *buf(node::Buffer::New(EPS::EMI_P2P_SHARED_SECRET_SIZE));
    
    ec->_sock.generateSharedSecret((uint8_t *)node::Buffer::Data(buf), node::Buffer::Length(buf));
    
    // Make a new persistent handle (do not just reuse the persistent buf->handle_ handle)
    return scope.Close(buf->handle_);
}
