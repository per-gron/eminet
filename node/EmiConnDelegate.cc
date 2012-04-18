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
  EmiConnDelegate *ecd = (EmiConnDelegate *)handle->data;
  ecd->_conn->_conn.connectionWarningCallback(ecd->_warningTimeoutWhenWarningTimerWasScheduled);
}

void EmiConnDelegate::connectionTimeout(uv_timer_t *handle, int status) {
  EmiConnDelegate *ecd = (EmiConnDelegate *)handle->data;
  ecd->_conn->_conn.connectionTimeoutCallback();
}

void EmiConnDelegate::tickTimeout(uv_timer_t *handle, int status) {
  EmiConnDelegate *ecd = (EmiConnDelegate *)handle->data;
  ecd->_conn->_conn.tickTimeoutCallback(EmiConnection::Now());
}

void EmiConnDelegate::heartbeatTimeout(uv_timer_t *handle, int status) {
  EmiConnDelegate *ecd = (EmiConnDelegate *)handle->data;
  ecd->_conn->_conn.heartbeatTimeoutCallback(EmiConnection::Now());
}

void EmiConnDelegate::rtoTimeout(uv_timer_t *handle, int status) {
  EmiConnDelegate *ecd = (EmiConnDelegate *)handle->data;
  ecd->_conn->_conn.rtoTimeoutCallback(EmiConnection::Now(), ecd->_rtoWhenRtoTimerWasScheduled);
}

EmiConnDelegate::EmiConnDelegate(EmiConnection *conn) : _conn(conn) {
  _tickTimer = (uv_timer_t *)malloc(sizeof(uv_timer_t));
  _tickTimer->data = this;
  uv_timer_init(uv_default_loop(), _tickTimer);
  
  _heartbeatTimer = (uv_timer_t *)malloc(sizeof(uv_timer_t));
  _heartbeatTimer->data = this;
  uv_timer_init(uv_default_loop(), _heartbeatTimer);
  
  _rtoTimer = (uv_timer_t *)malloc(sizeof(uv_timer_t));
  _rtoTimer->data = this;
  uv_timer_init(uv_default_loop(), _rtoTimer);
  
  _connectionTimer = (uv_timer_t *)malloc(sizeof(uv_timer_t));
  _connectionTimer->data = this;
  uv_timer_init(uv_default_loop(), _connectionTimer);
}

void EmiConnDelegate::invalidate() {
  uv_timer_stop(_tickTimer);
  uv_close((uv_handle_t *)_tickTimer, close_cb);

  uv_timer_stop(_heartbeatTimer);
  uv_close((uv_handle_t *)_heartbeatTimer, close_cb);
    
  uv_timer_stop(_rtoTimer);
  uv_close((uv_handle_t *)_rtoTimer, close_cb);

  uv_timer_stop(_connectionTimer);
  uv_close((uv_handle_t *)_connectionTimer, close_cb);
}

void EmiConnDelegate::emiConnMessage(EmiChannelQualifier channelQualifier, const unsigned char *data, size_t offset, size_t size) {
  HandleScope scope;
  
  const unsigned argc = 0;
  Handle<Value> argv[argc] = { };
  EmiSocket::connectionMessage->Call(Context::GetCurrent()->Global(), argc, argv);
}

void EmiConnDelegate::scheduleConnectionWarning(EmiTimeInterval warningTimeout) {
  uv_timer_stop(_connectionTimer);
  _warningTimeoutWhenWarningTimerWasScheduled = warningTimeout;
  uv_timer_start(_connectionTimer, EmiConnDelegate::warningTimeout, warningTimeout*NSECS_PER_SEC, /*repeats:*/0);
}

void EmiConnDelegate::scheduleConnectionTimeout(EmiTimeInterval interval) {
  uv_timer_stop(_connectionTimer);
  uv_timer_start(_connectionTimer, EmiConnDelegate::connectionTimeout, interval*NSECS_PER_SEC, /*repeats:*/0);
}

void EmiConnDelegate::ensureTickTimeout(EmiTimeInterval interval) {
  if (!uv_is_active((uv_handle_t *)_tickTimer)) {
    uv_timer_start(_tickTimer, EmiConnDelegate::tickTimeout, interval*NSECS_PER_SEC, /*repeats:*/0);
  }
}

void EmiConnDelegate::scheduleHeartbeatTimeout(EmiTimeInterval interval) {
  uv_timer_stop(_heartbeatTimer);
  uv_timer_start(_heartbeatTimer, EmiConnDelegate::heartbeatTimeout, interval*NSECS_PER_SEC, /*repeats:*/0);
}

void EmiConnDelegate::ensureRtoTimeout(EmiTimeInterval rto) {
  if (!uv_is_active((uv_handle_t *)_rtoTimer)) {
    // this._rto will likely change before the timeout fires. When
    // the timeout fires we want the value of _rto at the time
    // the timeout was set, not when it fires. That's why we store
    // rto with the NSTimer.
    _rtoWhenRtoTimerWasScheduled = rto;
    uv_timer_start(_rtoTimer, EmiConnDelegate::rtoTimeout, rto*NSECS_PER_SEC, /*repeats:*/0);
  }
}

void EmiConnDelegate::invalidateRtoTimeout() {
  uv_timer_stop(_rtoTimer);
}

void EmiConnDelegate::emiConnLost() {
  // TODO Call a real function with real arguments
  HandleScope scope;
  
  const unsigned argc = 0;
  Handle<Value> argv[argc] = { };
  EmiSocket::connectionLost->Call(Context::GetCurrent()->Global(), argc, argv);
}

void EmiConnDelegate::emiConnRegained() {
  // TODO Call a real function with real arguments
  HandleScope scope;
  
  const unsigned argc = 0;
  Handle<Value> argv[argc] = { };
  EmiSocket::connectionRegained->Call(Context::GetCurrent()->Global(), argc, argv);
}

void EmiConnDelegate::emiConnDisconnect(EmiDisconnectReason reason) {
  // TODO Call a real function with real arguments
  HandleScope scope;
  
  const unsigned argc = 1;
  Handle<Value> argv[argc] = { Integer::New(reason) };
  EmiSocket::connectionRegained->Call(Context::GetCurrent()->Global(), argc, argv);
}
