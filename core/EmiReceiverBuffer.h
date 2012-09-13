//
//  EmiReceiverBuffer.h
//  roshambo
//
//  Created by Per Eckerdal on 2012-02-16.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#ifndef emilir_EmiReceiverBuffer_h
#define emilir_EmiReceiverBuffer_h

#include "EmiNetUtil.h"
#include "EmiMessageHeader.h"

#include <set>
#include <map>
#include <vector>

template<class SockDelegate, class Receiver>
class EmiReceiverBuffer {
    typedef typename SockDelegate::Binding   Binding;
    typedef typename Binding::PersistentData PersistentData;
    typedef typename Binding::TemporaryData  TemporaryData;
    
    typedef std::map<EmiChannelQualifier, EmiNonWrappingSequenceNumber> EmiNonWrappingSequenceNumberMemo;
    
    // The purpose of this class is to encapsulate an efficient
    // algorithm for handling split messages.
    //
    // The class provides a fast way to know whether a split message
    // that arrives is the last one in the split, so we can reconstruct
    // the message and emit it.
    //
    // See http://avdongre.wordpress.com/2011/12/06/disjoint-set-data-structure-c/
    // and http://en.wikipedia.org/wiki/Disjoint-set_data_structure
    //
    // In addition to the conventional disjoint set data structure,
    // this class keeps track of whether the first and last messages
    // are present in the set.
    //
    // This extra data is required to quickly be able to determine
    // whether a disjoint set contains all messages in a split.
    class DisjointMessageSets {
        
        enum {
            DISJOINT_SET_HAS_FIRST_MESSAGE = 0x1,
            DISJOINT_SET_HAS_LAST_MESSAGE  = 0x2
        } DisjointSetFlags;
        
        struct DisjointSet {
            EmiNonWrappingSequenceNumber parent;
            unsigned rank;
            int flags;
            EmiNonWrappingSequenceNumber lastMessage; // The largest sequence number in this set
            size_t size; // The total size, in bytes, of the messages in this set
            
            DisjointSet(EmiNonWrappingSequenceNumber i) :
            parent(i),
            rank(0),
            flags(0),
            lastMessage(0),
            size(0) { }
        };
        
        typedef std::pair<EmiChannelQualifier, EmiNonWrappingSequenceNumber> ForestKey;
        
        struct ForestCmp {
            inline bool operator()(const ForestKey& a, const ForestKey& b) const {
                return (a.first  != b.first ?
                        a.first  <  b.first :
                        a.second <  b.second);
            }
        };
        
        typedef std::map<ForestKey, DisjointSet, ForestCmp> ForestMap;
        typedef typename ForestMap::iterator ForestIter;
        
        ForestMap _forest;
        
        typedef std::pair<ForestKey, DisjointSet*> FindResult;
        
        FindResult find(EmiChannelQualifier cq, EmiNonWrappingSequenceNumber sn) {
            ForestKey key(cq, sn);
            
            ForestIter iter = _forest.find(key);
            ForestIter end  = _forest.end();
            if (iter == end) {
                // If there is no entry for this sequence number and
                // channel qualifier, create one.
                iter = _forest.insert(make_pair(key, DisjointSet(sn))).first;
            }
            
            DisjointSet& ds = (*iter).second;
            
            if (ds.parent == sn) {
                return std::make_pair(key, &ds);
            }
            else {
                FindResult result = find(cq, ds.parent);
                ds.parent = result.first.second;
                return result;
            }
        }
        
