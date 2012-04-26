//
//  EmiConn.h
//  roshambo
//
//  Created by Per Eckerdal on 2012-04-10.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#ifndef emilir_EmiConn_h
#define emilir_EmiConn_h

#include "EmiSock.h"
#include "EmiTypes.h"
#include "EmiSenderBuffer.h"
#include "EmiReceiverBuffer.h"
#include "EmiSendQueue.h"
#include "EmiLogicalConnection.h"
#include "EmiMessage.h"
#include "EmiConnTime.h"
#include "EmiRtoTimer.h"
#include "EmiP2PData.h"

template<class SockDelegate, class ConnDelegate>
class EmiConn {
    typedef typename SockDelegate::Binding   Binding;
    typedef typename Binding::Error          Error;
    typedef typename Binding::PersistentData PersistentData;
    typedef typename Binding::TemporaryData  TemporaryData;
    typedef typename Binding::Address        Address;
    typedef typename Binding::Timer          Timer;
    
    typedef typename SockDelegate::ConnectionOpenedCallbackCookie ConnectionOpenedCallbackCookie;
    
    typedef EmiSock<SockDelegate, ConnDelegate> ES;
    typedef EmiLogicalConnection<SockDelegate, ConnDelegate> ELC;
    typedef EmiSendQueue<SockDelegate, ConnDelegate> ESQ;
    typedef EmiReceiverBuffer<SockDelegate, EmiConn> ERB;
    
    const uint16_t _inboundPort;
    const Address _address;
    
    ES &_emisock;
    
    EmiP2PData        _p2p;
    EmiConnectionType _type;
    
    ELC *_conn;
    EmiSenderBuffer<Binding> _senderBuffer;
    ERB _receiverBuffer;
    ESQ _sendQueue;
    EmiConnTime _time;
    
    bool _sentDataSinceLastHeartbeat;
    
    ConnDelegate _delegate;
    
    Timer                         *_tickTimer;
    Timer                         *_heartbeatTimer;
    EmiRtoTimer<Binding, EmiConn>  _rtoTimer;
    Timer                         *_connectionTimer;
    EmiTimeInterval                _warningTimeoutWhenWarningTimerWasScheduled;
    
    bool _issuedConnectionWarning;
    
private:
    // Private copy constructor and assignment operator
    inline EmiConn(const EmiConn& other);
    inline EmiConn& operator=(const EmiConn& other);
    
    void deleteELC(ELC *elc) {
        // This if ensures that ConnDelegate::invalidate is only invoked once.
        if (elc) {
            _delegate.invalidate();
            delete elc;
        }
    }
    
public:
    
    // Invoked by EmiReceiverBuffer
    inline void gotReceiverBufferMessage(typename ERB::Entry *entry) {
        if (!_conn) return;
        
        // gotMessage should only return false if the message arrived out of order or
        // some other similar error occured, but that should not happen because this
        // callback should only be called by the receiver buffer for messages that are
        // exactly in order.
        ASSERT(_conn->gotMessage(entry->header, Binding::castToTemporary(entry->data), 0, /*dontFlush:*/true));
    }
    
    EmiConn(const ConnDelegate& delegate, ES& socket, const EmiConnParams& params) :
    _inboundPort(params.inboundPort),
    _address(params.address),
    _conn(NULL),
    _delegate(delegate),
    _emisock(socket),
    _type(params.type),
    _p2p(params.p2p),
    _senderBuffer(_emisock.config.senderBufferSize),
    _receiverBuffer(_emisock.config.receiverBufferSize, *this),
    _sendQueue(*this),
    _time(),
    _sentDataSinceLastHeartbeat(false),
    _tickTimer(Binding::makeTimer()),
    _heartbeatTimer(Binding::makeTimer()),
    _rtoTimer(_time, *this),
    _connectionTimer(Binding::makeTimer()),
    _warningTimeoutWhenWarningTimerWasScheduled(0),
    _issuedConnectionWarning(false) {
        resetConnectionTimeout();
    }
    
