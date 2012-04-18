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
  static v8::Persistent<v8::Function> constructor;
  
  EmiConnection(EmiSocket *es, const struct sockaddr_storage& address, uint16_t inboundPort, bool initiator);
  
 public:
  static EmiTimeInterval Now();
  
  static void Init(v8::Handle<v8::Object> target);
  
  static v8::Handle<v8::Object> NewInstance(EmiSocket *es, const struct sockaddr_storage& address, uint16_t inboundPort, bool initiator);
  
  inline EC& getConn() { return _conn; }
  inline const EC& getConn() const { return _conn; }
  
 private:
  static v8::Handle<v8::Value> New(const v8::Arguments& args);
  static v8::Handle<v8::Value> Close(const v8::Arguments& args);
  static v8::Handle<v8::Value> ForceClose(const v8::Arguments& args);
  static v8::Handle<v8::Value> CloseOrForceClose(const v8::Arguments& args);
  static v8::Handle<v8::Value> Flush(const v8::Arguments& args);
  static v8::Handle<v8::Value> Send(const v8::Arguments& args);
};

#endif
