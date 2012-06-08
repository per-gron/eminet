//
//  EmiConn.h
//  roshambo
//
//  Created by Per Eckerdal on 2012-04-10.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#ifndef emilir_EmiConn_h
#define emilir_EmiConn_h

#include "EmiTypes.h"
#include "EmiSenderBuffer.h"
#include "EmiReceiverBuffer.h"
#include "EmiSendQueue.h"
#include "EmiLogicalConnection.h"
#include "EmiMessage.h"
#include "EmiCongestionControl.h"
#include "EmiP2PData.h"
#include "EmiConnTimers.h"
#include "EmiConnTime.h"
#include "EmiConnParams.h"
#include "EmiUdpSocket.h"
#include "EmiMessageHandler.h"

class EmiPacketHeader;
class EmiMessageHeader;

template<class SockDelegate, class ConnDelegate>
class EmiConn {
    typedef typename SockDelegate::Binding   Binding;
    typedef typename Binding::Error          Error;
    typedef typename Binding::PersistentData PersistentData;
    typedef typename Binding::TemporaryData  TemporaryData;
    
    typedef typename SockDelegate::ConnectionOpenedCallbackCookie ConnectionOpenedCallbackCookie;
    
    typedef EmiUdpSocket<Binding>                        EUS;
    typedef EmiMessage<Binding>                          EM;
    typedef EmiReceiverBuffer<SockDelegate, EmiConn>     ERB;
    typedef EmiSendQueue<SockDelegate, ConnDelegate>     ESQ;
    typedef EmiConnTimers<Binding, EmiConn>              ECT;
    typedef EmiMessageHandler<EmiConn, EmiConn, Binding> EMH;
    typedef EmiLogicalConnection<SockDelegate, ConnDelegate, ERB> ELC;
    
    // For makeServerConnection
    friend class EmiMessageHandler<EmiConn, EmiConn, Binding>;
    
    uint16_t               _inboundPort;
    // This gets set when we receive the first packet from the other host.
    // Before that, it is an address with port 0
    sockaddr_storage       _localAddress;
    // Is not the same as _remoteAddress for established P2P connections
    const sockaddr_storage _originalRemoteAddress;
    sockaddr_storage       _remoteAddress;
    
    EMH  _messageHandler;
    EUS *_socket;
    
    EmiP2PData        _p2p;
    EmiConnectionType _type;
    
    ELC *_conn;
    EmiSenderBuffer<Binding> _senderBuffer;
    ERB _receiverBuffer;
    ESQ _sendQueue;
    
    EmiCongestionControl _congestionControl;
    
    ECT _timers;
    typename Binding::Timer *_forceCloseTimer;
    
    ConnDelegate _delegate;
        
private:
    // Private copy constructor and assignment operator
    inline EmiConn(const EmiConn& other);
    inline EmiConn& operator=(const EmiConn& other);
    
    void deleteELC(ELC *elc) {
        if (_socket) {
            if (EMI_CONNECTION_TYPE_SERVER != _type) {
                // For server connections, EmiSock owns the socket
                delete _socket;
            }
            _socket = NULL;
        }
        
        // This if ensures that ConnDelegate::invalidate is only invoked once.
        if (elc) {
            _delegate.invalidate();
            delete elc;
        }
    }
    
    bool enqueueCloseMessageIfEmptySenderBuffer(EmiTimeInterval now, Error& err) {
        if (!_senderBuffer.empty()) {
            // We did not fail, so return true
            return true;
        }
        
        return _conn->enqueueCloseMessage(now, err);
    }
    
    static void forceCloseTimeoutCallback(EmiTimeInterval now, typename Binding::Timer *timer, void *data) {
        EmiConn *conn = (EmiConn *)data;
        Binding::freeTimer(timer);
        conn->forceClose(EMI_REASON_THIS_HOST_CLOSED);
    }
    
    inline bool shouldArtificiallyDropPacket() const {
        if (0 == config.fabricatedPacketDropRate) return false;
        
        return ((float)arc4random() / EmiNetUtil::ARC4RANDOM_MAX) < config.fabricatedPacketDropRate;
    }
    
    static void onMessage(EUS *socket,
                          void *userData,
                          EmiTimeInterval now,
                          const sockaddr_storage& inboundAddress,
                          const sockaddr_storage& remoteAddress,
                          const TemporaryData& data,
                          size_t offset,
                          size_t len) {
        EmiConn *conn((EmiConn *)userData);
        
        if (conn->shouldArtificiallyDropPacket()) {
            return;
        }
        
        bool unexpectedRemoteHost = (0 != EmiAddressCmp::compare(conn->_remoteAddress, remoteAddress));
        conn->_messageHandler.onMessage(/*acceptConnections:*/false,
                                        now, socket,
                                        unexpectedRemoteHost, conn,
                                        inboundAddress, remoteAddress,
                                        data, offset, len);
    }
    
