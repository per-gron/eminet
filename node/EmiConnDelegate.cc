#define BUILDING_NODE_EXTENSION

#include "EmiConnDelegate.h"
#include "EmiNodeUtil.h"

#include "EmiSocket.h"
#include "EmiConnection.h"

using namespace v8;

static void close_cb(uv_handle_t* handle) {
    free(handle);
}

void EmiConnDelegate::warningTimeout(uv_timer_t *handle, int status) {
    EmiConnection *conn = (EmiConnection *)handle->data;
    conn->_conn.connectionWarningCallback(conn->_conn.getDelegate()._warningTimeoutWhenWarningTimerWasScheduled);
}

void EmiConnDelegate::connectionTimeout(uv_timer_t *handle, int status) {
    EmiConnection *conn = (EmiConnection *)handle->data;
    conn->_conn.connectionTimeoutCallback();
}

EmiConnDelegate::EmiConnDelegate(EmiConnection& conn) : _conn(conn) {
    _connectionTimer = (uv_timer_t *)malloc(sizeof(uv_timer_t));
    _connectionTimer->data = &conn;
    uv_timer_init(uv_default_loop(), _connectionTimer);
}

void EmiConnDelegate::invalidate() {
    uv_timer_stop(_connectionTimer);
    uv_close((uv_handle_t *)_connectionTimer, close_cb);
    
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
                                     const v8::Local<v8::Object>& data,
                                     size_t offset,
                                     size_t size) {
    HandleScope scope;
    
    const unsigned argc = 6;
    Handle<Value> argv[argc] = {
        _conn._jsHandle.IsEmpty() ? Handle<Value>(Undefined()) : _conn._jsHandle,
        _conn.handle_,
        Number::New(channelQualifier),
        data,
        Number::New(offset),
        Number::New(size)
    };
    EmiSocket::connectionMessage->Call(Context::GetCurrent()->Global(), argc, argv);
}

void EmiConnDelegate::scheduleConnectionWarning(EmiTimeInterval warningTimeout) {
    uv_timer_stop(_connectionTimer);
    _warningTimeoutWhenWarningTimerWasScheduled = warningTimeout;
    uv_timer_start(_connectionTimer,
                   EmiConnDelegate::warningTimeout,
                   warningTimeout*EmiNodeUtil::MSECS_PER_SEC,
                   /*repeats:*/0);
}

void EmiConnDelegate::scheduleConnectionTimeout(EmiTimeInterval interval) {
    uv_timer_stop(_connectionTimer);
    uv_timer_start(_connectionTimer,
                   EmiConnDelegate::connectionTimeout,
                   interval*EmiNodeUtil::MSECS_PER_SEC,
                   /*repeats:*/0);
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
