#define BUILDING_NODE_EXTENSION

#include "EmiSocket.h"

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
