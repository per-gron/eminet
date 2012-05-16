//
//  EmiLogicalConnection.h
//  roshambo
//
//  Created by Per Eckerdal on 2012-02-16.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#ifndef emilir_EmiLogicalConnection_h
#define emilir_EmiLogicalConnection_h

#include "EmiMessage.h"
#include "EmiNetUtil.h"
#include "EmiNatPunchthrough.h"

#include <map>

template<class Data>
class EmiMessage;
template<class SockDelegate, class ConnDelegate>
class EmiConn;

typedef std::map<EmiChannelQualifier, EmiSequenceNumber> EmiLogicalConnectionMemo;

template<class SockDelegate, class ConnDelegate, class ReceiverBuffer>
class EmiLogicalConnection {
    typedef typename SockDelegate::Binding   Binding;
    typedef typename Binding::Error          Error;
    typedef typename Binding::PersistentData PersistentData;
    typedef typename Binding::TemporaryData  TemporaryData;
    typedef typename SockDelegate::ConnectionOpenedCallbackCookie ConnectionOpenedCallbackCookie;
    typedef EmiMessage<Binding>                 EM;
    typedef EmiConn<SockDelegate, ConnDelegate> EC;
    typedef EmiNatPunchthrough<Binding, EmiLogicalConnection> ENP;
    
    friend class EmiNatPunchthrough<Binding, EmiLogicalConnection>;
    
    ReceiverBuffer &_receiverBuffer;
    
    bool _closing;
    EC *_conn;
    EmiSequenceNumber _initialSequenceNumber;
    EmiSequenceNumber _otherHostInitialSequenceNumber;
    
    ConnectionOpenedCallbackCookie _connectionOpenedCallbackCookie;
    bool _sendingSyn;
    
    EmiLogicalConnectionMemo _sequenceMemo;
    EmiLogicalConnectionMemo _otherHostSequenceMemo;
    EmiLogicalConnectionMemo _reliableSequencedBuffer;
    
    // This contains the sequence number of these messages before
    // they have been acknowledged (then this var is set back to
    // -1): SYN, PRX-ACK, PRX-SYN.
    //
    // Because connections only ever wait for one of these messages
    // we get away with only using one instance variable for all of
    // them.
    int32_t _reliableHandshakeMsgSn; // -1 when it has no value
    
    ENP *_natPunchthrough; // This is NULL except in the NAT punchthrough phase of P2P connections
    
private:
    // Private copy constructor and assignment operator
    inline EmiLogicalConnection(const EmiLogicalConnection& other);
    inline EmiLogicalConnection& operator=(const EmiLogicalConnection& other);
    
    void invokeSynRstCallback(bool error, EmiDisconnectReason reason){
        if (_sendingSyn) {
            _sendingSyn = false;
            
            // For initiating connections (this connection is one, otherwise
            // this method would not get called), the heartbeat timer doesn't
            // get set immediately, to avoid sending heartbeats to hosts that
            // have not yet replied and might not even exist.
            //
            // Because of this, we need to set the heartbeat timer here.
            _conn->resetHeartbeatTimeout();
            
            SockDelegate::connectionOpened(_connectionOpenedCallbackCookie, error, reason, *_conn);
        }
    }
    void releaseReliableHandshakeMsg(EmiTimeInterval now) {
        if (-1 != _reliableHandshakeMsgSn) {
            _conn->deregisterReliableMessages(now, -1, _reliableHandshakeMsgSn);
            _reliableHandshakeMsgSn = -1;
        }
    }
    
    static EmiSequenceNumber generateSequenceNumber() {
        // Right shifting a bit because apparently the low bits are usually not that random
        return arc4random() >> 5;
    }
    
    int32_t sequenceNumberDifference(const EmiMessageHeader& header, bool updateExpectedSequenceNumber) {
        EmiLogicalConnectionMemo::iterator cur = _otherHostSequenceMemo.find(header.channelQualifier);
        EmiLogicalConnectionMemo::iterator end = _otherHostSequenceMemo.end();
        
        EmiSequenceNumber expectedSequenceNumber = (end == cur ? _otherHostInitialSequenceNumber : (*cur).second);
        
        if (updateExpectedSequenceNumber) {
            _otherHostSequenceMemo[header.channelQualifier] = EmiNetUtil::cyclicMax16(expectedSequenceNumber, header.sequenceNumber+1);
        }
        
        return EmiNetUtil::cyclicDifference16Signed(expectedSequenceNumber, header.sequenceNumber);
    }
    
