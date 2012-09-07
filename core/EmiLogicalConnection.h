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
#include "EmiNetRandom.h"
#include "EmiNatPunchthrough.h"
#include "EmiMessageHeader.h"
#include "EmiP2PEndpoints.h"

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
    typedef typename Binding::TimerCookie    TimerCookie;
    typedef typename SockDelegate::ConnectionOpenedCallbackCookie ConnectionOpenedCallbackCookie;
    typedef EmiMessage<Binding>                 EM;
    typedef EmiConn<SockDelegate, ConnDelegate> EC;
    typedef EmiNatPunchthrough<Binding, EmiLogicalConnection> ENP;
    
    friend class EmiNatPunchthrough<Binding, EmiLogicalConnection>;
    
    ReceiverBuffer &_receiverBuffer;
    
    bool _closing;
    EC *_conn;
    EmiP2PEndpoints   _p2pEndpoints;
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
    
    ENP  *_natPunchthrough; // This is NULL except in the NAT punchthrough phase of P2P connections
    bool  _natPunchthroughFailed;
    
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
            
            if (!error) {
                // This instructs the RTO timer that the connection is now opened.
                _conn->connectionOpened();
            }
            
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
        return EmiNetRandom<Binding>::random() & EMI_HEADER_SEQUENCE_NUMBER_MASK;
    }
    
    int32_t sequenceNumberDifference(const EmiMessageHeader& header, bool updateExpectedSequenceNumber) {
        EmiLogicalConnectionMemo::iterator cur = _otherHostSequenceMemo.find(header.channelQualifier);
        EmiLogicalConnectionMemo::iterator end = _otherHostSequenceMemo.end();
        
        EmiSequenceNumber expectedSequenceNumber = (end == cur ? _otherHostInitialSequenceNumber : (*cur).second);
        
        if (updateExpectedSequenceNumber) {
            _otherHostSequenceMemo[header.channelQualifier] =
                EmiNetUtil::cyclicMax24(expectedSequenceNumber,
                                        (header.sequenceNumber+1) & EMI_HEADER_SEQUENCE_NUMBER_MASK);
        }
        
        return EmiNetUtil::cyclicDifference24Signed(expectedSequenceNumber, header.sequenceNumber);
    }
    
    inline EmiSequenceNumber sequenceMemoForChannelQualifier(EmiChannelQualifier cq) {
        EmiLogicalConnectionMemo::iterator cur = _sequenceMemo.find(cq);
        return (_sequenceMemo.end() == cur ? _initialSequenceNumber : (*cur).second);
    }
    
    // Helper for the constructors
    void commonInit() {
        _initialSequenceNumber = EmiLogicalConnection::generateSequenceNumber();
        
        _reliableHandshakeMsgSn = -1;
        
        _natPunchthrough = NULL;
        _natPunchthroughFailed = false;
    }
    
    // Invoked by EmiNatPunchthrough
    inline void sendNatPunchthroughPacket(const sockaddr_storage& addr, const uint8_t *buf, size_t bufSize) {
        if (_conn) {
            _conn->sendDatagram(addr, buf, bufSize);
        }
    }
    
    // Invoked by EmiNatPunchthrough
    inline void natPunchthroughFinished(bool success) {
        _natPunchthroughFailed = !success;
        
        if (_conn) {
            _conn->emitNatPunchthroughFinished(success);
        }
    }
    
    // Invoked by EmiNatPunchthrough
    inline void natPunchthroughTeardownFinished() {
        delete _natPunchthrough;
        _natPunchthrough = NULL;
    }
    