    virtual ~EmiConn() {
        Binding::freeTimer(_tickTimer);
        Binding::freeTimer(_heartbeatTimer);
        Binding::freeTimer(_connectionTimer);
        
        _emisock.deregisterConnection(this);
        
        deleteELC(_conn);
    }
    
    EmiTimeInterval timeBeforeConnectionWarning() const {
        return 1/_emisock.config.heartbeatFrequency * _emisock.config.heartbeatsBeforeConnectionWarning;
    }
    
    static void connectionTimeoutCallback(EmiTimeInterval now, Timer *timer, void *data) {
        EmiConn *conn = (EmiConn *)data;
        
        conn->forceClose(EMI_REASON_CONNECTION_TIMED_OUT);
    }
    static void connectionWarningCallback(EmiTimeInterval now, Timer *timer, void *data) {
        EmiConn *conn = (EmiConn *)data;
        
        EmiTimeInterval connectionTimeout = conn->_emisock.config.connectionTimeout;
        
        conn->_issuedConnectionWarning = true;
        Binding::scheduleTimer(conn->_connectionTimer, connectionTimeoutCallback, conn,
                               connectionTimeout - conn->_warningTimeoutWhenWarningTimerWasScheduled,
                               /*repeating:*/false);
        
        conn->_delegate.emiConnLost();
    }
    void resetConnectionTimeout() {
        EmiTimeInterval warningTimeout = timeBeforeConnectionWarning();
        EmiTimeInterval connectionTimeout = _emisock.config.connectionTimeout;
        
        if (warningTimeout < connectionTimeout) {
            _warningTimeoutWhenWarningTimerWasScheduled = warningTimeout;
            Binding::scheduleTimer(_connectionTimer, connectionWarningCallback,
                                   this, warningTimeout, /*repeating:*/false);
        }
        else {
            Binding::scheduleTimer(_connectionTimer, connectionTimeoutCallback,
                                   this, connectionTimeout, /*repeating:*/false);
        }
        
        if (_issuedConnectionWarning) {
            _issuedConnectionWarning = false;
            _delegate.emiConnRegained();
        }
    }
    
    static void tickTimeoutCallback(EmiTimeInterval now, Timer *timer, void *data) {
        EmiConn *conn = (EmiConn *)data;
        
        if (conn->flush(now)) {
            conn->resetHeartbeatTimeout();
        }
    }
    void ensureTickTimeout() {
        if (!Binding::timerIsActive(_tickTimer)) {
            Binding::scheduleTimer(_tickTimer, tickTimeoutCallback, this, 1/_emisock.config.tickFrequency, /*repeating:*/false);
        }
    }
    
    static void heartbeatTimeoutCallback(EmiTimeInterval now, Timer *timer, void *data) {
        EmiConn *conn = (EmiConn *)data;
        
        if (!conn->_sentDataSinceLastHeartbeat) {
            conn->_sendQueue.enqueueHeartbeat();
            conn->ensureTickTimeout();
        }
        conn->resetHeartbeatTimeout();
    }
    void resetHeartbeatTimeout() {
        _sentDataSinceLastHeartbeat = false;
        
        // Don't send heartbeats until we've got a response from the remote host
        if (!isOpening()) {
            Binding::scheduleTimer(_heartbeatTimer, heartbeatTimeoutCallback,
                                   this, 1/_emisock.config.heartbeatFrequency,
                                   /*repeating:*/false);
        }
    }
    
    void rtoTimeout(EmiTimeInterval now, EmiTimeInterval rtoWhenRtoTimerWasScheduled) {
        _senderBuffer.eachCurrentMessage(now, rtoWhenRtoTimerWasScheduled, ^(EmiMessage<Binding> *msg) {
            Error err;
            // Reliable is set to false, because if the message is reliable, it is
            // already in the sender buffer and shouldn't be reinserted anyway
            
            // enqueueMessage can't fail because the reliable parameter is false
            ASSERT(enqueueMessage(now, msg, /*reliable:*/false, err));
        });
    }
    