    EmiSequenceNumber sequenceMemoForChannelQualifier(EmiChannelQualifier cq) {
        EmiLogicalConnectionMemo::iterator cur = _sequenceMemo.find(cq);
        return (_sequenceMemo.end() == cur ? _initialSequenceNumber : (*cur).second);
    }
    
    // Helper for the constructors
    void commonInit() {
        _initialSequenceNumber = EmiLogicalConnection::generateSequenceNumber();
        
        _reliableHandshakeMsgSn = -1;
        
        _natPunchthrough = NULL;
    }
    
    // Invoked by EmiNatPunchthrough
    inline void sendNatPunchthroughPacket(const sockaddr_storage& addr, const uint8_t *buf, size_t bufSize) {
        if (_conn) {
            _conn->sendDatagram(addr, buf, bufSize);
        }
    }
    
    // Invoked by EmiNatPunchthrough
    inline void natPunchthroughFinished(bool success) {
        delete _natPunchthrough;
        _natPunchthrough = NULL;
        
        if (_conn) {
            _conn->emitNatPunchthroughFinished(success);
        }
    }
    
public:
    
    EmiLogicalConnection(EC *connection, 
                         ReceiverBuffer& receiverBuffer,
                         EmiTimeInterval now,
                         EmiSequenceNumber sequenceNumber) :
    _receiverBuffer(receiverBuffer),
    _closing(false), _conn(connection),
    _otherHostInitialSequenceNumber(sequenceNumber),
    _sendingSyn(false), _connectionOpenedCallbackCookie() {
        ASSERT(EMI_CONNECTION_TYPE_SERVER == _conn->getType());
        
        commonInit();
        
        // sendInitMessage should not fail, because it only fails when this
        // connection is not EMI_CONNECTION_TYPE_SERVER
        Error err;
        ASSERT(sendInitMessage(now, err));
    }
    EmiLogicalConnection(EC *connection,
                         ReceiverBuffer& receiverBuffer,
                         EmiTimeInterval now,
                         const ConnectionOpenedCallbackCookie& connectionOpenedCallbackCookie) :
    _receiverBuffer(receiverBuffer),
    _closing(false), _conn(connection),
    _otherHostInitialSequenceNumber(0),
    _sendingSyn(true), _connectionOpenedCallbackCookie(connectionOpenedCallbackCookie) {
        ASSERT(EMI_CONNECTION_TYPE_SERVER != _conn->getType());
        
        commonInit();
        
        // sendInitMessage really ought not to fail; it only fails when the
        // sender buffer is full, but this init message should be the first
        // message on the whole connection.
        Error err;
        ASSERT(sendInitMessage(now, err));
    }
    virtual ~EmiLogicalConnection() {
        // Just to be sure, since these ivars are __unsafe_unretained
        _conn = NULL;
        
        if (_natPunchthrough) {
            delete _natPunchthrough;
        }
    }
    
    // This method only fails when sending a SYN message, that is,
    // _synRstCallback is set, which happens when this connection is
    // not EMI_CONNECTION_TYPE_SERVER.
    bool sendInitMessage(EmiTimeInterval now, Error& err) {
        bool error = false;
        
        releaseReliableHandshakeMsg(now);
        
        EM *msg = _sendingSyn ?
            EM::makeSynMessage(_initialSequenceNumber,
                               _conn->getP2PData().p2pCookie,
                               _conn->getP2PData().p2pCookieLength) :
            EM::makeSynRstMessage(_initialSequenceNumber);
        
        if (_sendingSyn) {
            _reliableHandshakeMsgSn = msg->sequenceNumber;
        }
        
        if (!_conn->enqueueMessage(now, msg, /*reliable:*/_sendingSyn, err)) {
            error = true;
        }
        
        msg->release();
        
        return !error;
    }
    
    // Invoked by EmiConnection
    void wasClosed(EmiDisconnectReason reason) {
        invokeSynRstCallback(true, reason);
        
        _conn->emitDisconnect(reason);
        _conn = NULL;
        _closing = false;
    }
    