    // Invoked by _messageHandler
    EmiConn *makeServerConnection(const sockaddr_storage& remoteAddress, uint16_t inboundPort) {
        // This should never happen, because we never pass acceptConnections=true to onMessage
        ASSERT(false && "Internal error");
    }
    
public:
    const EmiSockConfig config;
    
    // Invoked by EmiReceiverBuffer
    inline void gotReceiverBufferMessage(EmiTimeInterval now, typename ERB::Entry *entry) {
        if (!_conn) return;
        
        // gotMessage should only return false if the message arrived out of order or
        // some other similar error occured, but that should not happen because this
        // callback should only be called by the receiver buffer for messages that are
        // exactly in order.
        ASSERT(_conn->gotMessage(now, entry->header,
                                 Binding::castToTemporary(entry->data), /*offset:*/0,
                                 /*dontFlush:*/true));
    }
    
    EmiConn(const ConnDelegate& delegate,
            const EmiSockConfig& config_,
            const EmiConnParams<Binding>& params) :
    _inboundPort(params.inboundPort),
    _originalRemoteAddress(params.address),
    _remoteAddress(params.address),
    _conn(NULL),
    _delegate(delegate),
    _messageHandler(*this),
    _socket(params.socket),
    _type(params.type),
    _p2p(params.p2p),
    _senderBuffer(config_.senderBufferSize),
    _receiverBuffer(config_.receiverBufferSize, *this),
    _sendQueue(*this, config_.mtu),
    _congestionControl(),
    _timers(config_, *this),
    _forceCloseTimer(NULL),
    config(config_) {
        EmiNetUtil::anyAddr(0, AF_INET, &_localAddress);
    }
    
    virtual ~EmiConn() {
        if (_forceCloseTimer) {
            Binding::freeTimer(_forceCloseTimer);
        }
        
        deleteELC(_conn);
    }
    