    void forceClose(EmiDisconnectReason reason) {
        _emisock.deregisterConnection(this);
        
        if (_conn) {
            ELC *conn = _conn;
            
            // _conn is NULLed out to ensure that we don't fire
            // several disconnect events, which would happen if
            // the disconnect delegate callback calls forceClose.
            _conn = NULL;
            
            conn->wasClosed(reason);
            
            // Because we just NULLed out _conn, we need to delete
            // it.
            deleteELC(conn);
        }
    }
    
    void sentPacket() {
        _sentDataSinceLastHeartbeat = true;
    }
    void gotPacket() {
        resetConnectionTimeout();
    }
    void gotTimestamp(EmiTimeInterval now, const uint8_t *data, size_t len) {
        _time.gotTimestamp(_emisock.config.heartbeatFrequency, now, data, len);
    }
    
    // Delegates to EmiSendQueue
    void enqueueAck(EmiChannelQualifier channelQualifier, EmiSequenceNumber sequenceNumber) {
        if (_sendQueue.enqueueAck(channelQualifier, sequenceNumber)) {
            ensureTickTimeout();
        }
    }
    
    // Delegates to EmiSenderBuffer
    void deregisterReliableMessages(int32_t channelQualifier, EmiSequenceNumber sequenceNumber) {
        _senderBuffer.deregisterReliableMessages(channelQualifier, sequenceNumber);
        
        // This will clear the rto timeout if the sender buffer is empty
        _rtoTimer.updateRtoTimeout();
    }
    
    bool senderBufferIsEmpty(EmiRtoTimer<Binding, EmiConn>& ert) const {
        return _senderBuffer.empty();
    }
    
    /// Delegates to EmiReceiverBuffer
    void bufferMessage(const EmiMessageHeader& header, const TemporaryData& buf, size_t offset, size_t length) {
        _receiverBuffer.bufferMessage(header, buf, offset, length);
    }
    void flushBuffer(EmiChannelQualifier channelQualifier, EmiSequenceNumber sequenceNumber) {
        _receiverBuffer.flushBuffer(channelQualifier, sequenceNumber);
    }
    
    // Returns false if the sender buffer didn't have space for the message.
    // Failing only happens for reliable mesages.
    bool enqueueMessage(EmiTimeInterval now, EmiMessage<Binding> *msg, bool reliable, Error& err) {
        if (reliable) {
            if (!_senderBuffer.registerReliableMessage(msg, err, now)) {
                return false;
            }
            _rtoTimer.updateRtoTimeout();
        }
        
        _sendQueue.enqueueMessage(msg, _time, now);
        ensureTickTimeout();
        
        return true;
    }
    
    // The first time this methods is called, it opens the EmiConnection and returns true.
    // Subsequent times it just resends the init message and returns false.
    bool opened(EmiTimeInterval now, EmiSequenceNumber otherHostInitialSequenceNumber) {
        ASSERT(EMI_CONNECTION_TYPE_SERVER == _type);
        
        if (_conn) {
            // sendInitMessage should not fail, because it can only
            // fail when it attempts to send a SYN message, but we're
            // sending a SYN-RST message here.
            Error err;
            ASSERT(_conn->sendInitMessage(now, err));
            
            return false;
        }
        else {
            _conn = new ELC(this, now, otherHostInitialSequenceNumber);
            return true;
        }
    }
    bool open(EmiTimeInterval now, const ConnectionOpenedCallbackCookie& cookie) {
        ASSERT(EMI_CONNECTION_TYPE_CLIENT == _type);
        
        if (_conn) {
            // We don't need to explicitly resend the init message here;
            // SYN connection init messages like this are reliable messages
            // and will be resent automatically when appropriate.
            return false;
        }
        else {
            _conn = new ELC(this, now, cookie);
            return true;
        }
    }
    