    void gotPrx(EmiTimeInterval now) {
        if (EMI_CONNECTION_TYPE_P2P != _conn->getType()) {
            // This type of message should only be sent in a P2P connection.
            // This isn't one, so we just ignore it.
            return;
        }
        
        // This is a SYN with cookie response. The P2P mediator has received
        // our SYN message but has not yet received a SYN from the other
        // peer.
        releaseReliableHandshakeMsg(now);
    }
    
    // Warning: This method might deallocate the object
    void gotRst() {
        _conn->forceClose(EMI_REASON_OTHER_HOST_CLOSED);
    }
    
    // Warning: This method might deallocate the object
    void gotSynRstAck() {
        // Technically, the other host could just send a
        // SYN-RST-ACK (confirm connection close) message
        // without this host even attempting to close the
        // connection. In that case, in order to not send
        // a EMI_REASON_THIS_HOST_CLOSED disconnect reason
        // to the code that uses EmiNet, which could
        // potentially be really confusing, tell the delegate
        // that the other host initiated the close (which,
        // even though it did in an incorrect way, is closer
        // to the truth than saying that this host closed)
        _conn->forceClose(isClosing() ?
                          EMI_REASON_THIS_HOST_CLOSED :
                          EMI_REASON_OTHER_HOST_CLOSED);
    }
    
    void gotPrxRstSynAck(EmiTimeInterval now, const uint8_t *data, size_t len) {
        
        /// Parse the incoming data
        
        const int family = _conn->getLocalAddress().ss_family;
        const size_t ipLen = EmiNetUtil::ipLength(_conn->getLocalAddress());
        static const size_t portLen = sizeof(uint16_t);
        const size_t endpointPairLen = 2*(ipLen+portLen);
        const size_t dataLen = 2*endpointPairLen;
        
        if (len != dataLen) {
            return;
        }
        
        sockaddr_storage addrs[2];
        
        const uint8_t *dataPtr = data+endpointPairLen;
        for (int i=0; i<2; i++) {
            EmiNetUtil::makeAddress(family,
                                    dataPtr, ipLen,
                                    *((uint16_t *)(dataPtr+ipLen)),
                                    &addrs[i]);
            
            dataPtr += ipLen+portLen;
        }
        
        
        /// Release the reliable PRX-ACK handshake message
        
        releaseReliableHandshakeMsg(now);
        
        
        /// Initiate NAT punch through
        
        if (!_natPunchthrough) {
            _natPunchthrough = new ENP(_conn->getEmiSock().config.connectionTimeout,
                                       *this,
                                       _initialSequenceNumber,
                                       _conn->getRemoteAddress(),
                                       _conn->getP2PData(),
                                       /*myEndpointPair:*/data, endpointPairLen,
                                       /*peerEndpointPair:*/data+endpointPairLen, endpointPairLen,
                                       /*peerInnerAddr:*/addrs[0],
                                       /*peerOuterAddr:*/addrs[1]);
        }
    }
    
    inline void gotPrxSyn(const sockaddr_storage& remoteAddr,
                          const uint8_t *data,
                          size_t len) {
        if (_natPunchthrough) {
            _natPunchthrough->gotPrxSyn(remoteAddr, data, len);
        }
    }
    
    template<class ConnRtoTimer>
    inline void gotPrxSynAck(const sockaddr_storage& remoteAddr,
                             const uint8_t *data,
                             size_t len,
                             ConnRtoTimer& connRtoTimer,
                             sockaddr_storage *connsRemoteAddr,
                             EmiConnTime *connsTime) {
        if (_natPunchthrough) {
            _natPunchthrough->gotPrxSynAck(remoteAddr, data, len,
                                           connRtoTimer, connsRemoteAddr, connsTime);
        }
    }
    
    inline void gotPrxRstAck(const sockaddr_storage& remoteAddr) {
        if (_natPunchthrough) {
            _natPunchthrough->gotPrxRstAck(remoteAddr);
        }
    }
    
