#define BUILDING_NODE_EXTENSION

#include "EmiSocket.h"
#include "EmiConnection.h"
#include "EmiConnectionParams.h"
#include "EmiP2PSocket.h"
#include "../core/EmiTypes.h"

#include <node.h>

using namespace v8;

void InitAll(Handle<Object> target) {
    HandleScope scope;
    
    EmiSocket::Init(target);
    EmiConnection::Init(target);
    EmiConnectionParams::Init(target);
    EmiP2PSocket::Init(target);
    
    
    Local<Object> obj(Object::New());
    target->Set(String::NewSymbol("enums"), obj);
#define X(name)                                \
    obj->Set(String::NewSymbol(#name),         \
             Number::New(name))
    
    // EmiPriority
    X(EMI_PRIORITY_IMMEDIATE);
    X(EMI_PRIORITY_HIGH);
    X(EMI_PRIORITY_MEDIUM);
    X(EMI_PRIORITY_LOW);
    
    // EmiChannelType
    X(EMI_CHANNEL_TYPE_UNRELIABLE);
    X(EMI_CHANNEL_TYPE_UNRELIABLE_SEQUENCED);
    X(EMI_CHANNEL_TYPE_RELIABLE_SEQUENCED);
    X(EMI_CHANNEL_TYPE_RELIABLE_ORDERED);
    
    // EmiDisconnectReason
    X(EMI_REASON_NO_ERROR);
    X(EMI_REASON_THIS_HOST_CLOSED);
    X(EMI_REASON_OTHER_HOST_CLOSED);
    X(EMI_REASON_CONNECTION_TIMED_OUT);
    X(EMI_REASON_OTHER_HOST_DID_NOT_RESPOND);
    
#undef X
}

NODE_MODULE(eminet, InitAll)