    // Note that this method might deallocate the connection
    // object! It must not be called from within code that
    // subsequently uses the object.
    void forceClose(EmiDisconnectReason reason) {
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
    
    // Returns false if the packet came to an invalid inboundAddress
    bool gotPacket(EmiTimeInterval now,
                   const sockaddr_storage& inboundAddress,
                   const EmiPacketHeader& packetHeader,
                   size_t packetLength) {
        if (EmiNetUtil::isAnyAddr(_localAddress)) {
            _localAddress = inboundAddress;
        }
        else if (0 != EmiAddressCmp::compare(_localAddress, inboundAddress)) {
            return false;
        }
        
        _timers.gotPacket(packetHeader, now);
        _congestionControl.gotPacket(now, _timers.getTime().getRtt(),
                                     _sendQueue.lastSentSequenceNumber(),
                                     packetHeader, packetLength);
        
        if (packetHeader.flags & EMI_RTT_REQUEST_PACKET_FLAG) {
            _sendQueue.enqueueRttResponse(packetHeader.sequenceNumber, now);
            _timers.ensureTickTimeout();
        }
        
        return true;
    }
    
    // Delegates to EmiSendQueue
    void enqueueAck(EmiChannelQualifier channelQualifier, EmiSequenceNumber sequenceNumber) {
        if (_sendQueue.enqueueAck(channelQualifier, sequenceNumber)) {
            _timers.ensureTickTimeout();
        }
    }
    
    // Delegates to EmiSenderBuffer
    void deregisterReliableMessages(EmiTimeInterval now, int32_t channelQualifier, EmiSequenceNumber sequenceNumber) {
        _senderBuffer.deregisterReliableMessages(channelQualifier, sequenceNumber);
        
        // This will clear the rto timeout if the sender buffer is empty
        _timers.updateRtoTimeout();
        
        if (_conn->isClosing()) {
            Error err;
            if (!enqueueCloseMessageIfEmptySenderBuffer(now, err)) {
                // We failed to enqueue the close connection message.
                // I can't think of any reasonable thing to do here but
                // to force close.
                forceClose();
            }
        }
    }
    
    // Returns false if the sender buffer didn't have space for the message.
    // Failing only happens for reliable mesages.
    bool enqueueMessage(EmiTimeInterval now, EmiMessage<Binding> *msg, bool reliable, Error& err) {
        if (reliable) {
            if (!_senderBuffer.registerReliableMessage(msg, err, now)) {
                return false;
            }
            _timers.updateRtoTimeout();
        }
        
        _timers.ensureTickTimeout();
        
        Error enqueueError;
        if (!_sendQueue.enqueueMessage(msg, _congestionControl, _timers.getTime(), now, enqueueError)) {
            // This failing is not a catastrophic failure; if the message is not reliable
            // the system should allow for it to be lost anyway, and if the message is
            // reliable, it will be re-inserted into the send queue on the applicable RTO
            // timeout.
            
            // For now, we'll do nothing here. In the future, it might be a good idea to
            // improve this by telling the client of EmiNet that this happened.
        }
        
        return true;
    }
    
    // The first time this methods is called, it opens the EmiConnection and returns true.
    // Subsequent times it just resends the init message and returns false.
    bool opened(const sockaddr_storage& inboundAddress, EmiTimeInterval now, EmiSequenceNumber otherHostInitialSequenceNumber) {
        ASSERT(EMI_CONNECTION_TYPE_SERVER == _type);
        
        _localAddress = inboundAddress;
        
        if (_conn) {
            // sendInitMessage should not fail, because it can only
            // fail when it attempts to send a SYN message, but we're
            // sending a SYN-RST message here.
            Error err;
            ASSERT(_conn->sendInitMessage(now, err));
            
            return false;
        }
        else {
            _conn = new ELC(this, _receiverBuffer, now, otherHostInitialSequenceNumber);
            
            // This instructs the RTO timer that the connection is now opened.
            connectionOpened();
            
            return true;
        }
    }
    bool open(EmiTimeInterval now,
              const sockaddr_storage& bindAddress,
              const ConnectionOpenedCallbackCookie& cookie,
              Error& err) {
        ASSERT(EMI_CONNECTION_TYPE_CLIENT == _type ||
               EMI_CONNECTION_TYPE_P2P    == _type);
                
        _socket = EUS::open(_delegate.getSocketCookie(), onMessage, this, bindAddress, err);
        
        if (!_socket) {
            return false;
        }
        
        _inboundPort = _socket->getLocalPort();
        
        if (!_inboundPort) {
            delete _socket;
            _socket = NULL;
            return false;
        }
        
        if (_conn) {
            // We don't need to explicitly resend the init message here;
            // SYN connection init messages like this are reliable messages
            // and will be resent automatically when appropriate.
        }
        else {
            // This will send the SYN message
            _conn = new ELC(this, _receiverBuffer, now, cookie);
        }
        
        return true;
    }
    
    // This instructs the RTO timer that the connection is now opened.
    inline void connectionOpened() {
        _timers.connectionOpened();
    }
    
    inline void resetHeartbeatTimeout() {
        _timers.resetHeartbeatTimeout();
    }
    
    // Methods that EmiConnTimers invoke
    
    // Warning: This method might deallocate the object
    inline void connectionTimeout() {
        forceClose(EMI_REASON_CONNECTION_TIMED_OUT);
    }
    inline void connectionLost() {
        _delegate.emiConnLost();
    }
    inline void connectionRegained() {
        _delegate.emiConnLost();
    }
    void eachCurrentMessageIteration(EmiTimeInterval now, EmiMessage<Binding> *msg) {
        Error err;
        // Reliable is set to false, because if the message is reliable, it is
        // already in the sender buffer and shouldn't be reinserted anyway
        
        // enqueueMessage can't fail because the reliable parameter is false
        ASSERT(enqueueMessage(now, msg, /*reliable:*/false, err));
    }
    void rtoTimeout(EmiTimeInterval now, EmiTimeInterval rtoWhenRtoTimerWasScheduled) {
        _congestionControl.onRto();
        
        _senderBuffer.eachCurrentMessage(now, rtoWhenRtoTimerWasScheduled, *this);
    }
    inline void enqueueHeartbeat() {
        _sendQueue.enqueueHeartbeat();
    }
    inline void enqueueNak(EmiPacketSequenceNumber nak) {
        _sendQueue.enqueueNak(nak);
    }
    inline bool senderBufferIsEmpty() const {
        return _senderBuffer.empty();
    }
    
    // Methods that delegate to EmiLogicalConnetion
#define X(msg)                  \
    inline void got##msg() {    \
        if (_conn) {            \
            _conn->got##msg();  \
        }                       \
    }
    // Warning: This method might deallocate the object
    X(Rst);
    // Warning: This method might deallocate the object
    X(SynRstAck);
#undef X
    inline void gotPrx(EmiTimeInterval now) {
        if (_conn) {
            _conn->gotPrx(now);
        }
    }
    inline void gotPrxRstSynAck(EmiTimeInterval now, const uint8_t *data, size_t len) {
        if (_conn) {
            _conn->gotPrxRstSynAck(now, data, len);
        }
    }
    inline void gotPrxSyn(const sockaddr_storage& remoteAddr,
                          const uint8_t *data,
                          size_t len) {
        if (_conn) {
            _conn->gotPrxSyn(remoteAddr, data, len);
        }
    }
    inline void gotPrxSynAck(const sockaddr_storage& remoteAddr,
                          const uint8_t *data,
                          size_t len) {
        if (_conn) {
            _conn->gotPrxSynAck(remoteAddr, data, len, _timers, &_remoteAddress, &_timers.getTime());
        }
    }
    inline void gotPrxRstAck(const sockaddr_storage& remoteAddr) {
        if (_conn) {
            _conn->gotPrxRstAck(remoteAddr);
        }
    }
    // Delegates to EmiLogicalConnection
    bool gotSynRst(EmiTimeInterval now,
                   const sockaddr_storage& inboundAddr,
                   EmiSequenceNumber otherHostInitialSequenceNumber) {
        return _conn && _conn->gotSynRst(now, inboundAddr, otherHostInitialSequenceNumber);
    }
    // Delegates to EmiLogicalConnection
    bool gotMessage(EmiTimeInterval now,
                    const EmiMessageHeader& header,
                    const TemporaryData& data, size_t offset,
                    bool dontFlush) {
        if (!_conn) {
            return false;
        }
        else {
            return _conn->gotMessage(now, header, data, offset, dontFlush);
        }
    }
    
    
    // Invoked by EmiLogicalConnection
    void emitDisconnect(EmiDisconnectReason reason) {
        _delegate.emiConnDisconnect(reason);
    }
    void emitMessage(EmiChannelQualifier channelQualifier, const TemporaryData& data, size_t offset, size_t size) {
        _delegate.emiConnMessage(channelQualifier, data, offset, size);
    }
    void emitNatPunchthroughFinished(bool success) {
        _delegate.emiNatPunchthroughFinished(success);
    }
    
