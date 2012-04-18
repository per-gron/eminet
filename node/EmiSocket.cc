#define BUILDING_NODE_EXTENSION

#include "EmiSocket.h"

#include "EmiNodeUtil.h"
#include "EmiConnection.h"

#include <node.h>

using namespace v8;


Persistent<Function> EmiSocket::gotConnection;
Persistent<Function> EmiSocket::connectionMessage;
Persistent<Function> EmiSocket::connectionLost;
Persistent<Function> EmiSocket::connectionRegained;
Persistent<Function> EmiSocket::connectionDisconnect;

EmiSocket::EmiSocket(const EmiSockConfig<EmiSockDelegate::Address>& sc) :
  _sock(sc, EmiSockDelegate(this)) {}

EmiSocket::~EmiSocket() {}

void EmiSocket::Init(Handle<Object> target) {
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

Handle<Value> EmiSocket::New(const Arguments& args) {
  HandleScope scope;
  
  EmiSockConfig<EmiSockDelegate::Address> sc;
  
  EmiSocket* obj = new EmiSocket(sc);
  // TODO Call desuspend on the EmiSock
  obj->counter_ = args[0]->IsUndefined() ? 0 : args[0]->NumberValue();
  obj->Wrap(args.This());
  // TODO Make the persistent handle weak

  return args.This();
}

Handle<Value> EmiSocket::PlusOne(const Arguments& args) {
  HandleScope scope;

  EmiSocket* obj = ObjectWrap::Unwrap<EmiSocket>(args.This());
  obj->counter_ += 1;

  return scope.Close(Number::New(obj->counter_));
}

Handle<Value> EmiSocket::Suspend(const Arguments& args) {
  HandleScope scope;
  
  if (0 != args.Length()) {
    ThrowException(Exception::TypeError(String::New("Wrong number of arguments")));
    return scope.Close(Undefined());
  }

  EmiSocket* es = ObjectWrap::Unwrap<EmiSocket>(args.This());
  es->_sock.suspend();

  return scope.Close(Undefined());
}

Handle<Value> EmiSocket::Desuspend(const Arguments& args) {
  HandleScope scope;
  
  if (0 != args.Length()) {
    ThrowException(Exception::TypeError(String::New("Wrong number of arguments")));
    return scope.Close(Undefined());
  }

  EmiSocket* es = ObjectWrap::Unwrap<EmiSocket>(args.This());
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
    ThrowException(Exception::TypeError(String::New("Wrong number of arguments")));
    return scope.Close(Undefined());
  }
  
  if (!args[0]->IsString() || !args[1]->IsNumber() || !args[2]->IsFunction()) {
    ThrowException(Exception::TypeError(String::New("Wrong arguments")));
    return scope.Close(Undefined());
  }
  
  String::Utf8Value host(args[0]);
  uint16_t          port((uint16_t) args[1]->NumberValue());
  Local<Object>     callback(args[2]->ToObject());
  
  
  /// Lookup IP
  
  sockaddr_storage *address;
  if (AF_INET == family) {
    struct sockaddr_in address4(uv_ip4_addr(*host, port));
    address = (sockaddr_storage *)&address4;
  }
  else if (AF_INET6 == family) {
    struct sockaddr_in6 address6(uv_ip6_addr(*host, port));
    address = (sockaddr_storage *)&address6;
  }
  else {
    ASSERT(0 && "unexpected address family");
    abort();
  }
  
  
  /// Do the actual connect
  
  EmiSocket* es = ObjectWrap::Unwrap<EmiSocket>(args.This());
  EmiError err;
  Persistent<Object> cookie(Persistent<Object>::New(callback));
  if (!es->_sock.connect(EmiConnection::Now(), *address, cookie, err)) {
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
