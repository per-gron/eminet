#define BUILDING_NODE_EXTENSION

#include "EmiSocket.h"

#include "EmiNodeUtil.h"
#include "EmiConnection.h"

#include <node.h>
#include <stdlib.h>

using namespace v8;

static void parseIp(const char* host,
                    uint16_t port,
                    int family,
                    sockaddr_storage *out) {
  if (AF_INET == family) {
    struct sockaddr_in address4(uv_ip4_addr(host, port));
    memcpy(out, &address4, sizeof(struct sockaddr_in));
  }
  else if (AF_INET6 == family) {
    struct sockaddr_in6 address6(uv_ip6_addr(host, port));
    memcpy(out, &address6, sizeof(struct sockaddr_in));
  }
  else {
    ASSERT(0 && "unexpected address family");
    abort();
  }
}

static int parseAddressFamily(const char* typeStr) {
  if (0 == strcmp(typeStr, "udp4")) {
    return AF_INET;
  }
  else if (0 == strcmp(typeStr, "udp6")) {
    return AF_INET6;
  }
  else {
    ASSERT(0 && "unexpected address family");
    abort();
  }
}

Persistent<String> EmiSocket::mtuSymbol;
Persistent<String> EmiSocket::heartbeatFrequencySymbol;
Persistent<String> EmiSocket::tickFrequencySymbol;
Persistent<String> EmiSocket::heartbeatsBeforeConnectionWarningSymbol;
Persistent<String> EmiSocket::connectionTimeoutSymbol;
Persistent<String> EmiSocket::receiverBufferSizeSymbol;
Persistent<String> EmiSocket::senderBufferSizeSymbol;
Persistent<String> EmiSocket::acceptConnectionsSymbol;
Persistent<String> EmiSocket::typeSymbol;
Persistent<String> EmiSocket::portSymbol;
Persistent<String> EmiSocket::addressSymbol;
Persistent<String> EmiSocket::fabricatedPacketDropRateSymbol;

Persistent<Function> EmiSocket::gotConnection;
Persistent<Function> EmiSocket::connectionMessage;
Persistent<Function> EmiSocket::connectionLost;
Persistent<Function> EmiSocket::connectionRegained;
Persistent<Function> EmiSocket::connectionDisconnect;

EmiSocket::EmiSocket(const EmiSockConfig<EmiSockDelegate::Address>& sc) :
  _sock(sc, EmiSockDelegate(this)) {}

EmiSocket::~EmiSocket() {}

void EmiSocket::Init(Handle<Object> target) {
  mtuSymbol = Persistent<String>::New(String::NewSymbol("mtu"));
  heartbeatFrequencySymbol = Persistent<String>::New(String::NewSymbol("heartbeatFrequency"));
  tickFrequencySymbol = Persistent<String>::New(String::NewSymbol("tickFrequency"));
  heartbeatsBeforeConnectionWarningSymbol = Persistent<String>::New(String::NewSymbol("heartbeatsBeforeConnectionWarning"));
  connectionTimeoutSymbol = Persistent<String>::New(String::NewSymbol("connectionTimeout"));
  receiverBufferSizeSymbol = Persistent<String>::New(String::NewSymbol("receiverBufferSize"));
  senderBufferSizeSymbol = Persistent<String>::New(String::NewSymbol("senderBufferSize"));
  acceptConnectionsSymbol = Persistent<String>::New(String::NewSymbol("acceptConnections"));
  typeSymbol = Persistent<String>::New(String::NewSymbol("type"));
  portSymbol = Persistent<String>::New(String::NewSymbol("port"));
  addressSymbol = Persistent<String>::New(String::NewSymbol("address"));
  fabricatedPacketDropRateSymbol = Persistent<String>::New(String::NewSymbol("fabricatedPacketDropRate"));
  
  // Prepare constructor template
  Local<FunctionTemplate> tpl = FunctionTemplate::New(New);
  tpl->SetClassName(String::NewSymbol("EmiSocket"));
  tpl->InstanceTemplate()->SetInternalFieldCount(1);
  // Prototype
  tpl->PrototypeTemplate()->Set(String::NewSymbol("plusOne"),
      FunctionTemplate::New(PlusOne)->GetFunction());
  tpl->PrototypeTemplate()->Set(String::NewSymbol("suspend"),
      FunctionTemplate::New(Suspend)->GetFunction());
  tpl->PrototypeTemplate()->Set(String::NewSymbol("desuspend"),
      FunctionTemplate::New(Desuspend)->GetFunction());
  tpl->PrototypeTemplate()->Set(String::NewSymbol("connect4"),
      FunctionTemplate::New(Connect4)->GetFunction());
  tpl->PrototypeTemplate()->Set(String::NewSymbol("connect6"),
      FunctionTemplate::New(Connect6)->GetFunction());

  Persistent<Function> constructor = Persistent<Function>::New(tpl->GetFunction());
  target->Set(String::NewSymbol("EmiSocket"), constructor);
}

#define THROW_TYPE_ERROR(err)                                 \
  do {                                                        \
    ThrowException(Exception::TypeError(String::New(err)));   \
    return scope.Close(Undefined());                          \
  } while (0)