    // Returns false if the connection is not in the opening state (that's an error)
    bool gotSynRst(EmiTimeInterval now,
                   const sockaddr_storage& inboundAddr,
                   EmiSequenceNumber otherHostInitialSequenceNumber) {
        if (!isOpening()) {
            return false;
        }
        
        releaseReliableHandshakeMsg(now);
        _otherHostInitialSequenceNumber = otherHostInitialSequenceNumber;
        
        invokeSynRstCallback(false, EMI_REASON_NO_ERROR);
        
        if (_conn && EMI_CONNECTION_TYPE_P2P == _conn->getType()) {
            // Send reliable PRX-SYN message
            
            EM *msg = EM::makePrxAckMessage(_initialSequenceNumber, inboundAddr);
            
            Error err;
            ASSERT(_conn->enqueueMessage(now, msg, /*reliable:*/_sendingSyn, err));
            
            _reliableHandshakeMsgSn = msg->sequenceNumber;
            
            msg->release();
        }
        
        return true;
    }
    
    // Returns true if the packet was processed successfully, false otherwise.
#define EMI_GOT_INVALID_PACKET(err) do { /* NSLog(err); */ return false; } while (1)
    bool gotMessage(EmiTimeInterval now,
                    const EmiMessageHeader& header,
                    const TemporaryData &data, size_t offset,
                    bool dontFlush) {
        EmiChannelQualifier channelQualifier = header.channelQualifier;
        EmiChannelType channelType = EMI_CHANNEL_QUALIFIER_TYPE(channelQualifier);
        
        if (EMI_CHANNEL_TYPE_UNRELIABLE == channelType || EMI_CHANNEL_TYPE_UNRELIABLE_SEQUENCED == channelType) {
            if (header.flags & EMI_ACK_FLAG) EMI_GOT_INVALID_PACKET("Got unreliable message with ACK flag");
            if (header.flags & EMI_SACK_FLAG) EMI_GOT_INVALID_PACKET("Got unreliable message with SACK flag");
            
            if (EMI_CHANNEL_TYPE_UNRELIABLE_SEQUENCED == channelType &&
                0 < sequenceNumberDifference(header, true)) {
                // The packet arrived out of order; drop it
                return false;
            }
            
            _conn->emitMessage(channelQualifier, data, offset, header.length);
        }
        else if (EMI_CHANNEL_TYPE_RELIABLE_SEQUENCED == channelType) {
            if (header.flags & EMI_SACK_FLAG) EMI_GOT_INVALID_PACKET("SACK does not make sense on RELIABLE_SEQUENCED channels");
            
            if (-1 != header.sequenceNumber &&
                0 < sequenceNumberDifference(header, true)) {
                // The packet arrived out of order; drop it
                return false;
            }
            
            _conn->enqueueAck(channelQualifier, header.sequenceNumber);
            
            if (header.flags & EMI_ACK_FLAG) {
                EmiLogicalConnectionMemo::iterator cur = _reliableSequencedBuffer.find(channelQualifier);
                if (_reliableSequencedBuffer.end() != cur &&
                    (*cur).second == header.ack) {
                    _conn->deregisterReliableMessages(now, channelQualifier, header.ack);
                    _reliableSequencedBuffer.erase(channelQualifier);
                }
            }
            
            // A packet with zero length indicates that it is just an ACK packet
            if (0 != header.length) {
                size_t realOffset = offset + (header.flags & EMI_ACK_FLAG ? 2 : 0);
                _conn->emitMessage(channelQualifier, data, realOffset, header.length);
            }
        }
        else if (EMI_CHANNEL_TYPE_RELIABLE_ORDERED == channelType) {
            if (header.flags & EMI_SACK_FLAG) EMI_GOT_INVALID_PACKET("SACK is not implemented");
            
            bool hasSequenceNumber = -1 != header.sequenceNumber;
            int32_t seqDiff = 0;
            if (hasSequenceNumber) {
                seqDiff = sequenceNumberDifference(header, false);
            }
            
            if (hasSequenceNumber && seqDiff >= 0) {
                // Send an ACK only if this message has a sequence number
                //
                // Also, send an ACK only if the received message's sequence number
                // is what we were expecting or if if it was older than we expected.
                _conn->enqueueAck(channelQualifier, header.sequenceNumber);
            }
            
            if (header.flags & EMI_ACK_FLAG) {
                _conn->deregisterReliableMessages(now, channelQualifier, header.ack);
            }
            
            if (hasSequenceNumber && 0 != seqDiff) {
                if (seqDiff < 0) {
                    // This message is newer than what we were expecting; save it in the input buffer
                    _receiverBuffer.bufferMessage(header, data, offset, header.length);
                }
                
                return false;
            }
            
            if (hasSequenceNumber) {
                _otherHostSequenceMemo[channelQualifier] = header.sequenceNumber+1;
            }
            
            // A packet with zero length indicates that it is just an ACK packet
            if (0 != header.length) {
                _conn->emitMessage(channelQualifier, data, offset, header.length);
                
                // The connection might have been closed in the message event handler,
                // that's why we check that _conn is non-NULL.
                if (_conn && !dontFlush && hasSequenceNumber) {
                    _receiverBuffer.flushBuffer(now, channelQualifier, header.sequenceNumber);
                }
            }
        }
        else {
            EMI_GOT_INVALID_PACKET("Unknown channel type");
        }
        
        return true;
    }
#undef EMI_GOT_INVALID_PACKET
    
