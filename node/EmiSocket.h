#define BUILDING_NODE_EXTENSION
#ifndef emilir_EmiSocket_h
#define emilir_EmiSocket_h

#include "EmiSockDelegate.h"
#include "EmiConnDelegate.h"
#include "EmiAddressCmp.h"

#include "../core/EmiSock.h"
#include "../core/EmiConn.h"
#include <node.h>

class EmiSocket : public node::ObjectWrap {
  typedef EmiSock<EmiSockDelegate, EmiConnDelegate> ES;
  
 private:
  ES _sock;
  double counter_;
  
  static v8::Persistent<v8::String> mtuSymbol;
  static v8::Persistent<v8::String> heartbeatFrequencySymbol;
  static v8::Persistent<v8::String> tickFrequencySymbol;
  static v8::Persistent<v8::String> heartbeatsBeforeConnectionWarningSymbol;
  static v8::Persistent<v8::String> connectionTimeoutSymbol;
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
  
  EmiSocket(const EmiSockConfig<EmiSockDelegate::Address>& sc);
  virtual ~EmiSocket();
  
  static v8::Handle<v8::Value> SetCallbacks(const v8::Arguments& args);
  static v8::Handle<v8::Value> New(const v8::Arguments& args);
  static v8::Handle<v8::Value> PlusOne(const v8::Arguments& args);
  static v8::Handle<v8::Value> Suspend(const v8::Arguments& args);
  static v8::Handle<v8::Value> Desuspend(const v8::Arguments& args);
  static v8::Handle<v8::Value> DoConnect(const v8::Arguments& args, int family);
  static v8::Handle<v8::Value> Connect4(const v8::Arguments& args);
  static v8::Handle<v8::Value> Connect6(const v8::Arguments& args);
  
 public:
  static void Init(v8::Handle<v8::Object> target);
  
  static v8::Persistent<v8::Function> gotConnection;
  static v8::Persistent<v8::Function> connectionMessage;
  static v8::Persistent<v8::Function> connectionLost;
  static v8::Persistent<v8::Function> connectionRegained;
  static v8::Persistent<v8::Function> connectionDisconnect;
  
  inline ES& getSock() { return _sock; }
  inline const ES& getSock() const { return _sock; }
};

#endif
