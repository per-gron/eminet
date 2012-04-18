#define BUILDING_NODE_EXTENSION

#include "EmiSocket.h"
#include "EmiConnection.h"

#include <node.h>

using namespace v8;

void InitAll(Handle<Object> target) {
  EmiSocket::Init(target);
  EmiConnection::Init(target);
}

NODE_MODULE(eminet, InitAll)