    bool initiateCloseProcess(EmiTimeInterval now, Error& err) {
        if (_closing || !_conn) {
            // We're already closing or closed; no need to initiate a new close process
            err = Binding::makeError("com.emilir.eminet.closed", 0);
            return false;
        }
        
        _closing = true;
        
        // If we're currently performing a handshake, cancel that.
        releaseReliableHandshakeMsg(now);
        
        if (isOpening()) {
            // If the connection is not yet open, invoke the connection
            // callback with an error code. This is necessary because
            // this class promises to invoke the synRstCallback.
            invokeSynRstCallback(true, EMI_REASON_THIS_HOST_CLOSED);
        }
        
        return true;
    }
    
    bool enqueueCloseMessage(EmiTimeInterval now, Error& err) {
        bool error(false);
        
        // Send an RST (connection close) message
        EM *msg = EM::makeRstMessage(_initialSequenceNumber);
        if (!_conn->enqueueMessage(now, msg, /*reliable:*/true, err)) {
            error = true;
        }
        msg->release();
        
        return !error;
    }
    
    // Returns false if the sender buffer was full and the message couldn't be sent
    bool send(const PersistentData& data, EmiTimeInterval now, EmiChannelQualifier channelQualifier, EmiPriority priority, Error& err) {
        bool error = false;
        
        // This has to be called before we increment _sequenceMemo[cq]
        EmiChannelQualifier prevSeqMemo = sequenceMemoForChannelQualifier(channelQualifier);
        
        EM *msg = EM::makeDataMessage(channelQualifier,
                                      sequenceMemoForChannelQualifier(channelQualifier),
                                      data,
                                      priority);
        _sequenceMemo[channelQualifier] = msg->sequenceNumber+1;
        
        EmiChannelType channelType = EMI_CHANNEL_QUALIFIER_TYPE(channelQualifier);
        
        bool reliable = (EMI_CHANNEL_TYPE_RELIABLE_SEQUENCED == channelType ||
                         EMI_CHANNEL_TYPE_RELIABLE_ORDERED == channelType);
        
        if (isClosed()) {
            err = Binding::makeError("com.emilir.eminet.closed", 0);
            error = true;
            goto cleanup;
        }
        
        if (0 == Binding::extractLength(data)) {
            err = Binding::makeError("com.emilir.eminet.emptymessage", 0);
            error = true;
            goto cleanup;
        }
        
        if (EMI_CHANNEL_TYPE_RELIABLE_SEQUENCED == channelType) {
            EmiLogicalConnectionMemo::iterator cur = _reliableSequencedBuffer.find(channelQualifier);
            if (_reliableSequencedBuffer.end() != cur) {
                _conn->deregisterReliableMessages(now, channelQualifier, prevSeqMemo);
            }
            _reliableSequencedBuffer[channelQualifier] = msg->sequenceNumber;
        }
        
        if (!_conn->enqueueMessage(now, msg, reliable, err)) {
            // _sequenceMemo[channelQualifier] has been bumped. Since the message wasn't sent: Undo that
            _sequenceMemo[channelQualifier] = prevSeqMemo;
            error = true;
            goto cleanup;
        }
        
    cleanup:
        msg->release();
        return !error;
    }
    
    bool isOpening() const {
        return _sendingSyn;
    }
    bool isClosing() const {
        // Strictly speaking, checking if the connection is closed
        // should not be necessary, we do it just to be sure.
        return !isClosed() && _closing;
    }
    bool isClosed() const {
        return !_conn;
    }
    
    EmiSequenceNumber getOtherHostInitialSequenceNumber() const {
        return _otherHostInitialSequenceNumber;
    }
};

#endif
