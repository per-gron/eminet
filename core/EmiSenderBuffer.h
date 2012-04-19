//
//  EmiSenderBuffer.h
//  roshambo
//
//  Created by Per Eckerdal on 2012-02-16.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#ifndef emilir_EmiSenderBuffer_h
#define emilir_EmiSenderBuffer_h

#include "EmiMessage.h"
#include "EmiNetUtil.h"

#include <set>
#include <vector>

template<class SockDelegate>
class EmiSenderBuffer {
    typedef void (^EmiSenderBufferIteratorBlock)(EmiMessage<SockDelegate> *message);
    
    struct EmiSenderBufferNextMsgTreeCmp {
        bool operator()(EmiMessage<SockDelegate> *a, EmiMessage<SockDelegate> *b) const {
            EmiTimeInterval art = a->registrationTime;
            EmiTimeInterval brt = b->registrationTime;
            
            if (art < brt) return true;
            else if (art > brt) return false;
            else {
                int32_t acq = a->channelQualifier;
                int32_t bcq = b->channelQualifier;
                
                if (acq < bcq) return true;
                else if (acq > bcq) return false;
                else {
                    return emiCyclicDifference16Signed(a->sequenceNumber, b->sequenceNumber) < 0;
                }
            }
        }
    };
    
    struct EmiSenderBufferSendBufferCmp {
        bool operator()(EmiMessage<SockDelegate> *a, EmiMessage<SockDelegate> *b) const {
            int32_t acq = a->channelQualifier;
            int32_t bcq = b->channelQualifier;
            
            if (acq < bcq) return true;
            else if (acq > bcq) return false;
            else {
                return emiCyclicDifference16Signed(a->sequenceNumber, b->sequenceNumber) < 0;
            }
        }
    };
    
    typedef std::set<EmiMessage<SockDelegate> *, EmiSenderBufferNextMsgTreeCmp> EmiSenderBufferNextMsgTree;
    typedef std::set<EmiMessage<SockDelegate> *, EmiSenderBufferSendBufferCmp>  EmiSenderBufferSendBuffer;
    typedef typename std::set<EmiMessage<SockDelegate> *, EmiSenderBufferNextMsgTreeCmp>::iterator EmiSenderBufferNextMsgTreeIter;
    typedef typename std::set<EmiMessage<SockDelegate> *, EmiSenderBufferSendBufferCmp>::iterator  EmiSenderBufferSendBufferIter;
    
    // Buffer max size
    size_t _size;
    
    // Contains at most one message per channel. It is sorted by regTime
    EmiSenderBufferNextMsgTree _nextMsgTree;
    // contains all messages in the reliable buffer. It is sorted by
    // channelQualifier and sequenceNumber
    EmiSenderBufferSendBuffer _sendBuffer;
    size_t _sendBufferSize;
    
private:
    // Private copy constructor and assignment operator
    inline EmiSenderBuffer(const EmiSenderBuffer& other);
    inline EmiSenderBuffer& operator=(const EmiSenderBuffer& other);
    
    EmiMessage<SockDelegate> *messageSearch(EmiMessage<SockDelegate> *messageStub) {
        EmiSenderBufferSendBufferIter iter = _sendBuffer.lower_bound(messageStub);
        EmiSenderBufferSendBufferIter end = _sendBuffer.end();
        
        // We need to look at both *iter and *(++iter), and of course
        // also guard for if we reach end. That's what this loop does.
        for (int i=0; iter != end && i<2; i++) {
            if (messageStub->channelQualifier == (*iter)->channelQualifier) return *iter;
            ++iter;
        }
        
        return NULL;
    }
    
public:
    
    EmiSenderBuffer(size_t size) : _size(size), _sendBufferSize(0) {}
    virtual ~EmiSenderBuffer() {
        EmiSenderBufferSendBufferIter iter = _sendBuffer.begin();
        EmiSenderBufferSendBufferIter end = _sendBuffer.end();
        while (iter != end) {
            (*iter)->release();
            ++iter;
        }
    }
    
