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
#define X(name, value)                         \
    obj->Set(String::NewSymbol(#name),         \
             Number::New(value))
    
    // EmiPriority
    X(PRIORITY_IMMEDIATE, EMI_PRIORITY_IMMEDIATE);
    X(PRIORITY_HIGH,      EMI_PRIORITY_HIGH);
    X(PRIORITY_MEDIUM,    EMI_PRIORITY_MEDIUM);
    X(PRIORITY_LOW,       EMI_PRIORITY_LOW);
    
    // EmiChannelType
    X(UNRELIABLE,           EMI_CHANNEL_TYPE_UNRELIABLE);
    X(UNRELIABLE_SEQUENCED, EMI_CHANNEL_TYPE_UNRELIABLE_SEQUENCED);
    X(RELIABLE_SEQUENCED,   EMI_CHANNEL_TYPE_RELIABLE_SEQUENCED);
    X(RELIABLE_ORDERED,     EMI_CHANNEL_TYPE_RELIABLE_ORDERED);
    
    // EmiDisconnectReason
    X(NO_ERROR,                   EMI_REASON_NO_ERROR);
    X(THIS_HOST_CLOSED,           EMI_REASON_THIS_HOST_CLOSED);
    X(OTHER_HOST_CLOSED,          EMI_REASON_OTHER_HOST_CLOSED);
    X(CONNECTION_TIMED_OUT,       EMI_REASON_CONNECTION_TIMED_OUT);
    X(OTHER_HOST_DID_NOT_RESPOND, EMI_REASON_OTHER_HOST_DID_NOT_RESPOND);
    
#undef X
}

NODE_MODULE(eminet, InitAll)