        // Returns the new root
        FindResult merge(EmiChannelQualifier cq,
                         EmiNonWrappingSequenceNumber i,
                         EmiNonWrappingSequenceNumber j) {
            FindResult findIResult = find(cq, i);
            FindResult findJResult = find(cq, j);
            
            EmiNonWrappingSequenceNumber rootISn = findIResult.first.second;
            EmiNonWrappingSequenceNumber rootJSn = findJResult.first.second;
            
            DisjointSet& rootI = *(findIResult.second);
            DisjointSet& rootJ = *(findJResult.second);
            
            if (rootISn != rootJSn) {
                bool iIsTheNewRoot = rootI.rank > rootJ.rank;
                
                EmiNonWrappingSequenceNumber newRootSn = (iIsTheNewRoot ? rootISn : rootJSn);
                
                DisjointSet& newRoot = (iIsTheNewRoot ? rootI : rootJ);
                DisjointSet& nonRoot = (iIsTheNewRoot ? rootJ : rootI);
                
                if (rootI.rank == rootJ.rank) {
                    newRoot.rank += 1;
                }
                
                nonRoot.parent = newRootSn;
                newRoot.flags |= nonRoot.flags;
                newRoot.lastMessage = std::max(newRoot.lastMessage,
                                               nonRoot.lastMessage);
                newRoot.size += nonRoot.size;
                
                return (iIsTheNewRoot ? findIResult : findJResult);
            }
            else {
                return findIResult;
            }
        }
        
    public:
        typedef std::pair<bool, std::pair<EmiNonWrappingSequenceNumber, size_t> > MessageData;
        
        // This method must be called exactly once per message,
        // otherwise the message size data will get messed up.
        void gotMessage(EmiChannelQualifier cq,
                        EmiNonWrappingSequenceNumber i,
                        EmiMessageFlags messageFlags,
                        size_t messageSize) {
            
            bool first = !(messageFlags & EMI_SPLIT_NOT_FIRST_FLAG);
            bool last  = !(messageFlags & EMI_SPLIT_NOT_LAST_FLAG);
            
            if (first && last) {
                // This is a non-split message; there's no need to store
                // it in the forest since we already know that it is
                // complete.
                return;
            }
            
            // Merge the message with sequence number i with the following
            // message, but only for messages that are not last in a split.
            DisjointSet& root = *(last ?
                                  find(cq, i).second :
                                  merge(cq, i, i+1).second);
            
            if (first) root.flags |= DISJOINT_SET_HAS_FIRST_MESSAGE;
            if (last)  root.flags |= DISJOINT_SET_HAS_LAST_MESSAGE;
            
            root.lastMessage = std::max(root.lastMessage, i);
            root.size += messageSize;
        }
        
        // This method returns a pair of (whether a full set of split messages
        // have been received, [a pair of (the largest sequence number in this
        // set, the total size of the messages in this set in bytes)])
        MessageData getMessageData(EmiChannelQualifier cq,
                                   EmiNonWrappingSequenceNumber i,
                                   EmiMessageFlags messageFlags,
                                   size_t messageSize) {
            
            if (!(messageFlags & EMI_SPLIT_NOT_FIRST_FLAG) &&
                !(messageFlags & EMI_SPLIT_NOT_LAST_FLAG)) {
                // This is a non-split message; there's no need to store
                // it in the forest since we already know that it is
                // complete.
                return std::make_pair(true, std::make_pair(i, messageSize));
            }
            else {
                DisjointSet& root = *(find(cq, i).second);
                
                return std::make_pair(((root.flags & DISJOINT_SET_HAS_FIRST_MESSAGE) &&
                                       (root.flags & DISJOINT_SET_HAS_LAST_MESSAGE)),
                                      std::make_pair(root.lastMessage,
                                                     root.size));
            }
        }
        
        // Removes a message from the disjoint sets data structure.
        // Also removes all older messages than the specified message.
        // The sequence number parameter can be the sequence number
        // of any message that is in the split group to be removed.
        //
        // This method is supposed to be used to implement reliable
        // channels.
        //
        // Note: This method must only be used for messages that are
        // complete, that is, all parts of the split group have been
        // registered. Failing to observe this will wreak havoc with
        // the data structure!
        //
        // Furthermore, this method must not be used for message sets
        // that contain only one message.
        void removeMessageAndOlderMessages(EmiChannelQualifier cq, EmiNonWrappingSequenceNumber i) {
            DisjointSet& root = *(find(cq, i).second);
            
            ASSERT((root.flags & DISJOINT_SET_HAS_FIRST_MESSAGE) &&
                   (root.flags & DISJOINT_SET_HAS_LAST_MESSAGE));
            
            _forest.erase(_forest.lower_bound(ForestKey(cq, 0)),
                          _forest.upper_bound(ForestKey(cq, root.lastMessage)));
        }
        
