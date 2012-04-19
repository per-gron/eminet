#define BUILDING_NODE_EXTENSION

#include "EmiConnectionParams.h"

#include <node.h>

using namespace v8;

Persistent<ObjectTemplate> EmiConnectionParams::constructor;

EmiConnectionParams::EmiConnectionParams(EmiSocket& es_,
                                         const struct sockaddr_storage& address_,
                                         uint16_t inboundPort_,
                                         bool initiator_) :
es(es_),
address(address_),
inboundPort(inboundPort_),
initiator(initiator_) {}

void EmiConnectionParams::Init(Handle<Object> target) {
  HandleScope scope;
  
  Local<ObjectTemplate> tpl(ObjectTemplate::New());
  tpl->SetInternalFieldCount(1);
  constructor = Persistent<ObjectTemplate>::New(tpl);
}

Handle<Object> EmiConnectionParams::NewInstance(EmiSocket& es, const struct sockaddr_storage& address, uint16_t inboundPort, bool initiator) {
  HandleScope scope;
  
  EmiConnectionParams *ecp = new EmiConnectionParams(es, address, inboundPort, initiator);
  Local<Object> obj(constructor->NewInstance());
  obj->SetPointerInInternalField(0, ecp);

  return scope.Close(obj);
}
