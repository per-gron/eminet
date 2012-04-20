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

template<class SockDelegate, class ConnDelegate>
class EmiConn {
    typedef typename SockDelegate::Error          Error;
    typedef typename SockDelegate::PersistentData PersistentData;
    typedef typename SockDelegate::TemporaryData  TemporaryData;
    typedef typename SockDelegate::Address        Address;
    typedef typename SockDelegate::ConnectionOpenedCallbackCookie ConnectionOpenedCallbackCookie;
    
    typedef EmiSock<SockDelegate, ConnDelegate> ES;
    typedef EmiLogicalConnection<SockDelegate, ConnDelegate> ELC;
    typedef EmiSendQueue<SockDelegate, ConnDelegate> ESQ;
    typedef EmiReceiverBuffer<SockDelegate, EmiConn> ERB;
    
    const uint16_t _inboundPort;
    const Address _address;
    
    ES &_emisock;
    bool _initiator;
    
    ELC *_conn;
    EmiSenderBuffer<SockDelegate> _senderBuffer;
    ERB _receiverBuffer;
    ESQ _sendQueue;
    EmiConnTime _time;
    
    bool _receivedDataSinceLastHeartbeat;
    
    ConnDelegate _delegate;
    
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
        
        if (!_conn->gotMessage(&entry->header, SockDelegate::castToTemporary(entry->data), entry->offset, true /* dontFlush */)) {
            // gotMessage should only return false if the message arrived out of order or
            // some other similar error occured, but that should not happen because this
            // callback should only be called by the receiver buffer for messages that are
            // exactly in order.
            SockDelegate::panic();
        }
    }
    
    EmiConn(const ConnDelegate& delegate, uint16_t inboundPort, const Address& address, ES& socket, bool initiator) :
    _inboundPort(inboundPort),
    _address(address),
    _conn(NULL),
    _delegate(delegate),
    _emisock(socket),
    _initiator(initiator),
    _senderBuffer(_emisock.config.senderBufferSize),
    _receiverBuffer(_emisock.config.receiverBufferSize, *this),
    _sendQueue(*this),
    _time(),
    _receivedDataSinceLastHeartbeat(false),
    _issuedConnectionWarning(false) {
        resetConnectionTimeout();
    }
    
    virtual ~EmiConn() {
        _emisock.deregisterConnection(this);
        deleteELC(_conn);
    }
    
    EmiTimeInterval timeBeforeConnectionWarning() const {
        return 1/_emisock.config.heartbeatFrequency * _emisock.config.heartbeatsBeforeConnectionWarning;
    }
    
    void connectionTimeoutCallback() {
        forceClose(EMI_REASON_CONNECTION_TIMED_OUT);
    }
    void connectionWarningCallback(EmiTimeInterval alreadyWaitedTime) {
        EmiTimeInterval connectionTimeout = _emisock.config.connectionTimeout;
        
        _issuedConnectionWarning = true;
        _delegate.scheduleConnectionTimeout(connectionTimeout - alreadyWaitedTime);
        
        _delegate.emiConnLost();
    }
    void resetConnectionTimeout() {
        EmiTimeInterval warningTimeout = timeBeforeConnectionWarning();
        EmiTimeInterval connectionTimeout = _emisock.config.connectionTimeout;
        
        if (warningTimeout < connectionTimeout) {
            _delegate.scheduleConnectionWarning(warningTimeout);
        }
        else {
            _delegate.scheduleConnectionTimeout(connectionTimeout);
        }
        
        if (_issuedConnectionWarning) {
            _issuedConnectionWarning = false;
            _delegate.emiConnRegained();
        }
    }
    
    void tickTimeoutCallback(EmiTimeInterval now) {
        if (flush(now)) {
            resetHeartbeatTimeout();
        }
    }
    void ensureTickTimeout() {
        _delegate.ensureTickTimeout(1/_emisock.config.tickFrequency);
    }
    
    void heartbeatTimeoutCallback(EmiTimeInterval now) {
        if (_initiator) {
            // If we have received data since the last heartbeat, we don't need to ask for a heartbeat reply
            _sendQueue.sendHeartbeat(_receivedDataSinceLastHeartbeat, now);
        }
        else {
            _sendQueue.enqueueHeartbeat();
            ensureTickTimeout();
        }
        resetHeartbeatTimeout();
    }
    void resetHeartbeatTimeout() {
        _receivedDataSinceLastHeartbeat = false;
        if (!isOpening()) {
            // Don't send heartbeats until we've got a response from the remote host
            _delegate.scheduleHeartbeatTimeout(1/_emisock.config.heartbeatFrequency);
        }
    }
    
    void rtoTimeoutCallback(EmiTimeInterval now, EmiTimeInterval rto) {
        _senderBuffer.eachCurrentMessage(now, rto, ^(EmiMessage<SockDelegate> *msg) {
            // Reliable is set to NO, because if the message is reliable, it is already
            // in the sender buffer and shouldn't be reinserted anyway
            Error err;
            if (!enqueueMessage(now, msg, /*reliable:*/false, err)) {
                // This can't happen because the reliable parameter was false
                SockDelegate::panic();
            }
        });
        
        _time.onRtoTimeout();
        
        updateRtoTimeout();
    }
    void updateRtoTimeout() {
        if (!_senderBuffer.empty()) {
            _delegate.ensureRtoTimeout(_time.getRto());
        }
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
    
    // Returns the time relative to when the connection was initiated
    EmiTimeInterval getCurrentTime(EmiTimeInterval now) {
        return _time.getCurrentTime(now);
    }
    EmiTimestamp largestReceivedTime() const {
        return _time.getLargestReceivedTime();
    }
    bool hasReceivedTime() const {
        return _time.hasReceivedTime();
    }
    EmiTimeInterval gotLargestReceivedTimeAt() const {
        return _time.gotLargestReceivedTimeAt();
    }
    
    void gotPacket() {
        resetConnectionTimeout();
    }
    void gotHeartbeat(bool wasResponse) {
        resetConnectionTimeout();
        if (!wasResponse) {
            _sendQueue.enqueueHeartbeat();
            ensureTickTimeout();
        }
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
        
        if (_senderBuffer.empty()) {
            _delegate.invalidateRtoTimeout();
        }
    }
    
    /// Delegates to EmiReceiverBuffer
    void bufferMessage(const EmiMessageHeader& header, const TemporaryData& buf, size_t offset) {
        _receiverBuffer.bufferMessage(header, buf, offset);
    }
    void flushBuffer(EmiChannelQualifier channelQualifier, EmiSequenceNumber sequenceNumber) {
        _receiverBuffer.flushBuffer(channelQualifier, sequenceNumber);
    }
    
    // Returns false if the sender buffer didn't have space for the message.
    // Failing only happens for reliable mesages.
    bool enqueueMessage(EmiTimeInterval now, EmiMessage<SockDelegate> *msg, bool reliable, Error& err) {
        if (reliable) {
            if (!_senderBuffer.registerReliableMessage(msg, err)) {
                return false;
            }
            updateRtoTimeout();
        }
        
        _sendQueue.enqueueMessage(msg, now);
        ensureTickTimeout();
        
        return true;
    }
    
    // The first time this methods is called, it opens the EmiConnection and returns true.
    // Subsequent times it just resends the init message and returns false.
    bool opened(EmiTimeInterval now, EmiSequenceNumber otherHostInitialSequenceNumber) {
        if (_conn) {
            Error err;
            if (!_conn->resendInitMessage(now, err)) {
                // This should not happen, because resendInitMessage can only
                // fail when it attempts to send a SYN message, but we're
                // sending a SYN-RST message here.
                SockDelegate::panic();
            }
            return false;
        }
        else {
            _conn = new ELC(this, now, otherHostInitialSequenceNumber);
            return true;
        }
    }
    bool open(EmiTimeInterval now, const ConnectionOpenedCallbackCookie& cookie) {
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
    
    // Delegates to EmiLogicalConnection
    void gotRst() {
        if (_conn) {
            _conn->gotRst();
        }
    }
    // Delegates to EmiLogicalConnection
    void gotSynRstAck() {
        if (_conn) {
            _conn->gotSynRstAck();
        }
    }
    // Delegates to EmiLogicalConnection
    bool gotSynRst(EmiSequenceNumber otherHostInitialSequenceNumber) {
        return _conn && _conn->gotSynRst(otherHostInitialSequenceNumber);
    }
    // Delegates to EmiLogicalConnection
    bool gotMessage(EmiMessageHeader *header, const TemporaryData& data, size_t offset, bool dontFlush) {
        if (!_conn) {
            return false;
        }
        else {
            _receivedDataSinceLastHeartbeat = true;
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
            err = SockDelegate::makeError("com.emilir.eminet.closed", 0);
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
        return _sendQueue.flush(now);
    }
    
    // Delegates to EmiLogicalConnection
    //
    // This method assumes ownership over the data parameter, and will release it
    // with SockDelegate::releaseData when it's done with it. The buffer must not
    // be modified or released until after SockDelegate::releasePersistentData has
    // been called on it.
    bool send(EmiTimeInterval now, const PersistentData& data, EmiChannelQualifier channelQualifier, EmiPriority priority, Error& err) {
        if (!_conn || _conn->isClosing()) {
            err = SockDelegate::makeError("com.emilir.eminet.closed", 0);
            SockDelegate::releasePersistentData(data);
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
    bool isInitiator() const {
        return _initiator;
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
};

#endif