        // Removes all messages stored for a given channel.
        //
        // This method is supposed to be used to implement unreliable
        // channels.
        void clearChannel(EmiChannelQualifier cq) {
            _forest.erase(_forest.lower_bound(ForestKey(cq, 0)),
                          _forest.upper_bound(ForestKey(cq, EMI_NON_WRAPPING_SEQUENCE_NUMBER_MAX)));
        }
    };
    
    class Entry {
    private:
        // Private copy constructor and assignment operator
        inline Entry(const Entry& other);
        inline Entry& operator=(const Entry& other);
        
    public:
        Entry(EmiNonWrappingSequenceNumber guessedNonWrappedSequenceNumber_,
              const EmiMessageHeader& header_,
              const TemporaryData &data_,
              size_t offset,
              size_t length) :
        guessedNonWrappedSequenceNumber(guessedNonWrappedSequenceNumber_),
        header(header_),
        data(Binding::makePersistentData(Binding::extractData(data_)+offset, length)) {}
        
        Entry() :
        guessedNonWrappedSequenceNumber(0) {}
        
        ~Entry() {
            Binding::releasePersistentData(data);
        }
        
        EmiNonWrappingSequenceNumber guessedNonWrappedSequenceNumber;
        EmiMessageHeader header;
        PersistentData data;
    };
    
    struct BufferTreeCmp {
        inline bool operator()(Entry *a, Entry *b) const {
            EmiChannelQualifier acq = a->header.channelQualifier;
            EmiChannelQualifier bcq = b->header.channelQualifier;
            
            if (acq != bcq) return acq < bcq;
            else {
                return a->guessedNonWrappedSequenceNumber < b->guessedNonWrappedSequenceNumber;
            }
        }
    };
    
    typedef std::set<Entry *, BufferTreeCmp> BufferTree;
    typedef typename BufferTree::iterator    BufferTreeIter;
    // Buffer max size
    size_t _size;
    
    DisjointMessageSets _messageSets;
    BufferTree _tree;
    size_t _bufferSize;
    
    EmiNonWrappingSequenceNumberMemo _otherHostSequenceMemo;
    
    Receiver &_receiver;
    
private:
    // Private copy constructor and assignment operator
    inline EmiReceiverBuffer(const EmiReceiverBuffer& other);
    inline EmiReceiverBuffer& operator=(const EmiReceiverBuffer& other);
    
    static size_t bufferEntrySize(size_t headerLength, size_t length) {
        return headerLength + length;
    }
    
    EmiNonWrappingSequenceNumber expectedSequenceNumber(const EmiMessageHeader& header) {
        EmiNonWrappingSequenceNumberMemo::iterator cur = _otherHostSequenceMemo.find(header.channelQualifier);
        EmiNonWrappingSequenceNumberMemo::iterator end = _otherHostSequenceMemo.end();
        
        return (end == cur ? _receiver.getOtherHostInitialSequenceNumber() : (*cur).second);
    }
    
    void bufferMessage(EmiNonWrappingSequenceNumber guessedNonWrappedSequenceNumber,
                       const EmiMessageHeader& header,
                       const TemporaryData& buf,
                       size_t offset,
                       size_t length) {
        size_t msgSize = EmiReceiverBuffer::bufferEntrySize(header.headerLength, header.length);
        
        // Discard the message if it doesn't fit in the buffer
        if (_bufferSize + msgSize <= _size) {
            Entry *entry = new Entry(guessedNonWrappedSequenceNumber, header, buf, offset, length);
            
            bool wasInserted = _tree.insert(entry).second;
            
            // Only increment this._bufferSize if the message wasn't already in the buffer
            if (wasInserted) {
                _bufferSize += msgSize;
                
                _messageSets.gotMessage(entry->header.channelQualifier,
                                        entry->guessedNonWrappedSequenceNumber,
                                        entry->header.flags,
                                        /*messageSize:*/entry->header.length);
            }
            else {
                delete entry;
            }
        }
    }
                
