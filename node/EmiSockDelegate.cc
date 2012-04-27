#define BUILDING_NODE_EXTENSION

#include "EmiSockDelegate.h"

#include "EmiNodeUtil.h"
#include "EmiSocket.h"
#include "EmiConnection.h"

#include <node.h>
#include <netinet/in.h>

using namespace v8;

static void recv_cb(uv_udp_t *socket,
                    const struct sockaddr_storage& addr,
                    ssize_t nread,
                    const v8::Local<v8::Object>& slab,
                    size_t offset) {
    HandleScope scope;
    
    EmiSocket *wrap = reinterpret_cast<EmiSocket *>(socket->data);
    
    if (nread < 0) {
        const unsigned argc = 2;
        Handle<Value> argv[argc] = {
            wrap->handle_,
            EmiNodeUtil::errStr(uv_last_error(uv_default_loop()))
        };
        EmiSocket::connectionError->Call(Context::GetCurrent()->Global(), argc, argv);
        return;
    }
    if (nread > 0) {
        wrap->getSock().onMessage(EmiConnection::Now(),
                                  socket,
                                  addr,
                                  slab,
                                  offset,
                                  nread);
    }
}

EmiSockDelegate::EmiSockDelegate(EmiSocket& es) : _es(es) {}

void EmiSockDelegate::closeSocket(EmiSockDelegate& delegate, uv_udp_t *socket) {
    // This allows V8's GC to reclaim the EmiSocket when no UDP sockets are open
    //
    // TODO Perhaps I should do this on the next uv tick, since this might dealloc
    // the whole socket, which will probably not end up well.
    //
    // TODO What happens when this method is actually called from _es's destructor?
    delegate._es.Unref();
    
    EmiNodeUtil::closeSocket(socket);
}

uv_udp_t *EmiSockDelegate::openSocket(const sockaddr_storage& address, Error& error) {
    ES& sock(_es._sock);
    
    uv_udp_t *ret(EmiNodeUtil::openSocket(address,
                                          recv_cb, &sock.getDelegate()._es,
                                          error));
    
    if (ret) {
        // This prevents V8's GC to reclaim the EmiSocket while UDP sockets are open
        sock.getDelegate()._es.Ref();
    }
    
    return ret;
}

void EmiSockDelegate::extractLocalAddress(uv_udp_t *socket, sockaddr_storage& address) {
    int len(sizeof(sockaddr_storage));
    uv_udp_getsockname(socket, (struct sockaddr *)&address, &len);
}

EmiSockDelegate::EC *EmiSockDelegate::makeConnection(const EmiConnParams& params) {
    HandleScope scope;
    
    // TODO I think the HandleScope will dispose of EmiConnection after
    // this function, which is not what we want... Exactly what will
    // happen?
    
    Handle<Object> obj(EmiConnection::NewInstance(_es, params));
    EmiConnection *ec = node::ObjectWrap::Unwrap<EmiConnection>(obj);
    return &ec->getConn();
}

void EmiSockDelegate::sendData(uv_udp_t *socket,
                               const sockaddr_storage& address,
                               const uint8_t *data,
                               size_t size) {
    EmiNodeUtil::sendData(socket, address, data, size);
}

void EmiSockDelegate::gotConnection(EC& conn) {
    HandleScope scope;
    
    Handle<Value> jsHandle(conn.getEmiSock().getDelegate()._es._jsHandle);
    
    const unsigned argc = 3;
    Handle<Value> argv[argc] = {
        jsHandle.IsEmpty() ? Handle<Value>(Undefined()) : jsHandle,
        conn.getEmiSock().getDelegate()._es.handle_,
        conn.getDelegate().getConnection().handle_
    };
    Local<Value> ret = EmiSocket::gotConnection->Call(Context::GetCurrent()->Global(), argc, argv);
    
    if (!ret.IsEmpty() && ret->IsObject()) {
        conn.getDelegate().getConnection().setJsHandle(ret->ToObject());
    }
}

void EmiSockDelegate::connectionOpened(ConnectionOpenedCallbackCookie& cookie,
                                       bool error,
                                       EmiDisconnectReason reason,
                                       EC& conn) {
    HandleScope scope;
    
    const unsigned argc = 2;
    Handle<Value> argv[argc];
    
    // TODO Give the error as something better than just the error code
    argv[0] = error ? Number::New(reason) : Null();
    
    if (error) {
        argv[1] = Null();
    }
    else {
        argv[1] = conn.getDelegate().getConnection().handle_;
    }
    
    Local<Value> ret = cookie->CallAsFunction(Context::GetCurrent()->Global(), argc, argv);
    
    if (!ret.IsEmpty() && ret->IsObject()) {
        conn.getDelegate().getConnection().setJsHandle(ret->ToObject());
    }
    
    cookie.Dispose();
}
