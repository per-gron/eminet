#define BUILDING_NODE_EXTENSION
#ifndef eminet_EmiConnectionParams_h
#define eminet_EmiConnectionParams_h

#include "EmiBinding.h"
#include "EmiConnDelegate.h"

#include "../core/EmiConn.h"
#include <node.h>

class EmiSocket;

// This is a class designed to help EmiConnection::NewInstance
class EmiConnectionParams : public node::ObjectWrap {
    typedef EmiConnParams<EmiBinding> ECP;
private:
    EmiConnectionParams(EmiSocket& es_,
                        const ECP& params_);
    
    static v8::Persistent<v8::ObjectTemplate> constructor;
    
public:
    static void Init(v8::Handle<v8::Object> target);
    
    EmiSocket& es;
    ECP params;
    
    static v8::Handle<v8::Object> NewInstance(EmiSocket& es,
                                              const ECP& params);
};

#endif
