#define BUILDING_NODE_EXTENSION

#include "EmiConnectionParams.h"

#include <node.h>

using namespace v8;

EmiConnectionParams::EmiConnectionParams(EmiSocket *es_, const struct sockaddr_storage& address_, uint16_t inboundPort_, bool initiator_) :
es(es_), address(address_), inboundPort(inboundPort_), initiator(initiator_) {}

Handle<Object> EmiConnectionParams::NewInstance(EmiSocket *es, const struct sockaddr_storage& address, uint16_t inboundPort, bool initiator) {
  HandleScope scope;
  
  EmiConnectionParams *ecp = new EmiConnectionParams(es, address, inboundPort, initiator);
  Local<Object> obj = Object::New();
  ecp->Wrap(obj);

  return scope.Close(obj);
}
