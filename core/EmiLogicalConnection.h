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

#include <map>

template<class Data>
class EmiMessage;
template<class SockDelegate, class ConnDelegate>
class EmiConn;

typedef std::map<EmiChannelQualifier, EmiSequenceNumber> EmiLogicalConnectionMemo;

template<class SockDelegate, class ConnDelegate>
class EmiLogicalConnection {
    typedef typename SockDelegate::Error Error;
    typedef typename SockDelegate::Data  Data;
    typedef typename SockDelegate::ConnectionOpenedCallbackCookie ConnectionOpenedCallbackCookie;
    typedef EmiConn<SockDelegate, ConnDelegate> EC;
    
    bool _closing;
    EC *_conn;
    EmiSequenceNumber _initialSequenceNumber;
    EmiSequenceNumber _otherHostInitialSequenceNumber;
    
    ConnectionOpenedCallbackCookie _connectionOpenedCallbackCookie;
    bool _sendingSyn;
    
    EmiLogicalConnectionMemo _sequenceMemo;
    EmiLogicalConnectionMemo _otherHostSequenceMemo;
    EmiLogicalConnectionMemo _reliableSequencedBuffer;
    
    int32_t _synMsgSn; // -1 when it has no value
    
private:
    // Private copy constructor and assignment operator
    inline EmiLogicalConnection(const EmiLogicalConnection& other);
    inline EmiLogicalConnection& operator=(const EmiLogicalConnection& other);
    
    void invokeSynRstCallback(bool error, EmiDisconnectReason reason){
        if (_sendingSyn) {
            _sendingSyn = false;
            SockDelegate::connectionOpened(_connectionOpenedCallbackCookie, error, reason, *_conn);
        }
    }
    void releaseSynMsg() {
        if (-1 != _synMsgSn) {
            _conn->deregisterReliableMessages(-1, _synMsgSn);
            _synMsgSn = -1;
        }
    }
    
    static EmiSequenceNumber generateSequenceNumber() {
        // Right shifting a bit because apparently the low bits are usually not that random
        return arc4random() >> 5;
    }
    
    int32_t sequenceNumberDifference(EmiMessageHeader *header, bool updateExpectedSequenceNumber) {
        EmiLogicalConnectionMemo::iterator cur = _otherHostSequenceMemo.find(header->channelQualifier);
        EmiLogicalConnectionMemo::iterator end = _otherHostSequenceMemo.end();
        
        EmiSequenceNumber expectedSequenceNumber = (end == cur ? _otherHostInitialSequenceNumber : (*cur).second);
        
        if (updateExpectedSequenceNumber) {
            _otherHostSequenceMemo[header->channelQualifier] = emiCyclicMax16(expectedSequenceNumber, header->sequenceNumber+1);
        }
        
        return emiCyclicDifference16Signed(expectedSequenceNumber, header->sequenceNumber);
    }
    
    EmiSequenceNumber sequenceMemoForChannelQualifier(EmiChannelQualifier cq) {
        EmiLogicalConnectionMemo::iterator cur = _sequenceMemo.find(cq);
        return (_sequenceMemo.end() == cur ? _initialSequenceNumber : (*cur).second);
    }
    
    // The caller is responsible for releasing the returned object
    EmiMessage<SockDelegate> *makeDataMessage(EmiChannelQualifier cq, const Data& data, EmiPriority priority, EmiTimeInterval now) {
        EmiMessage<SockDelegate> *msg = new EmiMessage<SockDelegate>(data);
        msg->registrationTime = now;
        msg->channelQualifier = cq;
        msg->sequenceNumber = sequenceMemoForChannelQualifier(cq);
        msg->priority = priority;
        
        _sequenceMemo[cq] = msg->sequenceNumber+1;
        
        return msg;
    }
    
    // The caller is responsible for releasing the returned object
    static EmiMessage<SockDelegate> *makeSynAndOrRstMessage(EmiFlags flags, EmiTimeInterval now, EmiSequenceNumber sequenceNumber) {
        EmiMessage<SockDelegate> *msg = new EmiMessage<SockDelegate>;
        msg->priority = EMI_PRIORITY_HIGH;
        msg->registrationTime = now;
        msg->channelQualifier = -1; // Special SYN/RST message channel. SenderBuffer requires this to be an integer
        msg->sequenceNumber = sequenceNumber;
        msg->flags = flags;
        return msg;
    }
    
    // The caller is responsible for releasing the returned object
    EmiMessage<SockDelegate> *makeSynMessage(EmiTimeInterval now) const {
        return EmiLogicalConnection::makeSynAndOrRstMessage(EMI_SYN_FLAG, now, _initialSequenceNumber);
    }
    
    // The caller is responsible for releasing the returned object
    EmiMessage<SockDelegate> *makeSynRstMessage(EmiTimeInterval now) const {
        return EmiLogicalConnection::makeSynAndOrRstMessage(EMI_SYN_FLAG | EMI_RST_FLAG, now, _initialSequenceNumber);
    }
    
