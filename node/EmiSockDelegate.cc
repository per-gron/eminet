#define BUILDING_NODE_EXTENSION

#include "EmiSockDelegate.h"

#include "EmiNodeUtil.h"
#include "EmiSocket.h"
#include "EmiConnection.h"
#include "slab_allocator.h"

#include <stdexcept>
#include <node.h>

#define SLAB_SIZE (1024 * 1024)

using namespace v8;

static node::SlabAllocator slab_allocator(SLAB_SIZE);

static Handle<String> errStr(uv_err_t err) {
    HandleScope scope;
    
    Local<String> errStr;
    
    if (err.code == UV_UNKNOWN) {
        char errno_buf[100];
        snprintf(errno_buf, 100, "Unknown system errno %d", err.sys_errno_);
        errStr = String::New(errno_buf);
    } else {
        errStr = String::NewSymbol(uv_err_name(err));
    }
    
    return scope.Close(errStr);
}

inline static uint16_t sockaddrPort(const struct sockaddr_storage& addr) {
    if (AF_INET6 == addr.ss_family) {
        struct sockaddr_in6& addr6(*((struct sockaddr_in6 *)&addr));
        return addr6.sin6_port;
    }
    else if (AF_INET == addr.ss_family) {
        struct sockaddr_in& addr(*((struct sockaddr_in *)&addr));
        return addr.sin_port;
    }
    else {
        ASSERT(0 && "unexpected address family");
        abort();
    }
}

static void send_cb(uv_udp_send_t* req, int status) {
    free(req);
}

static uv_buf_t alloc_cb(uv_handle_t* handle, size_t suggested_size) {
    node::ObjectWrap *wrap = static_cast<node::ObjectWrap *>(handle->data);
    char *buf = slab_allocator.Allocate(wrap->handle_, suggested_size);
    return uv_buf_init(buf, suggested_size);
}

static void recv_cb(uv_udp_t *handle,
                    ssize_t nread,
                    uv_buf_t buf,
                    struct sockaddr *addr,
                    unsigned flags) {
    HandleScope scope;
    
    printf("!? recv_cb %ld\n", nread);
    
    EmiSocket *wrap = reinterpret_cast<EmiSocket *>(handle->data);
    Local<Object> slab = slab_allocator.Shrink(wrap->handle_,
                                               buf.base,
                                               nread < 0 ? 0 : nread);
    if (nread == 0) return;
    
    if (nread < 0) {
        const unsigned argc = 2;
        Handle<Value> argv[argc] = {
            wrap->handle_,
            errStr(uv_last_error(uv_default_loop()))
        };
        EmiSocket::connectionError->Call(Context::GetCurrent()->Global(), argc, argv);
        return;
    }
    
    if (flags & UV_UDP_PARTIAL) {
        // Discard the packet
        return;
    }
    
    wrap->getSock().onMessage(EmiConnection::Now(),
                              handle,
                              sockaddrPort(*((struct sockaddr_storage *)addr)),
                              *((struct sockaddr_storage *)addr),
                              slab,
                              buf.base - node::Buffer::Data(slab),
                              nread);
}

static void close_cb(uv_handle_t* handle) {
    free(handle);
}

EmiSockDelegate::EmiSockDelegate(EmiSocket& es) : _es(es) {}

void EmiSockDelegate::closeSocket(EmiSockDelegate::ES& sock, uv_udp_t *socket) {
    // This allows V8's GC to reclaim the EmiSocket when no UDP sockets are open
    //
    // TODO Perhaps I should do this on the next uv tick, since this might dealloc
    // the whole socket, which will probably not end up well.
    //
    // TODO What happens when this method is actually called from _es's destructor?
    sock.getDelegate()._es.Unref();
    
    uv_close((uv_handle_t *)socket, close_cb);
}

uv_udp_t *EmiSockDelegate::openSocket(EmiSockDelegate::ES& sock, uint16_t port, Error& error) {
    int err;
    uv_udp_t *socket = (uv_udp_t *)malloc(sizeof(uv_udp_t));
    
    err = uv_udp_init(uv_default_loop(), socket);
    if (0 != err) {
        goto error;
    }
    
    if (AF_INET == sock.config.address.ss_family) {
        struct sockaddr_in& addr(*((struct sockaddr_in *)&sock.config.address));
        
        char buf[100];
        uv_ip4_name(&addr, buf, sizeof(buf));
        
        err = uv_udp_bind(socket, addr, /*flags:*/0);
        if (0 != err) {
            goto error;
        }
    }
    else if (AF_INET6 == sock.config.address.ss_family) {
        struct sockaddr_in6& addr6(*((struct sockaddr_in6 *)&sock.config.address));
        err = uv_udp_bind6(socket, addr6, /*flags:*/0);
        if (0 != err) {
            goto error;
        }
    }
    else {
        ASSERT(0 && "unexpected address family");
        abort();
    }
    
    err = uv_udp_recv_start(socket, alloc_cb, recv_cb);
    if (0 != err) {
        goto error;
    }
    
    socket->data = &sock.getDelegate()._es;
    
    // This prevents V8's GC to reclaim the EmiSocket while UDP sockets are open
    sock.getDelegate()._es.Ref();
    
    return socket;
    
error:
    free(socket);
    return NULL;
}

