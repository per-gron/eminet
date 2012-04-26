#define BUILDING_NODE_EXTENSION

#include "EmiP2PSockDelegate.h"

#include "EmiNodeUtil.h"
#include "EmiP2PSocket.h"
#include "EmiConnection.h"

#include "../core/EmiP2PSock.h"
#include <node.h>

using namespace v8;

static void recv_cb(uv_udp_t *socket,
                    const struct sockaddr_storage& addr,
                    ssize_t nread,
                    const v8::Local<v8::Object>& slab,
                    size_t offset) {
    HandleScope scope;
    
    EmiP2PSocket *wrap = reinterpret_cast<EmiP2PSocket *>(socket->data);
    
    if (nread < 0) {
        const unsigned argc = 2;
        Handle<Value> argv[argc] = {
            wrap->handle_,
            EmiNodeUtil::errStr(uv_last_error(uv_default_loop()))
        };
        EmiP2PSocket::connectionError->Call(Context::GetCurrent()->Global(), argc, argv);
        return;
    }
    if (nread > 0) {
        wrap->getP2PSock().onMessage(EmiConnection::Now(),
                                     socket,
                                     addr,
                                     slab,
                                     offset,
                                     nread);
    }
}

EmiP2PSockDelegate::EmiP2PSockDelegate(EmiP2PSocket& es) : _es(es) {}

void EmiP2PSockDelegate::closeSocket(EmiP2PSockDelegate::EPS& sock, uv_udp_t *socket) {
    // This allows V8's GC to reclaim the EmiSocket when no UDP sockets are open
    //
    // TODO Perhaps I should do this on the next uv tick, since this might dealloc
    // the whole socket, which will probably not end up well.
    //
    // TODO What happens when this method is actually called from _es's destructor?
    sock.getDelegate()._es.Unref();
    
    EmiNodeUtil::closeSocket(socket);
}

uv_udp_t *EmiP2PSockDelegate::openSocket(uint16_t port, Error& error) {
    EPS& sock(_es._sock);
    
    uv_udp_t *ret(EmiNodeUtil::openSocket(sock.config.address, port,
                                          recv_cb, &sock.getDelegate()._es,
                                          error));
    
    if (ret) {
        // This prevents V8's GC to reclaim the EmiSocket while UDP sockets are open
        sock.getDelegate()._es.Ref();
    }
    
    return ret;
}

void EmiP2PSockDelegate::sendData(uv_udp_t *socket,
                               const Address& address,
                               const uint8_t *data,
                               size_t size) {
    EmiNodeUtil::sendData(socket, address, data, size);
}