public:
    
    // This constructor is invoked for connections where
    // this side is the server side.
    EmiLogicalConnection(EC *connection, 
                         ReceiverBuffer& receiverBuffer,
                         EmiTimeInterval now,
                         EmiSequenceNumber sequenceNumber) :
    _receiverBuffer(receiverBuffer),
    _closing(false), _conn(connection),
    _p2pEndpoints(),
    _otherHostInitialSequenceNumber(sequenceNumber),
    _sendingSyn(false), _connectionOpenedCallbackCookie() {
        ASSERT(EMI_CONNECTION_TYPE_SERVER == _conn->getType());
        
        commonInit();
        
        // sendInitMessage should not fail, because it only fails when this
        // connection is not EMI_CONNECTION_TYPE_SERVER
        Error err;
        ASSERT(sendInitMessage(now, err));
    }
    
    // This constructor is invoked for connections where
    // this side is the client side and for P2P connections.
    EmiLogicalConnection(EC *connection,
                         ReceiverBuffer& receiverBuffer,
                         EmiTimeInterval now,
                         const ConnectionOpenedCallbackCookie& connectionOpenedCallbackCookie) :
    _receiverBuffer(receiverBuffer),
    _closing(false), _conn(connection),
    _p2pEndpoints(),
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
        
        uint8_t *data = NULL;
        size_t dataLen = 0;
        
        if (_sendingSyn) {
            data    = _conn->getP2PData().p2pCookie;
            dataLen = _conn->getP2PData().p2pCookieLength;
            
            _reliableHandshakeMsgSn = _initialSequenceNumber;
        }
        
        if (!_conn->enqueueMessage(now,
                                   EMI_PRIORITY_CONTROL,
                                   EMI_CONTROL_CHANNEL,
                                   _initialSequenceNumber,
                                   (_sendingSyn ? EMI_SYN_FLAG : EMI_SYN_FLAG | EMI_RST_FLAG),
                                   data,
                                   dataLen,
                                   /*reliable:*/_sendingSyn,
                                   err)) {
            error = true;
        }
        
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
    
    void gotPrxRstSynAck(EmiTimeInterval now, const TimerCookie& timerCookie,
                         const uint8_t *data, size_t len) {
        
        /// Parse the incoming data
        
        const int family = _conn->getLocalAddress().ss_family;
        const size_t ipLen = EmiNetUtil::ipLength(_conn->getLocalAddress());
        static const size_t portLen = sizeof(uint16_t);
        const size_t endpointPairLen = 2*(ipLen+portLen);
        const size_t dataLen = 2*endpointPairLen;
        
        if (len != dataLen) {
            return;
        }
        
        
        /// Release the reliable PRX-ACK handshake message
        
        releaseReliableHandshakeMsg(now);
        
        
        /// Initiate NAT punch through
        
        if (!_natPunchthrough) {
            _p2pEndpoints = EmiP2PEndpoints(family,
                                            /*myEndpointPair:*/data, endpointPairLen,
                                            /*peerEndpointPair:*/data+endpointPairLen, endpointPairLen);
            
            sockaddr_storage peerInnerAddr;
            sockaddr_storage peerOuterAddr;
            _p2pEndpoints.extractPeerInnerAddress(&peerInnerAddr);
            _p2pEndpoints.extractPeerOuterAddress(&peerOuterAddr);
            
            _natPunchthrough = new ENP(_conn->config.connectionTimeout,
                                       *this,
                                       timerCookie,
                                       _initialSequenceNumber,
                                       _conn->getRemoteAddress(),
                                       _conn->getP2PData(),
                                       _p2pEndpoints,
                                       peerInnerAddr,
                                       peerOuterAddr);
        }
    }
    
    inline void gotPrxSyn(const sockaddr_storage& remoteAddr,
                          const uint8_t *data,
                          size_t len) {
        if (_p2pEndpoints.myEndpointPair &&
            _p2pEndpoints.peerEndpointPair) {
            ENP::gotPrxSyn(*this, _conn->getP2PData(),
                           _p2pEndpoints,
                           remoteAddr, data, len);
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
        else if (_conn &&
                 EMI_CONNECTION_TYPE_P2P == _conn->getType() &&
                 ENP::prxSynAckIsValid(_conn->getP2PData(), _p2pEndpoints, data, len)) {
            sockaddr_storage peerInnerEndpoint;
            sockaddr_storage peerOuterEndpoint;
            _p2pEndpoints.extractPeerInnerAddress(&peerInnerEndpoint);
            _p2pEndpoints.extractPeerOuterAddress(&peerOuterEndpoint);
            
            if (0 == EmiAddressCmp::compare(remoteAddr, peerInnerEndpoint) &&
                0 == EmiAddressCmp::compare(_conn->getRemoteAddress(), peerOuterEndpoint)) {
                // This probably demands some explanation. Given the fact that the NAT punch
                // through works by sending two PRX-SYN packets in parallel, one to the inner
                // endpoint, and one to the outer endpoint, which independently get their own
                // PRX-SYN-ACK response, there is a race condition which might lead to one
                // peer finishing its part of the NAT punch through process with the inner
                // endpoint, while the other picks the outer endpoint.
                //
                // When this happens, the connection is in effect dropped, because both peers
                // will reject packets coming from the other peer because of their "invalid"
                // remote address.
                //
                // To work around this, the protocol specifies that the inner endpoint has
                // priority. This is achieved through two steps, this being one of them.
                // If a valid PRX-SYN-ACK message is received from the other peer's inner
                // endpoint, we will use the inner endpoint, even if the NAT punch through
                // process form our point of view has decided to use the outer endpoint.
                //
                // (The other thing that needs to be done is that a peer that has concluded
                // to use the inner endpoint looks for non-PRX packets from the outer
                // endpoint and respond to them by sending a PRX-SYN-ACK message to the inner
                // endpoint)
                
                // Switch to the inner endpoint
                
                memcpy(connsRemoteAddr, &remoteAddr, sizeof(sockaddr_storage));
            }
        }
    }
    
    inline void gotPrxRstAck(const sockaddr_storage& remoteAddr) {
        if (_natPunchthrough) {
            _natPunchthrough->gotPrxRstAck(remoteAddr);
        }
    }
    
    bool gotNonPrxMessageFromUnexpectedRemoteHost(const sockaddr_storage& remoteAddr) {
        // See gotPrxSynAck for a description of what we're doing here.
        
        if (!_conn || EMI_CONNECTION_TYPE_P2P != _conn->getType()) {
            return false;
        }
        
        sockaddr_storage peerInnerEndpoint;
        sockaddr_storage peerOuterEndpoint;
        _p2pEndpoints.extractPeerInnerAddress(&peerInnerEndpoint);
        _p2pEndpoints.extractPeerOuterAddress(&peerOuterEndpoint);
        
        if (0 != EmiAddressCmp::compare(remoteAddr, peerOuterEndpoint) ||
            0 != EmiAddressCmp::compare(_conn->getRemoteAddress(), peerInnerEndpoint)) {
            return false;
        }
        
        ENP::sendPrxSynAckPacket(*this, _conn->getP2PData(),
                                 _p2pEndpoints, peerInnerEndpoint);
        
        return true;
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
            // Send reliable PRX-ACK message
            
            uint8_t buf[96];
            size_t msgSize = EM::fillPrxAckMessage(inboundAddr, buf, sizeof(buf));
            
            Error err;
            ASSERT(_conn->enqueueMessage(now,
                                         EMI_PRIORITY_CONTROL,
                                         EMI_CONTROL_CHANNEL,
                                         _initialSequenceNumber,
                                         EMI_PRX_FLAG | EMI_ACK_FLAG,
                                         buf,
                                         msgSize,
                                         /*reliable:*/true,
                                         err));
            
            _reliableHandshakeMsgSn = _initialSequenceNumber;
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
        
        if ((EMI_CHANNEL_TYPE_UNRELIABLE_SEQUENCED == channelType ||
             EMI_CHANNEL_TYPE_RELIABLE_SEQUENCED   == channelType) &&
            -1 != header.sequenceNumber) {
            int32_t snDiff = sequenceNumberDifference(header, true);
            
            if (snDiff > 0) {
                // The packet arrived out of order; drop it
                return false;
            }
            else if (snDiff < 0) {
                // We have lost one or more packets.
                _conn->emitPacketLoss(channelQualifier, -snDiff);
            }
        }
        
        if (EMI_CHANNEL_TYPE_UNRELIABLE == channelType || EMI_CHANNEL_TYPE_UNRELIABLE_SEQUENCED == channelType) {
            if (header.flags & EMI_ACK_FLAG) EMI_GOT_INVALID_PACKET("Got unreliable message with ACK flag");
            if (header.flags & EMI_SACK_FLAG) EMI_GOT_INVALID_PACKET("Got unreliable message with SACK flag");
            
            if (0 == header.length) {
                // Unreliable packets with zero header length are nonsensical
                return false;
            }
            
            _conn->emitMessage(channelQualifier, data, offset, header.length);
        }
        else if (EMI_CHANNEL_TYPE_RELIABLE_SEQUENCED == channelType) {
            if (header.flags & EMI_SACK_FLAG) EMI_GOT_INVALID_PACKET("SACK does not make sense on RELIABLE_SEQUENCED channels");
            
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
            
            bool hasSequenceNumber = (-1 != header.sequenceNumber);
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
                _otherHostSequenceMemo[channelQualifier] = (header.sequenceNumber+1) & EMI_HEADER_SEQUENCE_NUMBER_MASK;
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
        if (!_conn->enqueueMessage(now,
                                   EMI_PRIORITY_CONTROL,
                                   EMI_CONTROL_CHANNEL,
                                   _initialSequenceNumber,
                                   /*flags:*/EMI_RST_FLAG,
                                   /*data:*/NULL,
                                   /*dataLen:*/0,
                                   /*reliable:*/true,
                                   err)) {
            error = true;
        }
        
        return !error;
    }
    
    // Returns false if the sender buffer was full and the message couldn't be sent
    bool send(const PersistentData& data, EmiTimeInterval now, EmiChannelQualifier channelQualifier, EmiPriority priority, Error& err) {
        // This has to be called before we increment _sequenceMemo[cq]
        EmiSequenceNumber prevSeqMemo = sequenceMemoForChannelQualifier(channelQualifier);
        
        EmiChannelType channelType = EMI_CHANNEL_QUALIFIER_TYPE(channelQualifier);
        
        bool reliable = (EMI_CHANNEL_TYPE_RELIABLE_SEQUENCED == channelType ||
                         EMI_CHANNEL_TYPE_RELIABLE_ORDERED == channelType);
        
        if (isClosed()) {
            err = Binding::makeError("com.emilir.eminet.closed", 0);
            return false;
        }
        
        if (0 == Binding::extractLength(data)) {
            err = Binding::makeError("com.emilir.eminet.emptymessage", 0);
            return false;
        }
        
        if (_conn->enqueueMessage(now,
                                  priority,
                                  channelQualifier,
                                  /*sequenceNumber:*/prevSeqMemo,
                                  /*flags:*/0,
                                  &data,
                                  reliable,
                                  err)) {
            // enqueueMessage succeeded
            _sequenceMemo[channelQualifier] = (prevSeqMemo+1) & EMI_HEADER_SEQUENCE_NUMBER_MASK;
        }
        else {
            // enqueueMessage failed
            return false;
        }
        
        if (EMI_CHANNEL_TYPE_RELIABLE_SEQUENCED == channelType) {
            // We have now successfully enqueued a new message on a RELIABLE_SEQUENCED
            // channel. We can safely deregister previous reliable messages on the
            // channel.
            
            EmiLogicalConnectionMemo::iterator cur = _reliableSequencedBuffer.find(channelQualifier);
            if (_reliableSequencedBuffer.end() != cur) {
                _conn->deregisterReliableMessages(now, channelQualifier, prevSeqMemo);
            }
            _reliableSequencedBuffer[channelQualifier] = prevSeqMemo;
        }
        
        return true;
    }
    
    bool isOpening() const {
        return _sendingSyn;
    }
    bool isClosing() const {
        // Strictly speaking, checking if the connection is closed
        // should not be necessary, because this method ought to
        // never be called when the connection is closed, but we
        // do it just to be sure.
        return !isClosed() && _closing;
    }
    bool isClosed() const {
        return !_conn;
    }
    
    EmiSequenceNumber getOtherHostInitialSequenceNumber() const {
        return _otherHostInitialSequenceNumber;
    }
    
    EmiP2PState getP2PState() const {
        if (!_conn) {
            return EMI_P2P_STATE_NOT_ESTABLISHING;
        }
        else if (EMI_CONNECTION_TYPE_P2P != _conn->getType()) {
            return EMI_P2P_STATE_NOT_ESTABLISHING;
        }
        else if (0 != EmiAddressCmp::compare(_conn->getOriginalRemoteAddress(), _conn->getRemoteAddress())) {
            // If the current remote address is not equal to the original
            // remote address, that means that the P2P connection is
            // established.
            //
            // Note that the connection can be established even if
            // _natPunchthrough is not NULL, because _natPunchthrough is
            // even after the P2P connection is established, until the P2P
            // mediator connection is torn down.
            return EMI_P2P_STATE_ESTABLISHED;
        }
        else if (_natPunchthrough) {
            // Note that NULL != _natPunchthrough does not imply that the
            // P2P state is EMI_P2P_STATE_ESTABLISHING, it can actually
            // be EMI_P2P_STATE_ESTABLISHED, as described above.
            return EMI_P2P_STATE_ESTABLISHING;
        }
        else {
            return _natPunchthroughFailed ? EMI_P2P_STATE_FAILED : EMI_P2P_STATE_ESTABLISHING;
        }
    }
};

#endif