uint16_t EmiSockDelegate::extractLocalPort(uv_udp_t *socket) {
    Address address;
    int len(sizeof(address));
    
    uv_udp_getsockname(socket, (struct sockaddr *)&address, &len);
    
    if (sizeof(sockaddr_in) == len) {
        sockaddr_in *addr = (sockaddr_in *)&address;
        return addr->sin_port;
    }
    else if (sizeof(sockaddr_in6) == len) {
        sockaddr_in6 *addr = (sockaddr_in6 *)&address;
        return addr->sin6_port;
    }
    else {
        ASSERT(0 && "Invalid sockaddr size");
    }
}

EmiSockDelegate::EC *EmiSockDelegate::makeConnection(const Address& address,
                                                     uint16_t inboundPort,
                                                     bool initiator) {
    HandleScope scope;
    
    // TODO I think the HandleScope will dispose of EmiConnection after
    // this function, which is not what we want... Exactly what will
    // happen?
    
    Handle<Object> obj(EmiConnection::NewInstance(_es,
                                                  address,
                                                  inboundPort,
                                                  initiator));
    EmiConnection *ec = node::ObjectWrap::Unwrap<EmiConnection>(obj);
    return &ec->getConn();
}

void EmiSockDelegate::sendData(uv_udp_t *socket,
                               const Address& address,
                               const uint8_t *data,
                               size_t size) {
    uv_udp_send_t *req = (uv_udp_send_t *)malloc(sizeof(uv_udp_send_t)+
                                                 sizeof(uv_buf_t));
    uv_buf_t      *buf = (uv_buf_t *)&req[1];
    
    *buf = uv_buf_init((char *)data, size);
    
    if (AF_INET == address.ss_family) {
        if (0 != uv_udp_send(req,
                             socket,
                             buf,
                             /*bufcnt:*/1,
                             *((struct sockaddr_in *)&address),
                             send_cb)) {
            free(req);
        }
    }
    else if (AF_INET6 == address.ss_family) {
        if (0 != uv_udp_send6(req,
                              socket,
                              buf,
                              /*bufcnt:*/1,
                              *((struct sockaddr_in6 *)&address),
                              send_cb)) {
            free(req);
        }
    }
    else {
        ASSERT(0 && "unexpected address family");
    }
}

void EmiSockDelegate::gotConnection(EC& conn) {
    HandleScope scope;
    
    const unsigned argc = 2;
    Handle<Value> argv[argc] = {
        conn.getEmiSock().getDelegate()._es.handle_,
        conn.getDelegate().getConnection().handle_
    };
    EmiSocket::gotConnection->Call(Context::GetCurrent()->Global(), argc, argv);
}

void EmiSockDelegate::connectionOpened(ConnectionOpenedCallbackCookie& cookie,
                                       bool error,
                                       EmiDisconnectReason reason,
                                       EC& conn) {
    const unsigned argc = 3;
    Handle<Value> argv[argc];
    argv[0] = conn.getEmiSock().getDelegate()._es.handle_;
    
    // TODO Give the error as something better than just the error code
    argv[1] = error ? Number::New(reason) : Null();
    
    if (error) {
        argv[2] = Null();
    }
    else {
        argv[2] = conn.getDelegate().getConnection().handle_;
    }
    
    cookie->CallAsFunction(Context::GetCurrent()->Global(), argc, argv);
    
    cookie.Dispose();
}

void EmiSockDelegate::panic() {
    throw std::runtime_error("EmiNet internal error");
}

Persistent<Object> EmiSockDelegate::makePersistentData(const Local<Object>& data,
                                                       size_t offset,
                                                       size_t length) {
    HandleScope scope;
    
    // Copy the buffer
    node::Buffer *buf(node::Buffer::New(node::Buffer::Data(data)+offset,
                                        length));
    
    // Make a new persistent handle (do not just reuse the persistent buf->handle_ handle)
    return Persistent<Object>::New(buf->handle_);
}