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

template<class Binding>
class EmiSenderBuffer {
    typedef typename Binding::Error Error;
    typedef EmiMessage<Binding>     EM;
    
    struct NextMsgTreeCmp {
        inline bool operator()(const EM *a, const EM *b) const {
            EmiTimeInterval art = a->registrationTime;
            EmiTimeInterval brt = b->registrationTime;
            
            if (art != brt) return art < brt;
            else {
                int32_t acq = a->channelQualifier;
                int32_t bcq = b->channelQualifier;
                
                if (acq != bcq) return acq < bcq;
                else {
                    return a->nonWrappingSequenceNumber < b->nonWrappingSequenceNumber;
                }
            }
        }
    };
    
    struct SendBufferCmp {
        inline bool operator()(const EM *a, const EM *b) const {
            int32_t acq = a->channelQualifier;
            int32_t bcq = b->channelQualifier;
            
            if (acq != bcq) return acq < bcq;
            else {
                return a->nonWrappingSequenceNumber < b->nonWrappingSequenceNumber;
            }
        }
    };
    
    typedef std::vector<EM *> EmiMessageVector;
    typedef std::set<EM *, NextMsgTreeCmp>      NextMsgTree;
    typedef std::set<EM *, SendBufferCmp>       SendBuffer;
    typedef typename EmiMessageVector::iterator EmiMessageVectorIter;
    typedef typename NextMsgTree::iterator      NextMsgTreeIter;
    typedef typename SendBuffer::iterator       SendBufferIter;
    
    // Buffer max size
    size_t _size;
    
    // Contains at most one message per channel. It is sorted by regTime
    NextMsgTree _nextMsgTree;
    // contains all messages in the reliable buffer. It is sorted by
    // channelQualifier and sequenceNumber
    SendBuffer _sendBuffer;
    size_t _sendBufferSize;
    
private:
    // Private copy constructor and assignment operator
    inline EmiSenderBuffer(const EmiSenderBuffer& other);
    inline EmiSenderBuffer& operator=(const EmiSenderBuffer& other);
    
    EM *messageSearch(EM *messageStub) {
        SendBufferIter iter = _sendBuffer.lower_bound(messageStub);
        SendBufferIter end  = _sendBuffer.end();
        
        // We need to look at both *iter and *(++iter), and of course
        // also guard for if we reach end. That's what this loop does.
        for (int i=0; iter != end && i<2; i++) {
            if (messageStub->channelQualifier == (*iter)->channelQualifier) return *iter;
            ++iter;
        }
        
        return NULL;
    }
    
    size_t messageSize(size_t dataSize, size_t numMessages = 1) {
        return dataSize + numMessages*EM::maximalHeaderSize();
    }
    
public:
    
    EmiSenderBuffer(size_t size) : _size(size), _sendBufferSize(0) {}
    virtual ~EmiSenderBuffer() {
        SendBufferIter iter = _sendBuffer.begin();
        SendBufferIter end  = _sendBuffer.end();
        while (iter != end) {
            (*iter)->release();
            ++iter;
        }
    }
    
    bool fitsIntoBuffer(size_t dataSize, size_t numMessages) {
        return _size >= _sendBufferSize+messageSize(dataSize, numMessages);
    }
    
    // Returns false if the buffer didn't have space for the message
    bool registerReliableMessage(EM *message, Error& err, EmiTimeInterval now) {
        size_t msgSize = messageSize(Binding::extractLength(message->data));
        
        if (_sendBufferSize+msgSize > _size) {
            err = Binding::makeError("com.emilir.eminet.sendbufferoverflow", 0);
            return false;
        }
        
        message->registrationTime = now;
        
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
    //
    // channelQualifier is int32_t to be able to contain -1, which
    // is a special control message channel.
    void deregisterReliableMessages(int32_t channelQualifier,
                                    EmiNonWrappingSequenceNumber nonWrappingSequenceNumber) {
        EM msgStub;
        msgStub.channelQualifier          = channelQualifier;
        msgStub.nonWrappingSequenceNumber = nonWrappingSequenceNumber;
        
        SendBufferIter begin = _sendBuffer.begin();
        SendBufferIter iter  = _sendBuffer.lower_bound(&msgStub);
        
        if (iter == _sendBuffer.end()) return;
        
        EmiMessageVector toBeRemoved;
        do {
            EM *msg = *iter;
            
            if (channelQualifier != msg->channelQualifier) {
                break;
            }
            if (msg->nonWrappingSequenceNumber > nonWrappingSequenceNumber) {
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
            
            if (iter == begin) break;
            
            --iter;
        } while (1);
        
        bool wasInReliableTree = false;
        
        EmiMessageVectorIter viter = toBeRemoved.begin();
        EmiMessageVectorIter vend = toBeRemoved.end();
        while (viter != vend) {
            EM *msg = *viter;
            
            bool wasRemovedFromSendBuffer = (0 != _sendBuffer.erase(msg));
            ASSERT(wasRemovedFromSendBuffer);
            
            _sendBufferSize -= messageSize(Binding::extractLength(msg->data));
            
            bool wasRemovedFromNextMsgTree = 0 != _nextMsgTree.erase(msg);
            wasInReliableTree = wasRemovedFromNextMsgTree || wasInReliableTree;
            
            msg->release();
            
            ++viter;
        }
        
        if (wasInReliableTree) {
            EM *newMsg = messageSearch(&msgStub);
            if (newMsg) _nextMsgTree.insert(newMsg);
        }
    }
    
    bool empty() const {
        return _nextMsgTree.empty();
    }
    
    template<class Delegate>
    void eachCurrentMessage(EmiTimeInterval now, EmiTimeInterval rto,
                            Delegate& delegate) {
        NextMsgTreeIter iter = _nextMsgTree.begin();
        NextMsgTreeIter end = _nextMsgTree.end();
        
        EmiMessageVector toBePushedToTheEnd;
        
        while (iter != end) {
            EM *msg = *iter;
            
            if (rto > now-msg->registrationTime) {
                // This message was sent less than RTO ago
                break;
            }
            
            // Since we're iterating _nextMsgTree, we
            // can't modify it here. Do it later.
            toBePushedToTheEnd.push_back(msg);
            
            delegate.eachCurrentMessageIteration(now, msg);
            
            ++iter;
        }
        
        EmiMessageVectorIter viter = toBePushedToTheEnd.begin();
        EmiMessageVectorIter vend  = toBePushedToTheEnd.end();
        while (viter != vend) {
            EM *msg = *viter;
            
            // We want to update msg->registrationTime, but because
            // _nextMsgTree's ordering depends on it, we can't change
            // it while the object is in _nextMsgTree
            
            bool wasRemovedFromNextMsgTree = 0 != _nextMsgTree.erase(msg);
            ASSERT(wasRemovedFromNextMsgTree);
            
            msg->registrationTime = now;
            
            bool wasInserted = _nextMsgTree.insert(msg).second;
            ASSERT(wasInserted);
            
            ++viter;
        }
    }
};

#endif
