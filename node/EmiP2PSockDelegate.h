#define BUILDING_NODE_EXTENSION
#ifndef emilir_EmiP2PSockDelegate_h
#define emilir_EmiP2PSockDelegate_h

#include "EmiError.h"
#include "EmiBinding.h"

#include "../core/EmiTypes.h"
#include <node.h>
#include <node_buffer.h>
#include <uv.h>

class EmiP2PSocket;
class EmiP2PSockDelegate;
template<class P2PSockDelegate>
class EmiP2PSock;

class EmiP2PSockDelegate {
    typedef EmiP2PSock<EmiP2PSockDelegate> EPS;
    
    typedef EmiBinding::Error   Error;
    typedef EmiBinding::Address Address;
    
    EmiP2PSocket& _es;
public:
    
    typedef EmiBinding                 Binding;
    typedef v8::Persistent<v8::Object> ConnectionOpenedCallbackCookie;
    
    EmiP2PSockDelegate(EmiP2PSocket& es);
    
    static void closeSocket(EPS& sock, uv_udp_t *socket);
    uv_udp_t *openSocket(const Address& address, uint16_t port, Error& err);
    
    static void sendData(uv_udp_t *socket,
                         const Address& address,
                         const uint8_t *data,
                         size_t size);
};

#endif