    // The caller is responsible for releasing the returned object
    EmiMessage<SockDelegate> *makeRstMessage(EmiTimeInterval now) const {
        return EmiLogicalConnection::makeSynAndOrRstMessage(EMI_RST_FLAG, now, _initialSequenceNumber);
    }
    
    // Helper for the constructors
    void commonInit() {
        _initialSequenceNumber = EmiLogicalConnection::generateSequenceNumber();
        
        _synMsgSn = -1;
    }
    
public:
    
    EmiLogicalConnection(EC *connection, EmiTimeInterval now, EmiSequenceNumber sequenceNumber) :
    _closing(false), _conn(connection), _otherHostInitialSequenceNumber(sequenceNumber),
    _sendingSyn(false), _connectionOpenedCallbackCookie() {
        commonInit();
        
        Error err;
        if (!resendInitMessage(now, err)) {
            // This should not happen, because resendInitMessage only fails when
            // this host is the connection's initiator, which is not the case here.
            SockDelegate::panic();
        }
    }
    EmiLogicalConnection(EC *connection, EmiTimeInterval now, const ConnectionOpenedCallbackCookie& connectionOpenedCallbackCookie) :
    _conn(connection), _otherHostInitialSequenceNumber(0),
    _sendingSyn(true), _connectionOpenedCallbackCookie(connectionOpenedCallbackCookie) {
        commonInit();
        
        Error err;
        if (!resendInitMessage(now, err)) {
            // This really ought not to happen; resendInitMessage only fails when
            // the sender buffer is full, but this init message should be the first
            // message on the whole connection.
            SockDelegate::panic();
        }
    }
    virtual ~EmiLogicalConnection() {
        // Just to be sure, since these ivars are __unsafe_unretained
        _conn = NULL;
    }
    
    // This method only fails when sending a SYN message, that is,
    // _synRstCallback is set, which happens when this host is the
    // initiator of the connection.
    bool resendInitMessage(EmiTimeInterval now, Error& err) {
        bool error = false;
        
        releaseSynMsg();
        
        EmiMessage<SockDelegate> *msg = _sendingSyn ? makeSynMessage(now) : makeSynRstMessage(now);
        
        if (_sendingSyn) {
            _synMsgSn = msg->sequenceNumber;
        }
        
        if (!_conn->enqueueMessage(now, msg, /*reliable:*/_sendingSyn, err)) {
            error = true;
        }
        
        msg->release();
        
        return !error;
    }
    
    // Invoked by EmiConnection
    void wasClosed(EmiDisconnectReason reason) {
        invokeSynRstCallback(true, EMI_REASON_OTHER_HOST_DID_NOT_RESPOND);
        
        _conn->emitDisconnect(reason);
        _conn = NULL;
        _closing = false;
    }
    
    void gotRst() {
        _conn->deregisterConnection(EMI_REASON_OTHER_HOST_CLOSED);
    }
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
        _conn->deregisterConnection(isClosing() ?
                                    EMI_REASON_THIS_HOST_CLOSED :
                                    EMI_REASON_OTHER_HOST_CLOSED);
    }
    // Returns false if the connection not in the opening state (that's an error)
    bool gotSynRst(EmiSequenceNumber otherHostInitialSequenceNumber) {
        if (!isOpening()) {
            return false;
        }
        
        releaseSynMsg();
        _otherHostInitialSequenceNumber = otherHostInitialSequenceNumber;
        
        invokeSynRstCallback(false, EMI_REASON_NO_ERROR);
        
        return true;
    }
    
    // Returns true if the packet was processed successfully, false otherwise.