Handle<Value> EmiSocket::New(const Arguments& args) {
  HandleScope scope;
  
  EmiSockConfig<EmiSockDelegate::Address> sc;
  
  size_t numArgs = args.Length();
  if (0 != numArgs && 1 != numArgs) {
    THROW_TYPE_ERROR("Wrong number of arguments");
  }
  
  if (numArgs) {
    if (!args[0]->IsObject()) {
      THROW_TYPE_ERROR("Wrong arguments");
    }
    
    Local<Object> conf(args[0]->ToObject());
    
#define X(sym) Local<Value> sym(conf->Get(mtu##Symbol))
    X(mtu);
    X(heartbeatFrequency);
    X(tickFrequency);
    X(heartbeatsBeforeConnectionWarning);
    X(connectionTimeout);
    X(receiverBufferSize);
    X(senderBufferSize);
    X(acceptConnections);
    X(type);
    X(port);
    X(address);
    X(fabricatedPacketDropRate);
#undef X

#define CHECK_PARAM(sym, pred)                                       \
    do {                                                             \
      if (sym.IsEmpty() || !sym->pred()) {                           \
        THROW_TYPE_ERROR("Invalid socket configuration parameters"); \
      }                                                              \
    } while (0)
#define HAS_PARAM(sym) (!sym.IsEmpty() && !sym->IsUndefined())
#define X(sym, pred, type, cast)                                     \
    do {                                                             \
      if (HAS_PARAM(sym)) {                                          \
        CHECK_PARAM(sym, pred);                                      \
        sc.sym = (type) sym->cast();                                 \
      }                                                              \
    } while (0)
    
    X(mtu,                               IsNumber,  size_t,          NumberValue);
    X(heartbeatFrequency,                IsNumber,  float,           NumberValue);
    X(tickFrequency,                     IsNumber,  float,           NumberValue);
    X(heartbeatsBeforeConnectionWarning, IsNumber,  float,           NumberValue);
    X(connectionTimeout,                 IsNumber,  EmiTimeInterval, NumberValue);
    X(senderBufferSize,                  IsNumber,  size_t,          NumberValue);
    X(acceptConnections,                 IsBoolean, bool,            BooleanValue);
    X(port,                              IsNumber,  uint16_t,        NumberValue);
    X(fabricatedPacketDropRate,          IsNumber,  EmiTimeInterval, NumberValue);
    
    if (HAS_PARAM(type) || HAS_PARAM(address)) {
      CHECK_PARAM(type, IsString);
      CHECK_PARAM(address, IsString);
      
      String::Utf8Value typeStr(type);
      int family(parseAddressFamily(*typeStr));
      
      String::Utf8Value host(address);
      parseIp(*host, sc.port, family, &sc.address);
    }

#undef CHECK_PARAM
#undef HAS_PARAM
#undef X
  }
  
  EmiSocket* obj = new EmiSocket(sc);
  EmiError err;
  if (!obj->_sock.desuspend(err)) {
    delete obj;
    
    // TODO Add the information in err to the exception that is thrown
    ThrowException(Exception::Error(String::New("Failed to open socket")));
    return scope.Close(Undefined());
  }
  
  obj->counter_ = 0;
  obj->Wrap(args.This());

  return args.This();
}

#define UNWRAP(name, args) EmiSocket *name(ObjectWrap::Unwrap<EmiSocket>(args.This()))

Handle<Value> EmiSocket::PlusOne(const Arguments& args) {
  HandleScope scope;
  
  UNWRAP(obj, args);
  obj->counter_ += 1;

  return scope.Close(Number::New(obj->counter_));
}

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
    // TODO Add the information in err to the exception that is thrown
    ThrowException(Exception::Error(String::New("Failed to desuspend socket")));
    return scope.Close(Undefined());
  }

  return scope.Close(Undefined());
}

Handle<Value> EmiSocket::DoConnect(const Arguments& args, int family) {
  HandleScope scope;
  
  
  /// Extract arguments
  
  if (3 != args.Length()) {
    THROW_TYPE_ERROR("Wrong number of arguments");
  }
  
  if (!args[0]->IsString() || !args[1]->IsNumber() || !args[2]->IsFunction()) {
    THROW_TYPE_ERROR("Wrong arguments");
  }
  
  String::Utf8Value host(args[0]);
  uint16_t          port((uint16_t) args[1]->NumberValue());
  Local<Object>     callback(args[2]->ToObject());
  
  
  /// Lookup IP
  
  sockaddr_storage address;
  parseIp(*host, port, family, &address);
  
  
  /// Do the actual connect
  
  UNWRAP(es, args);
  EmiError err;
  Persistent<Object> cookie(Persistent<Object>::New(callback));
  if (!es->_sock.connect(EmiConnection::Now(), address, cookie, err)) {
    // Since the connect operation failed, we need to dispose of the
    // cookie.  (If it succeeds, EmiSockDelegate::connectionOpened
    // will take care of the cookie disposal.)
    cookie.Dispose();
    cookie.Clear();
    
    // TODO Add the information in err to the exception that is thrown
    ThrowException(Exception::Error(String::New("Failed to connect")));
    return scope.Close(Undefined());
  }

  return scope.Close(Undefined());
}

Handle<Value> EmiSocket::Connect4(const Arguments& args) {
  return DoConnect(args, AF_INET);
}

Handle<Value> EmiSocket::Connect6(const Arguments& args) {
  return DoConnect(args, AF_INET6);
}