    void remove(Entry *entry) {
        size_t msgSize = EmiReceiverBuffer::bufferEntrySize(entry->header.headerLength,
                                                            entry->header.length);
        
        size_t numErasedElements = _tree.erase(entry);
        if (0 != numErasedElements) {
            // Only decrement this._bufferSize if the message was in the buffer
            _bufferSize -= msgSize;
        }
        
        delete entry;
    }
    
    BufferTreeIter processMessageSetData(EmiChannelQualifier channelQualifier,
                                         EmiNonWrappingSequenceNumber largestSequenceNumberInSet,
                                         BufferTreeIter iter,
                                         BufferTreeIter end,
                                         std::vector<Entry *>& toBeRemoved,
                                         uint8_t *buf, size_t bufSize) {
        Entry *entry = *iter;
        size_t bufPos = 0;
        
        // This loop increments iter until we are past the message set
        // we just processed, and marks all messages we pass for removal.
        do {
            size_t edlen = Binding::extractLength(entry->data);
            ASSERT(bufPos + edlen <= bufSize);
            memcpy(buf+bufPos, Binding::extractData(entry->data), edlen);
            bufPos += edlen;
            
            toBeRemoved.push_back(entry);
            
            ++iter;
        } while (iter != end &&
                 (entry = *iter) &&
                 entry->guessedNonWrappedSequenceNumber <= largestSequenceNumberInSet);
        
        ASSERT(bufPos == bufSize);
        
        // This message set has now been processed and can be safely
        // removed from _messageSets (not doing this is effectively
        // a memory leak)
        _messageSets.removeMessageAndOlderMessages(channelQualifier,
                                                   largestSequenceNumberInSet);
        
        return iter;
    }
    
    void flushBuffer(EmiTimeInterval now,
                     EmiChannelQualifier channelQualifier,
                     EmiNonWrappingSequenceNumber expectedSequenceNumber) {
        if (_tree.empty()) return;
        
        Entry mockEntry;
        mockEntry.guessedNonWrappedSequenceNumber = 0;
        mockEntry.header.channelQualifier = channelQualifier;
        
        BufferTreeIter iter = _tree.lower_bound(&mockEntry);
        BufferTreeIter end = _tree.end();
        
        typedef std::vector<Entry *>           EntryVector;
        typedef typename EntryVector::iterator EntryVectorIter;
        // The iterator gets messed up if we modify _tree from
        // within the loop, so we store changes in an intermediary
        // vector and perform the mutations after we're done with
        // iterating _tree.
        EntryVector toBeRemoved;
        
        Entry *entry;
        while (iter != end &&
               (entry = *iter) &&
               entry->header.channelQualifier == channelQualifier &&
               entry->guessedNonWrappedSequenceNumber <= expectedSequenceNumber) {
            
            // Search for a message set that contains entry->guessedNonWrappedSequenceNumber
            typename DisjointMessageSets::MessageData messageData;
            messageData = _messageSets.getMessageData(channelQualifier,
                                                     entry->guessedNonWrappedSequenceNumber,
                                                     entry->header.flags,
                                                     /*messageSize:*/entry->header.length);
            bool setIsComplete = messageData.first;
            EmiNonWrappingSequenceNumber largestSequenceNumberInSet = messageData.second.first;
            size_t totalSizeOfSet = messageData.second.second;
            
            // Update the expected sequence number to the largest sequence
            // number in the set, and enqueue an acknowledgment to the other
            // host that we have received all data up to that point.
            expectedSequenceNumber = largestSequenceNumberInSet+1;
            _otherHostSequenceMemo[channelQualifier] = largestSequenceNumberInSet+1;
            _receiver.enqueueAck(channelQualifier, largestSequenceNumberInSet & EMI_HEADER_SEQUENCE_NUMBER_MASK);
            
            // If the set is not complete, we can not continue from here;
            // we need to wait for it to be completed.
            if (!setIsComplete) {
                break;
            }
            
            if (largestSequenceNumberInSet == entry->guessedNonWrappedSequenceNumber) {
                // The message set consists of just one message
                //
                // This special case is purely an optimization, to avoid
                // the overhead of allocating a new TemporaryData buffer
                // when it's possible to just use entry->data right away.
                
                _receiver.emitMessage(channelQualifier,
                                      Binding::castToTemporary(entry->data),
                                      /*offset:*/0,
                                      entry->header.length);
                
                toBeRemoved.push_back(entry);
                ++iter;
            }
            else {
                // The message set contains more than one message
                
                TemporaryData mergedData = Binding::makeTemporaryData(totalSizeOfSet);
                
                iter = processMessageSetData(channelQualifier,
                                             largestSequenceNumberInSet,
                                             iter, end,
                                             toBeRemoved,
                                             /*buf:*/Binding::extractData(mergedData),
                                             /*bufSize:*/Binding::extractLength(mergedData));
                
                _receiver.emitMessage(channelQualifier,
                                      mergedData,
                                      /*offset:*/0,
                                      Binding::extractLength(mergedData));
            }
            
        }
        
        EntryVectorIter viter = toBeRemoved.begin();
        EntryVectorIter vend  = toBeRemoved.end();
        while (viter != vend) {
            remove(*viter);
            ++viter;
        }
    }
    
public:
    
