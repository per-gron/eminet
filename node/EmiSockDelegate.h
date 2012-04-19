#define BUILDING_NODE_EXTENSION
#ifndef emilir_EmiSockDelegate_h
#define emilir_EmiSockDelegate_h

#include "EmiError.h"

#include "../core/EmiTypes.h"
#include <node.h>
#include <node_buffer.h>
#include <uv.h>

class EmiSocket;
class EmiSockDelegate;
class EmiConnDelegate;
class EmiAddressCmp;
template<class SockDelegate, class ConnDelegate>
class EmiSock;
template<class SockDelegate, class ConnDelegate>
class EmiConn;

class EmiSockDelegate {
    typedef EmiSock<EmiSockDelegate, EmiConnDelegate> ES;
    typedef EmiConn<EmiSockDelegate, EmiConnDelegate> EC;
    
    EmiSocket& _es;
public:
    
    typedef EmiError                   Error;
    typedef EmiAddressCmp              AddressCmp;
    typedef uv_udp_t                   SocketHandle;
    typedef struct sockaddr_storage    Address;
    typedef v8::Persistent<v8::Object> Data;
    typedef v8::Persistent<v8::Object> ConnectionOpenedCallbackCookie;
    
    EmiSockDelegate(EmiSocket& es);
    
    static void closeSocket(ES& sock, uv_udp_t *socket);
    uv_udp_t *openSocket(ES& sock, uint16_t port, Error& err);
    static uint16_t extractLocalPort(uv_udp_t *socket);
    
    EC *makeConnection(const Address& address, uint16_t inboundPort, bool initiator);
    
    void sendData(uv_udp_t *socket,
                  const Address& address,
                  const uint8_t *data,
                  size_t size);
    void gotConnection(EC& conn);
    
    static void connectionOpened(ConnectionOpenedCallbackCookie& cookie,
                                 bool error,
                                 EmiDisconnectReason reason,
                                 EC& ec);
    
    static void panic();
    
    inline static EmiError makeError(const char *domain, int32_t code) {
        return EmiError(domain, code);
    }
    
    inline static void releaseData(v8::Persistent<v8::Object> buf) {
        buf.Dispose();
    }
    
    inline static const uint8_t *extractData(v8::Handle<v8::Object> data) {
        return (const uint8_t *)(data.IsEmpty() ? NULL : node::Buffer::Data(data));
    }
    inline static size_t extractLength(v8::Handle<v8::Object> data) {
        return data.IsEmpty() ? 0 : node::Buffer::Length(data);
    }
};

#endif
