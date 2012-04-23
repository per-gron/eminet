#define BUILDING_NODE_EXTENSION

#include "EmiConnectionParams.h"

#include <node.h>

using namespace v8;

Persistent<ObjectTemplate> EmiConnectionParams::constructor;

EmiConnectionParams::EmiConnectionParams(EmiSocket& es_,
                                         const ECP& params_) :
es(es_),
params(params_) {}

void EmiConnectionParams::Init(Handle<Object> target) {
    HandleScope scope;
    
    Local<ObjectTemplate> tpl(ObjectTemplate::New());
    tpl->SetInternalFieldCount(1);
    constructor = Persistent<ObjectTemplate>::New(tpl);
}

Handle<Object> EmiConnectionParams::NewInstance(EmiSocket& es,
                                                const ECP& params) {
    HandleScope scope;
    
    EmiConnectionParams *ecp = new EmiConnectionParams(es, params);
    Local<Object> obj(constructor->NewInstance());
    obj->SetPointerInInternalField(0, ecp);
    
    return scope.Close(obj);
}