#define EMI_GOT_INVALID_PACKET(err) do { /* NSLog(err); */ return false; } while (1)
    bool gotMessage(EmiMessageHeader *header, const Data &data, size_t offset, bool dontFlush) {
        EmiChannelQualifier channelQualifier = header->channelQualifier;
        EmiChannelType channelType = EMI_CHANNEL_QUALIFIER_TYPE(channelQualifier);
        
        if (EMI_CHANNEL_TYPE_UNRELIABLE == channelType || EMI_CHANNEL_TYPE_UNRELIABLE_SEQUENCED == channelType) {
            if (header->flags & EMI_ACK_FLAG) EMI_GOT_INVALID_PACKET("Got unreliable message with ACK flag");
            if (header->flags & EMI_SACK_FLAG) EMI_GOT_INVALID_PACKET("Got unreliable message with SACK flag");
            
            if (EMI_CHANNEL_TYPE_UNRELIABLE_SEQUENCED == channelType &&
                0 < sequenceNumberDifference(header, true)) {
                // The packet arrived out of order; drop it
                return false;
            }
            
            _conn->emitMessage(channelQualifier, data, offset, header->length);
        }
        else if (EMI_CHANNEL_TYPE_RELIABLE_SEQUENCED == channelType) {
            if (header->flags & EMI_SACK_FLAG) EMI_GOT_INVALID_PACKET("SACK does not make sense on RELIABLE_SEQUENCED channels");
            
            if (-1 != header->sequenceNumber &&
                0 < sequenceNumberDifference(header, true)) {
                // The packet arrived out of order; drop it
                return false;
            }
            
            _conn->enqueueAck(channelQualifier, header->sequenceNumber);
            
            if (header->flags & EMI_ACK_FLAG) {
                EmiLogicalConnectionMemo::iterator cur = _reliableSequencedBuffer.find(channelQualifier);
                if (_reliableSequencedBuffer.end() != cur &&
                    (*cur).second == header->ack) {
                    _conn->deregisterReliableMessages(channelQualifier, header->ack);
                    _reliableSequencedBuffer.erase(channelQualifier);
                }
            }
            
            // A packet with zero length indicates that it is just an ACK packet
            if (0 != header->length) {
                size_t realOffset = offset + (header->flags & EMI_ACK_FLAG ? 2 : 0);
                _conn->emitMessage(channelQualifier, data, realOffset, header->length);
            }
        }
        else if (EMI_CHANNEL_TYPE_RELIABLE_ORDERED == channelType) {
            if (header->flags & EMI_SACK_FLAG) EMI_GOT_INVALID_PACKET("SACK is not implemented");
            
            bool hasSequenceNumber = -1 != header->sequenceNumber;
            int32_t seqDiff = 0;
            if (hasSequenceNumber) {
                seqDiff = sequenceNumberDifference(header, false);
            }
            
            if (hasSequenceNumber && seqDiff >= 0) {
                // Send an ACK only if this message has a sequence number
                //
                // Also, send an ACK only if the received message's sequence number
                // is what we were expecting or if if it was older than we expected.
                _conn->enqueueAck(channelQualifier, header->sequenceNumber);
            }
            
            if (header->flags & EMI_ACK_FLAG) {
                _conn->deregisterReliableMessages(channelQualifier, header->ack);
            }
            
            if (hasSequenceNumber && 0 != seqDiff) {
                if (seqDiff < 0) {
                    // This message is newer than what we were expecting; save it in the input buffer
                    _conn->bufferMessage(header, data, offset);
                }
                
                return false;
            }
            
            if (hasSequenceNumber) {
                _otherHostSequenceMemo[channelQualifier] = header->sequenceNumber+1;
            }
            
            // A packet with zero length indicates that it is just an ACK packet
            if (0 != header->length) {
                _conn->emitMessage(channelQualifier, data, offset, header->length);
                
                // The connection might have been closed in the message event handler
                if (_conn && !dontFlush && hasSequenceNumber) {
                    _conn->flushBuffer(channelQualifier, header->sequenceNumber);
                }
            }
        }
        else {
            EMI_GOT_INVALID_PACKET("Unknown channel type");
        }
        
        return true;
    }
#undef EMI_GOT_INVALID_PACKET
    
    bool close(EmiTimeInterval now, Error& err) {
        if (_closing || !_conn) {
            // We're already closing or closed; no need to initiate a new close process
            err = SockDelegate::makeError("com.emilir.eminet.closed", 0);
            return false;
        }
        
        bool error = false;
        
        _closing = true;
        
        if (isOpening()) {
            // If the connection is not yet open, cancel the opening process
            releaseSynMsg();
            invokeSynRstCallback(true, EMI_REASON_THIS_HOST_CLOSED);
        }
        
        // Send an RST (connection close) message
        EmiMessage<SockDelegate> *msg = makeRstMessage(now);
        if (!_conn->enqueueMessage(now, msg, /*reliable:*/true, err)) {
            error = true;
        }
        msg->release();
        
        return !error;
    }
    // Returns false if the sender buffer was full and the message couldn't be sent
    bool send(const Data& data, EmiTimeInterval now, EmiChannelQualifier channelQualifier, EmiPriority priority, Error& err) {
        bool error = false;
        
        // This has to be called before makeDataMessage
        EmiChannelQualifier prevSeqMemo = sequenceMemoForChannelQualifier(channelQualifier);
        
        EmiMessage<SockDelegate> *msg = makeDataMessage(channelQualifier, data, priority, now);
        
        EmiChannelType channelType = EMI_CHANNEL_QUALIFIER_TYPE(channelQualifier);
        
        bool reliable = (EMI_CHANNEL_TYPE_RELIABLE_SEQUENCED == channelType ||
                         EMI_CHANNEL_TYPE_RELIABLE_ORDERED == channelType);
        
        if (isClosed()) {
            err = SockDelegate::makeError("com.emilir.eminet.closed", 0);
            error = true;
            goto cleanup;
        }
        
        if (0 == SockDelegate::extractLength(data)) {
            err = SockDelegate::makeError("com.emilir.eminet.emptymessage", 0);
            error = true;
            goto cleanup;
        }
        
        if (EMI_CHANNEL_TYPE_RELIABLE_SEQUENCED == channelType) {
            EmiLogicalConnectionMemo::iterator cur = _reliableSequencedBuffer.find(channelQualifier);
            if (_reliableSequencedBuffer.end() != cur) {
                _conn->deregisterReliableMessages(channelQualifier, prevSeqMemo);
            }
            _reliableSequencedBuffer[channelQualifier] = msg->sequenceNumber;
        }
        
        if (!_conn->enqueueMessage(now, msg, reliable, err)) {
            // _sequenceMemo[channelQualifier] has been bumped by _makeDataMessage. Since the message wasn't sent: Undo that
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