    // Methods that delegate to EmiLogicalConnetion
#define X(msg)                  \
    void got##msg() {           \
        if (_conn) {            \
            _conn->got##msg();  \
        }                       \
    }
    X(Prx);
    X(Rst);
    X(SynRstAck);
    X(PrxRstAck);
#undef X
    // Delegates to EmiLogicalConnection
    bool gotSynRst(EmiSequenceNumber otherHostInitialSequenceNumber) {
        return _conn && _conn->gotSynRst(otherHostInitialSequenceNumber);
    }
    // Delegates to EmiLogicalConnection
    bool gotMessage(const EmiMessageHeader& header, const TemporaryData& data, size_t offset, bool dontFlush) {
        if (!_conn) {
            return false;
        }
        else {
            return _conn->gotMessage(header, data, offset, dontFlush);
        }
    }
    
    
    // Invoked by EmiLogicalConnection
    void emitDisconnect(EmiDisconnectReason reason) {
        _delegate.emiConnDisconnect(reason);
    }
    void emitMessage(EmiChannelQualifier channelQualifier, const TemporaryData& data, size_t offset, size_t size) {
        _delegate.emiConnMessage(channelQualifier, data, offset, size);
    }
    
    bool close(EmiTimeInterval now, Error& err) {
        if (_conn) {
            return _conn->close(now, err);
        }
        else {
            // We're already closed
            err = Binding::makeError("com.emilir.eminet.closed", 0);
            return false;
        }
    }
    // Immediately closes the connection without notifying the other host.
    //
    // Note that if this method is used, there is no guarantee that you can
    // reconnect to the remote host immediately; this host immediately forgets
    // that it has been connected, but the other host does not. When
    // reconnecting, this host's OS might pick the same inbound port, and that
    // will confuse the remote host so the connection won't be established.
    void forceClose() {
        forceClose(EMI_REASON_THIS_HOST_CLOSED);
    }
    
    // Delegates to EmiSendQueue
    bool flush(EmiTimeInterval now) {
        return _sendQueue.flush(_time, now);
    }
    
    // Delegates to EmiLogicalConnection
    //
    // This method assumes ownership over the data parameter, and will release it
    // with SockDelegate::releaseData when it's done with it. The buffer must not
    // be modified or released until after SockDelegate::releasePersistentData has
    // been called on it.
    bool send(EmiTimeInterval now, const PersistentData& data, EmiChannelQualifier channelQualifier, EmiPriority priority, Error& err) {
        if (!_conn || _conn->isClosing()) {
            err = Binding::makeError("com.emilir.eminet.closed", 0);
            Binding::releasePersistentData(data);
            return false;
        }
        else {
            return _conn->send(data, now, channelQualifier, priority, err);
        }
    }
    
    ConnDelegate& getDelegate() {
        return _delegate;
    }
    
    const ConnDelegate& getDelegate() const {
        return _delegate;
    }
    void setDelegate(const ConnDelegate& delegate) {
        _delegate = delegate;
    }
    
    uint16_t getInboundPort() const {
        return _inboundPort;
    }
    
    const Address& getAddress() const {
        return _address;
    }
    
    bool issuedConnectionWarning() const {
        return _issuedConnectionWarning;
    }
    EmiConnectionType getType() const {
        return _type;
    }
    bool isOpen() const {
        return _conn && !_conn->isOpening() && !_conn->isClosing();
    }
    bool isOpening() const {
        return _conn && _conn->isOpening();
    }
    EmiSequenceNumber getOtherHostInitialSequenceNumber() const {
        return _conn ? _conn->getOtherHostInitialSequenceNumber() : 0;
    }
    
    // Invoked by EmiSendQueue
    void sendDatagram(const uint8_t *data, size_t size) {
        _emisock.sendDatagram(this, data, size);
    }
    
    inline ES &getEmiSock() {
        return _emisock;
    }
    
    inline const EmiP2PData &getP2PData() const {
        return _p2p;
    }
};

#endif
