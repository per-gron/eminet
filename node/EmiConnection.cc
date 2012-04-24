#define BUILDING_NODE_EXTENSION

#include "EmiConnection.h"
#include "EmiSocket.h"
#include "EmiNodeUtil.h"
#include "EmiConnectionParams.h"

#include <node.h>

using namespace v8;

Persistent<String>   EmiConnection::channelQualifierSymbol;
Persistent<String>   EmiConnection::prioritySymbol;
Persistent<Function> EmiConnection::constructor;

EmiConnection::EmiConnection(EmiSocket& es, const ECP& params) :
_conn(EmiConnDelegate(*this), es.getSock(), params) {};

EmiTimeInterval EmiConnection::Now() {
    return ((double)uv_hrtime())/EmiNodeUtil::NSECS_PER_SEC;
}

EmiConnection::~EmiConnection() {
    _jsHandle.Dispose();
}

void EmiConnection::Init(Handle<Object> target) {
    // Load symbols
#define X(sym) sym##Symbol = Persistent<String>::New(String::NewSymbol(#sym));
    X(channelQualifier);
    X(priority);
#undef X
    
    // Prepare constructor template
    Local<FunctionTemplate> tpl = FunctionTemplate::New(New);
    tpl->SetClassName(String::NewSymbol("EmiConnection"));
    tpl->InstanceTemplate()->SetInternalFieldCount(1);
  
  // Prototype
#define X(sym, name)                                        \
  tpl->PrototypeTemplate()->Set(String::NewSymbol(name),    \
      FunctionTemplate::New(sym)->GetFunction());
    X(Close,                      "close");
    X(ForceClose,                 "forceClose");
    X(CloseOrForceClose,          "closeOrForceClose");
    X(Flush,                      "flush");
    X(Send,                       "send");
    X(HasIssuedConnectionWarning, "hasIssuedConnectionWarning");
    X(GetSocket,                  "getSocket");
    X(GetAddressType,             "getAddressType");
    X(GetPort,                    "getPort");
    X(GetAddress,                 "getAddress");
    X(GetInboundPort,             "getInboundPort");
    X(IsOpen,                     "isOpen");
    X(IsOpening,                  "isOpening");
#undef X
    
    constructor = Persistent<Function>::New(tpl->GetFunction());
}

Handle<Object> EmiConnection::NewInstance(EmiSocket& es,
                                          const ECP& params) {
    HandleScope scope;
    
    const unsigned argc = 1;
    Handle<Value> argv[argc] = {
        EmiConnectionParams::NewInstance(es, params)
    };
    Local<Object> instance = constructor->NewInstance(argc, argv);
    
    return scope.Close(instance);
}

Handle<Value> EmiConnection::New(const Arguments& args) {
    HandleScope scope;
    
    ENSURE_NUM_ARGS(1, args);
    
    EmiConnectionParams *ecp =
    (EmiConnectionParams *)args[0]->ToObject()->GetPointerFromInternalField(0);
    
    EmiConnection *ec = new EmiConnection(ecp->es,
                                          ecp->params);
    
    delete ecp;
    
    ec->Wrap(args.This());
    
    // This prevents V8's GC to reclaim the EmiConnection before it's closed
    // The corresponding Unref is in EmiConnDelegate::invalidate
    ec->Ref();
    
    return args.This();
}

Handle<Value> EmiConnection::Close(const Arguments& args) {
    HandleScope scope;
    
    ENSURE_ZERO_ARGS(args);
    UNWRAP(EmiConnection, ec, args);
    
    EmiError err;
    if (!ec->_conn.close(Now(), err)) {
        return err.raise("Failed to close connection");
    }
    
    return scope.Close(Undefined());
}

Handle<Value> EmiConnection::ForceClose(const Arguments& args) {
    HandleScope scope;
    
    ENSURE_ZERO_ARGS(args);
    UNWRAP(EmiConnection, ec, args);
    
    ec->_conn.forceClose();
    
    return scope.Close(Undefined());
}

Handle<Value> EmiConnection::CloseOrForceClose(const Arguments& args) {
    HandleScope scope;
    
    ENSURE_ZERO_ARGS(args);
    UNWRAP(EmiConnection, ec, args);
    
    EmiError err;
    if (!ec->_conn.close(Now(), err)) {
        ec->_conn.forceClose();
    }
    
    return scope.Close(Undefined());
}

Handle<Value> EmiConnection::Flush(const Arguments& args) {
    HandleScope scope;
    
    ENSURE_ZERO_ARGS(args);
    UNWRAP(EmiConnection, ec, args);
    
    ec->_conn.flush(Now());
    
    return scope.Close(Undefined());
}