    // Returns false if the buffer didn't have space for the message
    bool registerReliableMessage(EmiMessage<SockDelegate> *message, typename SockDelegate::Error& err) {
        size_t msgSize = message->approximateSize();
        
        if (_sendBufferSize+msgSize > _size) {
            err = SockDelegate::makeError("com.emilir.eminet.sendbufferoverflow", 0);
            return false;
        }
        
        // Check if there already is a message with this channel qualifier in the buffer
        if (NULL == messageSearch(message)) {
            // Only add to _nextMsgTree if there wasn't already a message for that connection id
            // and channel id in the system.
            _nextMsgTree.insert(message);
        }
        
        bool wasInserted = _sendBuffer.insert(message).second;
        if (wasInserted) {
            message->retain();
            _sendBufferSize += msgSize;
        }
        
        return true;
    }
    
    // Deregisters all messages on the particular channelQualifier
    // whose sequenceNumber <= sequenceNumber
    void deregisterReliableMessages(int32_t channelQualifier, EmiSequenceNumber sequenceNumber) {
        EmiMessage<SockDelegate> msgStub;
        msgStub.channelQualifier = channelQualifier;
        msgStub.sequenceNumber = sequenceNumber;
        
        EmiSenderBufferSendBufferIter begin = _sendBuffer.begin();
        EmiSenderBufferSendBufferIter iter  = _sendBuffer.lower_bound(&msgStub);
        
        if (iter == _sendBuffer.end()) return;
        
        std::vector<EmiMessage<SockDelegate> *> toBeRemoved;
        do {
            EmiMessage<SockDelegate> *msg = *iter;
            
            if (channelQualifier != msg->channelQualifier) {
                break;
            }
            if (msg->sequenceNumber > sequenceNumber) {
                // This can happen because we used lower_bound.
                // It should not happen more than once, though.
                //
                // We do not continue; here, because that would
                // iterate the loop without decrementing iter,
                // resulting in an infinite loop.
            }
            else {
                toBeRemoved.push_back(msg);
            }
            
            --iter;
        } while (iter != begin);
        
        bool wasInReliableTree = false;
        
        typename std::vector<EmiMessage<SockDelegate> *>::iterator viter = toBeRemoved.begin();
        typename std::vector<EmiMessage<SockDelegate> *>::iterator vend = toBeRemoved.end();
        while (viter != vend) {
            EmiMessage<SockDelegate> *msg = *viter;
            
            bool wasRemovedFromSendBuffer = (0 != _sendBuffer.erase(msg));
            if (wasRemovedFromSendBuffer) {
                // wasRemovedFromSendBuffer should always be true, but we check just in case
                _sendBufferSize -= msg->approximateSize();
            }
            
            bool wasRemovedFromNextMsgTree = 0 != _nextMsgTree.erase(msg);
            wasInReliableTree = wasRemovedFromNextMsgTree || wasInReliableTree;
            
            msg->release();
            
            ++viter;
        }
        
        if (wasInReliableTree) {
            EmiMessage<SockDelegate> *newMsg = messageSearch(&msgStub);
            if (NULL != newMsg) _nextMsgTree.insert(newMsg);
        }
    }
    
    bool empty() const {
        return _nextMsgTree.empty();
    }
    void eachCurrentMessage(EmiTimeInterval now, EmiTimeInterval rto, EmiSenderBufferIteratorBlock block) const {
        EmiSenderBufferNextMsgTreeIter iter = _nextMsgTree.begin();
        EmiSenderBufferNextMsgTreeIter end = _nextMsgTree.end();
        
        while (iter != end) {
            EmiMessage<SockDelegate> *msg = *iter;
            
            if (rto > now-msg->registrationTime) {
                // This message was sent less than RTO ago
                break;
            }
            
            msg->registrationTime = now;
            block(msg);
            
            ++iter;
        }
    }
};

#endif
