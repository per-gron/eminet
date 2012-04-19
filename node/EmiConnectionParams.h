#define BUILDING_NODE_EXTENSION
#ifndef emilir_EmiConnectionParams_h
#define emilir_EmiConnectionParams_h

#include "EmiSockDelegate.h"
#include "EmiConnDelegate.h"
#include "EmiAddressCmp.h"

#include "../core/EmiConn.h"
#include <node.h>

// This is a class designed to help EmiConnection::NewInstance
class EmiConnectionParams : public node::ObjectWrap {
private:
    EmiConnectionParams(EmiSocket& es_, const struct sockaddr_storage& address_, uint16_t inboundPort_, bool initiator_);
    
    static v8::Persistent<v8::ObjectTemplate> constructor;
    
public:
    static void Init(v8::Handle<v8::Object> target);
    
    EmiSocket& es;
    struct sockaddr_storage address;
    uint16_t inboundPort;
    bool initiator;
    
    static v8::Handle<v8::Object> NewInstance(EmiSocket& es, const struct sockaddr_storage& address, uint16_t inboundPort, bool initiator);
};

#endif
