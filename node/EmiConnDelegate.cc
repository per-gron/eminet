#define BUILDING_NODE_EXTENSION

#include "EmiConnDelegate.h"
#include "EmiNodeUtil.h"

#include "EmiSocket.h"
#include "EmiConnection.h"

using namespace v8;

EmiConnDelegate::EmiConnDelegate(EmiConnection& conn) : _conn(conn) {
}

void EmiConnDelegate::invalidate() {
    if (EMI_CONNECTION_TYPE_SERVER == _conn._conn.getType()) {
        _conn._es._sock.deregisterServerConnection(&_conn._conn);
    }
    
    // This allows V8's GC to reclaim the EmiConnection when it's been closed
    // The corresponding Ref is in EmiConnection::New
    //
    // TODO Perhaps I should do this on the next uv tick, since this might dealloc
    // the whole connection, which will probably not end up well.
    //
    // TODO What happens when this method is actually called from _conn's destructor?
    _conn.Unref();
}

void EmiConnDelegate::emiConnMessage(EmiChannelQualifier channelQualifier,
                                     EmiSequenceNumber sequenceNumber,
                                     const v8::Local<v8::Object>& data,
                                     size_t offset,
                                     size_t size) {
    HandleScope scope;
    
    const unsigned argc = 7;
    Handle<Value> argv[argc] = {
        _conn._jsHandle.IsEmpty() ? Handle<Value>(Undefined()) : _conn._jsHandle,
        _conn.handle_,
        Number::New(channelQualifier),
        Number::New(sequenceNumber),
        data,
        Number::New(offset),
        Number::New(size)
    };
    EmiSocket::connectionMessage->Call(Context::GetCurrent()->Global(), argc, argv);
}

void EmiConnDelegate::emiConnLost() {
    HandleScope scope;
    
    const unsigned argc = 2;
    Handle<Value> argv[argc] = {
        _conn._jsHandle.IsEmpty() ? Handle<Value>(Undefined()) : _conn._jsHandle,
        _conn.handle_
    };
    EmiSocket::connectionLost->Call(Context::GetCurrent()->Global(), argc, argv);
}

void EmiConnDelegate::emiConnRegained() {
    HandleScope scope;
    
    const unsigned argc = 2;
    Handle<Value> argv[argc] = {
        _conn._jsHandle.IsEmpty() ? Handle<Value>(Undefined()) : _conn._jsHandle,
        _conn.handle_
    };
    EmiSocket::connectionRegained->Call(Context::GetCurrent()->Global(), argc, argv);
}

void EmiConnDelegate::emiConnDisconnect(EmiDisconnectReason reason) {
    HandleScope scope;
    
    const unsigned argc = 3;
    Handle<Value> argv[argc] = {
        _conn._jsHandle.IsEmpty() ? Handle<Value>(Undefined()) : _conn._jsHandle,
        _conn.handle_,
        Integer::New(reason) // TODO Give something better than just the error code
    };
    EmiSocket::connectionDisconnect->Call(Context::GetCurrent()->Global(), argc, argv);
}

void EmiConnDelegate::emiNatPunchthroughFinished(bool success) {
    HandleScope scope;
    
    const unsigned argc = 3;
    Handle<Value> argv[argc] = {
        _conn._jsHandle.IsEmpty() ? Handle<Value>(Undefined()) : _conn._jsHandle,
        _conn.handle_,
        Boolean::New(success)
    };
    EmiSocket::natPunchthroughFinished->Call(Context::GetCurrent()->Global(), argc, argv);
}
