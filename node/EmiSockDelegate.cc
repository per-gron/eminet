#define BUILDING_NODE_EXTENSION

#include "EmiSockDelegate.h"

#include "EmiNodeUtil.h"
#include "EmiSocket.h"
#include "EmiConnection.h"

#include <stdexcept>

using namespace v8;

static void send_cb(uv_udp_send_t* req, int status) {
  free(req);
}

static uv_buf_t alloc_cb(uv_handle_t* handle, size_t suggested_size) {
  // See https://github.com/joyent/node/blob/master/src/udp_wrap.cc for a more elaborate implementation
  
  static char slab[65536];
  
  ASSERT(suggested_size <= sizeof slab);
  
  return uv_buf_init(slab, sizeof slab);
}

static void recv_cb(uv_udp_t* handle,
                    ssize_t nread,
                    uv_buf_t buf,
                    struct sockaddr* addr,
                    unsigned flags) {
  if (flags & UV_UDP_PARTIAL) {
    // TODO Discard this packet
  }
}

static void close_cb(uv_handle_t* handle) {
  free(handle);
}

EmiSockDelegate::EmiSockDelegate(EmiSocket *es) : _es(es) {}

void EmiSockDelegate::closeSocket(uv_udp_t *socket) {
  uv_close((uv_handle_t *)socket, close_cb);
}

uv_udp_t *EmiSockDelegate::openSocket(uint16_t port, Error& error) {
  int err;
  uv_udp_t *socket = (uv_udp_t *)malloc(sizeof(uv_udp_t));
  
  err = uv_udp_init(uv_default_loop(), socket);
  if (0 != err) {
    goto error;
  }
  
  struct sockaddr_in6 addr6;
  addr6.sin6_len       = sizeof(struct sockaddr_in6);
  addr6.sin6_family    = AF_INET6;
  addr6.sin6_port      = htons(port);
  addr6.sin6_flowinfo  = 0;
  addr6.sin6_addr      = in6addr_any;
  addr6.sin6_scope_id  = 0;
  
  err = uv_udp_bind6(socket, addr6, /*flags:*/0);
  if (0 != err) {
    goto error;
  }
  
  err = uv_udp_recv_start(socket, alloc_cb, recv_cb);
  if (0 != err) {
    goto error;
  }
    
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

EC *EmiSockDelegate::makeConnection(const Address& address, uint16_t inboundPort, bool initiator) {
  HandleScope scope;
  
  // TODO I think the HandleScope will dispose of EmiConnection after
  // this function, which is not what we want... Exactly what will
  // happen?
  
  Handle<Object> obj(EmiConnection::NewInstance(_es, address, inboundPort, initiator));
  EmiConnection *ec = node::ObjectWrap::Unwrap<EmiConnection>(obj);
  return &ec->getConn();
}

void EmiSockDelegate::sendData(uv_udp_t *socket, const Address& address, const uint8_t *data, size_t size) {
  uv_udp_send_t *req = (uv_udp_send_t *)malloc(sizeof(uv_udp_send_t)+sizeof(uv_buf_t));
  uv_buf_t      *buf = (uv_buf_t *)&req[1];
  
  *buf = uv_buf_init((char *)data, size);
  
  if (AF_INET == address.ss_family) {
    if (0 != uv_udp_send(req, socket, buf, /*bufcnt:*/1, *((struct sockaddr_in *)&address), send_cb)) {
      free(req);
    }
  }
  else if (AF_INET6 == address.ss_family) {
    if (0 != uv_udp_send6(req, socket, buf, /*bufcnt:*/1, *((struct sockaddr_in6 *)&address), send_cb)) {
      free(req);
    }
  }
  else {
    ASSERT(0);
  }
}

void EmiSockDelegate::gotConnection(EC *conn) {
  // TODO Call a real function with real arguments
  HandleScope scope;
  
  const unsigned argc = 0;
  Handle<Value> argv[argc] = { };
  EmiSocket::gotConnection->Call(Context::GetCurrent()->Global(), argc, argv);
}

void EmiSockDelegate::connectionOpened(ConnectionOpenedCallbackCookie& cookie, bool error, EmiDisconnectReason reason, EC& ec) {
  // TODO
  
  cookie.Dispose();
}

void EmiSockDelegate::panic() {
  throw std::runtime_error("EmiNet internal error");
}