    EmiReceiverBuffer(size_t size, Receiver &receiver) :
    _size(size), _bufferSize(0), _receiver(receiver) {}
    
    virtual ~EmiReceiverBuffer() {
        BufferTreeIter iter = _tree.begin();
        while (iter != _tree.end()) {
            remove(*iter);
            
            // We can't just increment iter, because the call to
            // remove invalidated iter.
            iter = _tree.begin();
        }
        
        _size = 0;
        _bufferSize = 0;
    }
    
#define EMI_GOT_INVALID_PACKET(err) do { /* NSLog(err); */ return false; } while (1)
    bool gotMessage(EmiTimeInterval now,
                    const EmiMessageHeader& header,
                    const TemporaryData& data, size_t offset) {
        EmiChannelQualifier channelQualifier = header.channelQualifier;
        EmiChannelType channelType = EMI_CHANNEL_QUALIFIER_TYPE(channelQualifier);
        
        EmiNonWrappingSequenceNumber expectedSn = expectedSequenceNumber(header);
        
        // Based on the sequence number that we expect, try to guess the
        // non-wrapped sequence number.
        EmiNonWrappingSequenceNumber guessedNonWrappedSequenceNumber;
        {
            // positive diff means older than expected
            int32_t diff = EmiNetUtil::cyclicDifferenceSigned<EMI_HEADER_SEQUENCE_NUMBER_LENGTH>(expectedSn & EMI_HEADER_SEQUENCE_NUMBER_MASK,
                                                                                                 header.sequenceNumber);
            if (diff > 0 && diff > expectedSn) {
                // Don't allow a negative guessedNonWrappedSequenceNumber
                guessedNonWrappedSequenceNumber = header.sequenceNumber;
            }
            else {
                guessedNonWrappedSequenceNumber = expectedSn - diff;
            }
        }
        
        if ((EMI_CHANNEL_TYPE_UNRELIABLE_SEQUENCED == channelType ||
             EMI_CHANNEL_TYPE_RELIABLE_SEQUENCED   == channelType) &&
            -1 != header.sequenceNumber) {
            
            _otherHostSequenceMemo[header.channelQualifier] = std::max(expectedSn, guessedNonWrappedSequenceNumber+1);
            
            int64_t snDiff = (int64_t)expectedSn - (int64_t)guessedNonWrappedSequenceNumber;
            
            if (snDiff > 0) {
                // The packet arrived out of order; drop it
                return false;
            }
            else if (snDiff < 0) {
                // We have lost one or more packets.
                _receiver.emitPacketLoss(channelQualifier, -snDiff);
            }
        }
        
        if (EMI_CHANNEL_TYPE_UNRELIABLE == channelType || EMI_CHANNEL_TYPE_UNRELIABLE_SEQUENCED == channelType) {
            if (header.flags & EMI_ACK_FLAG) EMI_GOT_INVALID_PACKET("Got unreliable message with ACK flag");
            if (header.flags & EMI_SACK_FLAG) EMI_GOT_INVALID_PACKET("Got unreliable message with SACK flag");
            
            if (0 == header.length) {
                // Unreliable packets with zero header length are nonsensical
                return false;
            }
            
            _receiver.emitMessage(channelQualifier, data, offset, header.length);
        }
        else if (EMI_CHANNEL_TYPE_RELIABLE_SEQUENCED == channelType) {
            if (header.flags & EMI_SACK_FLAG) EMI_GOT_INVALID_PACKET("SACK does not make sense on RELIABLE_SEQUENCED channels");
            
            _receiver.enqueueAck(channelQualifier, header.sequenceNumber);
            
            if (header.flags & EMI_ACK_FLAG) {
                _receiver.gotReliableSequencedAck(now, channelQualifier, header.ack);
            }
            
            // A packet with zero length indicates that it is just an ACK packet
            if (0 != header.length) {
                size_t realOffset = offset + (header.flags & EMI_ACK_FLAG ? 2 : 0);
                _receiver.emitMessage(channelQualifier, data, realOffset, header.length);
            }
        }
        else if (EMI_CHANNEL_TYPE_RELIABLE_ORDERED == channelType) {
            if (header.flags & EMI_SACK_FLAG) EMI_GOT_INVALID_PACKET("SACK is not implemented");
            
            if (header.flags & EMI_ACK_FLAG) {
                EmiNonWrappingSequenceNumber nonWrappedAck = _receiver.guessSequenceNumberWrapping(channelQualifier, header.ack);
                _receiver.deregisterReliableMessages(now, channelQualifier, nonWrappedAck);
            }
            
            if (-1 != header.sequenceNumber) {
                ASSERT(0 != header.length);
                
                int64_t seqDiff = (int64_t)expectedSn - (int64_t)guessedNonWrappedSequenceNumber;
                
                if (seqDiff >= 0) {
                    // Send an ACK only if the received message's sequence number is
                    // what we were expecting or if if it was older than we expected.
                    _receiver.enqueueAck(channelQualifier, header.sequenceNumber);
                }
                
                if (0 == seqDiff && 0 == (header.flags & (EMI_SPLIT_NOT_FIRST_FLAG | EMI_SPLIT_NOT_LAST_FLAG))) {
                    // This is purely an optimization.
                    //
                    // When we receive a message that is not split, and that has
                    // the expected sequence number, we can bypass the buffering
                    // mechanism and emit it immediately, without touching the
                    // message split mechanism.
                    
                    EmiSequenceNumber newExpectedSn = expectedSn+1;
                    _otherHostSequenceMemo[channelQualifier] = newExpectedSn;
                    
                    _receiver.emitMessage(channelQualifier, data, offset, header.length);
                    
                    // The connection might have been closed when invoking emitMessage
                    if (!_receiver.isClosed()) {
                        flushBuffer(now, channelQualifier, newExpectedSn);
                    }
                }
                else if (seqDiff <= 0) {
                    bufferMessage(guessedNonWrappedSequenceNumber,
                                  header, data, offset, header.length);
                    flushBuffer(now, channelQualifier, expectedSn);
                }
            }
        }
        else {
            EMI_GOT_INVALID_PACKET("Unknown channel type");
        }
        
        return true;
    }
#undef EMI_GOT_INVALID_PACKET
};

#endif