    bool close(EmiTimeInterval now, Error& err) {
        if (_conn) {
            if (!_conn->initiateCloseProcess(now, err)) {
                return false;
            }
            
            if (!enqueueCloseMessageIfEmptySenderBuffer(now, err)) {
                return false;
            }
            
            return true;
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
        // To ensure that we don't deallocate this object immediately (which
        // could have happened if we invoked forceClose(EmiDisconnectReason)
        // immediately, we schedule a timer that will invoke forceClose later
        // on. This guarantees that we don't deallocate this object while
        // there are references to it left on the stack.
        if (!_forceCloseTimer) {
            _forceCloseTimer = Binding::makeTimer();
            Binding::scheduleTimer(_forceCloseTimer, forceCloseTimeoutCallback,
                                   this, /*time:*/0, /*repeating:*/false);
        }
    }
    
    // Delegates to EmiSendQueue
    // Returns true if something has been sent since the last tick
    bool tick(EmiTimeInterval now) {
        return _sendQueue.tick(_congestionControl, _timers.getTime(), now);
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
    
    inline ConnDelegate& getDelegate() {
        return _delegate;
    }
    
    inline const ConnDelegate& getDelegate() const {
        return _delegate;
    }
    inline void setDelegate(const ConnDelegate& delegate) {
        _delegate = delegate;
    }
    
    inline uint16_t getInboundPort() const {
        return _inboundPort;
    }
    
    inline const sockaddr_storage& getLocalAddress() const {
        return _localAddress;
    }
    
    inline const sockaddr_storage& getOriginalRemoteAddress() const {
        return _originalRemoteAddress;
    }
    
    inline const sockaddr_storage& getRemoteAddress() const {
        return _remoteAddress;
    }
    
    inline bool issuedConnectionWarning() const {
        return _timers.issuedConnectionWarning();
    }
    inline const EUS *getSocket() const {
        return _socket;
    }
    inline EUS *getSocket() {
        return _socket;
    }
    inline EmiConnectionType getType() const {
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
        sendDatagram(getRemoteAddress(), data, size);
    }
    
    // Invoked by EmiNatPunchthrough (via EmiLogicalConnection);
    // EmiNatPunchthrough needs the ability to send packets to
    // other addresses than the current _remoteAddress
    void sendDatagram(const sockaddr_storage& address, const uint8_t *data, size_t size) {
        _timers.sentPacket();
        
        if (shouldArtificiallyDropPacket()) {
            return;
        }
        
        if (_socket) {
            _socket->sendData(_localAddress, address, data, size);
        }
    }
    
    inline const EmiP2PData& getP2PData() const {
        return _p2p;
    }
    
    inline EmiP2PState getP2PState() const {
        if (!_conn) {
            return EMI_P2P_STATE_NOT_ESTABLISHING;
        }
        else {
            return _conn->getP2PState();
        }
    }
};

#endif