Handle<Value> EmiConnection::Send(const Arguments& args) {
    HandleScope scope;
    
    
    /// Basic argument checks
    
    size_t numArgs = args.Length();
    if (!(1 == numArgs || 2 == numArgs)) {
        THROW_TYPE_ERROR("Wrong number of arguments");
    }
    
    if (!args[0]->IsObject() || (2 == numArgs && !args[1]->IsObject())) {
        THROW_TYPE_ERROR("Wrong arguments");
    }
    
    
    /// Extract arguments
    
    EmiChannelQualifier channelQualifier = EMI_CHANNEL_QUALIFIER_DEFAULT;
    EmiPriority priority = EMI_PRIORITY_DEFAULT;
    
    if (2 == numArgs) {
        Local<Object> opts(args[1]->ToObject());
        Local<Value>   cqv(opts->Get(channelQualifierSymbol));
        Local<Value>    pv(opts->Get(prioritySymbol));
        
        if (!cqv.IsEmpty()) {
            if (!cqv->IsNumber()) {
                THROW_TYPE_ERROR("Wrong arguments");
            }
            
            channelQualifier = (EmiChannelQualifier) cqv->Uint32Value();
        }
        
        if (!pv.IsEmpty()) {
            if (!pv->IsNumber()) {
                THROW_TYPE_ERROR("Wrong arguments");
            }
            
            priority = (EmiPriority) pv->Uint32Value();
        }
    }
    
    
    // Do the actual send
    
    UNWRAP(EmiConnection, ec, args);
    
    EmiError err;
    if (!ec->_conn.send(Now(),
                        Persistent<Object>::New(args[0]->ToObject()),
                        channelQualifier,
                        priority,
                        err)) {
        return err.raise("Failed to send message");
    }
    
    return scope.Close(Undefined());
}

Handle<Value> EmiConnection::HasIssuedConnectionWarning(const Arguments& args) {
    HandleScope scope;
    
    ENSURE_ZERO_ARGS(args);
    UNWRAP(EmiConnection, ec, args);
    
    return scope.Close(Boolean::New(ec->_conn.issuedConnectionWarning()));
}

Handle<Value> EmiConnection::GetSocket(const Arguments& args) {
    HandleScope scope;
    
    ENSURE_ZERO_ARGS(args);
    UNWRAP(EmiConnection, ec, args);
    
    return scope.Close(ec->_conn.getEmiSock().getDelegate().getEmiSocket().getJsHandle());
}

Handle<Value> EmiConnection::GetAddressType(const Arguments& args) {
    HandleScope scope;
    
    ENSURE_ZERO_ARGS(args);
    UNWRAP(EmiConnection, ec, args);
    
    const char *type;
    
    const struct sockaddr_storage& addr(ec->_conn.getAddress());
    if (AF_INET == addr.ss_family) {
        type = "udp4";
    }
    else if (AF_INET6 == addr.ss_family) {
        type = "udp6";
    }
    else {
        ASSERT(0 && "unexpected address family");
    }
    
    return scope.Close(String::New(type));
}

Handle<Value> EmiConnection::GetPort(const Arguments& args) {
    HandleScope scope;
    
    ENSURE_ZERO_ARGS(args);
    UNWRAP(EmiConnection, ec, args);
    
    uint16_t port;
    
    const struct sockaddr_storage& addr(ec->_conn.getAddress());
    if (AF_INET == addr.ss_family) {
        port = ((struct sockaddr_in *)&addr)->sin_port;
    }
    else if (AF_INET6 == addr.ss_family) {
        port = ((struct sockaddr_in6 *)&addr)->sin6_port;
    }
    else {
        ASSERT(0 && "unexpected address family");
    }
    
    return scope.Close(Number::New(port));
}

Handle<Value> EmiConnection::GetAddress(const Arguments& args) {
    HandleScope scope;
    
    ENSURE_ZERO_ARGS(args);
    UNWRAP(EmiConnection, ec, args);
    
    char buf[256];
    
    const struct sockaddr_storage& addr(ec->_conn.getAddress());
    if (AF_INET == addr.ss_family) {
        uv_ip4_name((struct sockaddr_in *)&addr, buf, sizeof(buf));
    }
    else if (AF_INET6 == addr.ss_family) {
        uv_ip6_name((struct sockaddr_in6 *)&addr, buf, sizeof(buf));
    }
    else {
        ASSERT(0 && "unexpected address family");
    }
    
    return scope.Close(String::New(buf));
}

Handle<Value> EmiConnection::GetInboundPort(const Arguments& args) {
    HandleScope scope;
    
    ENSURE_ZERO_ARGS(args);
    UNWRAP(EmiConnection, ec, args);
    
    return scope.Close(Number::New(ec->_conn.getInboundPort()));
}

Handle<Value> EmiConnection::IsOpen(const Arguments& args) {
    HandleScope scope;
    
    ENSURE_ZERO_ARGS(args);
    UNWRAP(EmiConnection, ec, args);
    
    return scope.Close(Boolean::New(ec->_conn.isOpen()));
}

Handle<Value> EmiConnection::IsOpening(const Arguments& args) {
    HandleScope scope;
    
    ENSURE_ZERO_ARGS(args);
    UNWRAP(EmiConnection, ec, args);
    
    return scope.Close(Boolean::New(ec->_conn.isOpening()));
}
