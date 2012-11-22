//
//  EmiReceiverBuffer.h
//  eminet
//
//  Created by Per Eckerdal on 2012-02-16.
//  Copyright (c) 2012 Per Eckerdal. All rights reserved.
//

#ifndef eminet_EmiReceiverBuffer_h
#define eminet_EmiReceiverBuffer_h

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
            EmiNonWrappingSequenceNumber firstMessage; // The smallest sequence number in this set
            EmiNonWrappingSequenceNumber lastMessage;  // The largest  sequence number in this set
            size_t size; // The total size, in bytes, of the messages in this set
            
            DisjointSet(EmiNonWrappingSequenceNumber i) :
            parent(i),
            rank(0),
            flags(0),
            firstMessage(EMI_NON_WRAPPING_SEQUENCE_NUMBER_MAX),
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
        
        FindResult find(EmiChannelQualifier cq,
                        EmiNonWrappingSequenceNumber sn,
                        bool createIfMissing = true) {
            ForestKey key(cq, sn);
            
            ForestIter iter = _forest.find(key);
            ForestIter end  = _forest.end();
            if (iter == end) {
                if (!createIfMissing) {
                    return std::make_pair(key, (DisjointSet *)NULL);
                }
                
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
                newRoot.firstMessage = std::min(newRoot.firstMessage,
                                                nonRoot.firstMessage);
                newRoot.lastMessage  = std::max(newRoot.lastMessage,
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
            
            root.firstMessage = std::min(i, root.firstMessage);
            root.lastMessage  = std::max(i, root.lastMessage);
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
        // Note: This method must only be used for messages that are
        // complete, that is, all parts of the split group have been
        // registered.
        void removeMessageAndOlderMessages(EmiChannelQualifier cq, EmiNonWrappingSequenceNumber i) {
            DisjointSet *root = find(cq, i, /*createIfMissing:*/false).second;
            
            ASSERT(!root ||
                   ((root->flags & DISJOINT_SET_HAS_FIRST_MESSAGE) &&
                    (root->flags & DISJOINT_SET_HAS_LAST_MESSAGE)));
            
            _forest.erase(_forest.lower_bound(ForestKey(cq, 0)),
                          _forest.upper_bound(ForestKey(cq, root ? root->lastMessage : i)));
        }
        
        EmiNonWrappingSequenceNumber getLastSequenceNumberInSet(EmiChannelQualifier cq,
                                                                EmiNonWrappingSequenceNumber i) {
            FindResult findResult(find(cq, i, /*createIfMissing:*/false));
            return (findResult.second ?
                    findResult.second->lastMessage :
                    i);
        }
        
        EmiNonWrappingSequenceNumber getFirstSequenceNumberInSet(EmiChannelQualifier cq,
                                                                 EmiNonWrappingSequenceNumber i) {
            FindResult findResult(find(cq, i, /*createIfMissing:*/false));
            return (findResult.second ?
                    findResult.second->firstMessage :
                    i);
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
            
            return (acq != bcq ?
                    acq < bcq :
                    a->guessedNonWrappedSequenceNumber < b->guessedNonWrappedSequenceNumber);
        }
    };
    
    typedef std::set<Entry *, BufferTreeCmp> BufferTree;
    typedef typename BufferTree::iterator    BufferTreeIter;
    // Buffer max size
    size_t _size;
    
    DisjointMessageSets _messageSets;
    BufferTree _tree;
    size_t _bufferSize;
    
    // This is a map that contains the next message's expected
    // sequence number.
    //
    // For RELIABLE_SEQUENCED channels, this map contains not the
    // next message's expected sequence number, but a sequence
    // number in the split group (the disjoint set that contains the
    // first message of the group) of the next expected message.
    //
    // The reason for this seemingly odd thing for those channels
    // is to be able to know which ACK to send when receiving split
    // messages:
    //
    // The ack to be sent on each received reliable sequenced
    // message is the newest sequence number of the message set that
    // contains the most recently received message that is the first
    // part of a split.
    //
    // To ensure that no ack is ever sent that is smaller than a
    // previously sent ack, _expectedSnMemo is updated to be the
    // value of a sent ack whenever an ack is sent. (This applies
    // only for RELIABLE_SEQUENCED channels) This also minimizes
    // the negative impact on the sequence number wrapping guessing
    // algorithm, which also uses _expectedSnMemo.
    EmiNonWrappingSequenceNumberMemo _expectedSnMemo;
    
    Receiver &_receiver;
    
private:
    // Private copy constructor and assignment operator
    inline EmiReceiverBuffer(const EmiReceiverBuffer& other);
    inline EmiReceiverBuffer& operator=(const EmiReceiverBuffer& other);
    
    static size_t bufferEntrySize(size_t headerLength, size_t length) {
        return headerLength + length;
    }
    
    EmiNonWrappingSequenceNumber expectedSequenceNumber(const EmiMessageHeader& header) {
        EmiNonWrappingSequenceNumberMemo::iterator cur = _expectedSnMemo.find(header.channelQualifier);
        EmiNonWrappingSequenceNumberMemo::iterator end = _expectedSnMemo.end();
        
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
    
    void remove(BufferTreeIter begin, BufferTreeIter end) {
        BufferTreeIter iter = begin;
        while (iter != end) {
            Entry *entry = *iter;
            
            _bufferSize -= EmiReceiverBuffer::bufferEntrySize(entry->header.headerLength,
                                                              entry->header.length);
            
            delete entry;
            
            ++iter;
        }
        
        _tree.erase(begin, end);
    }
    
    // Processes a set of messages in a split that is known to be complete.
    // This method iterates through the messages and fills buf so that it
    // is a continuous buffer of the data of the messages.
    //
    // iter must point to the first Entry of the message set.
    //
    // The return value is an iterator pointing to the first entry that is
    // in the buffer that is not part of the message set. Note that the
    // return value can be _tree.end() or an iterator to a message with a
    // different channelQualifier.
    BufferTreeIter processMessageSetData(EmiChannelQualifier channelQualifier,
                                         EmiNonWrappingSequenceNumber largestSequenceNumberInSet,
                                         BufferTreeIter iter,
                                         uint8_t *buf, size_t bufSize) {
        BufferTreeIter end = _tree.end();
        Entry *entry = *iter;
        size_t bufPos = 0;
        
        // This loop increments iter until we are past the message set
        // we just processed, and marks all messages we pass for removal.
        do {
            size_t edlen = Binding::extractLength(entry->data);
            ASSERT(bufPos + edlen <= bufSize);
            memcpy(buf+bufPos, Binding::extractData(entry->data), edlen);
            bufPos += edlen;
            
            ++iter;
        } while (iter != end &&
                 (entry = *iter) &&
                 entry->guessedNonWrappedSequenceNumber <= largestSequenceNumberInSet);
        
        ASSERT(bufPos == bufSize);
        
        return iter;
    }
    
    // This method iterates through the buffer and emits as many
    // complete messages as it can find, while still enforcing
    // strict message ordering (no skipped messages).
    //
    // The search begins in iter.
    //
    // The return value is an iterator that points to the message
    // after the last one that was processed by processBuffer. Note
    // that the return value can be _tree.end() or an iterator to a
    // message with a different channelQualifier.
    BufferTreeIter processBuffer(EmiChannelQualifier channelQualifier,
                                 BufferTreeIter iter,
                                 int64_t *largestProcessedMessageSet,
                                 EmiNonWrappingSequenceNumber *largestProcessedSn) {
        
        if (largestProcessedMessageSet) {
            *largestProcessedMessageSet = -1;
        }
        if (largestProcessedSn) {
            *largestProcessedSn = -1;
        }
        
        BufferTreeIter end = _tree.end();
        
        if (iter == end) {
            return iter;
        }
        
        Entry *entry = *iter;
        
        if (entry->header.flags & EMI_SPLIT_NOT_FIRST_FLAG) {
            // The first message is in the middle of a split.
            // That means that there is no complete message that
            // we can process.
            //
            // This check is needed in order to ensure that we
            // don't invoke processMessageSetData with an
            // incomplete message set.
            return iter;
        }
        
        EmiNonWrappingSequenceNumber expectedSequenceNumber = entry->guessedNonWrappedSequenceNumber;
        
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
            if (largestProcessedSn) {
                *largestProcessedSn = largestSequenceNumberInSet;
            }
            
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
                
                ++iter;
            }
            else {
                // The message set contains more than one message
                
                uint8_t *mergedDataBuf;
                TemporaryData mergedData = Binding::makeTemporaryData(totalSizeOfSet, &mergedDataBuf);
                
                iter = processMessageSetData(channelQualifier,
                                             largestSequenceNumberInSet,
                                             iter,
                                             /*buf:*/mergedDataBuf,
                                             /*bufSize:*/Binding::extractLength(mergedData));
                
                _receiver.emitMessage(channelQualifier,
                                      mergedData,
                                      /*offset:*/0,
                                      Binding::extractLength(mergedData));
            }
            
            if (largestProcessedMessageSet) {
                *largestProcessedMessageSet = largestSequenceNumberInSet;
            }
        }
        
        return iter;
    }
    
    // This is works with RELIABLE_ORDERED channels.
    //
    // It goes through the receiver buffer, emits all messages
    // that are ready, and removes the emitted messages from the
    // buffer.
    void flushBuffer(EmiChannelQualifier channelQualifier,
                     EmiNonWrappingSequenceNumber expectedSequenceNumber) {
        if (_tree.empty()) return;
        
        Entry mockEntry;
        mockEntry.guessedNonWrappedSequenceNumber = 0;
        mockEntry.header.channelQualifier = channelQualifier;
        
        BufferTreeIter iter = _tree.lower_bound(&mockEntry);
        
        if (_tree.end() == iter) {
            // We found nothing.
            return;
        }
        
        Entry *foundEntry = *iter;
        if (foundEntry->header.channelQualifier != channelQualifier ||
            foundEntry->guessedNonWrappedSequenceNumber > expectedSequenceNumber) {
            // The entry we found was either not in the expected channel qualifier,
            // or it was newer than the newest permissible message to process.
            return;
        }
        
        int64_t largestProcessedMessageSet;
        EmiNonWrappingSequenceNumber largestProcessedSn;
        BufferTreeIter end = processBuffer(channelQualifier,
                                           iter,
                                           &largestProcessedMessageSet,
                                           &largestProcessedSn);
        
        if (-1 != largestProcessedSn) {
            _receiver.enqueueAck(channelQualifier, largestProcessedSn & EMI_HEADER_SEQUENCE_NUMBER_MASK);
            _expectedSnMemo[channelQualifier] = largestProcessedSn+1;
        }
        
        // We have now processed and emitted messages. The next
        // step is to remove those from the buffer data structure.
        //
        // To do this correctly, we need to remove data from both
        // _messageSets and _tree.
        
        if (-1 != largestProcessedMessageSet) {
            _messageSets.removeMessageAndOlderMessages(channelQualifier,
                                                       largestProcessedMessageSet);
        }
        remove(iter, end);
    }
    
    void processUnorderedMessage(EmiNonWrappingSequenceNumber guessedNonWrappedSequenceNumber,
                                 const EmiMessageHeader& header,
                                 const TemporaryData& data, size_t offset) {
        EmiChannelType channelType = EMI_CHANNEL_QUALIFIER_TYPE(header.channelQualifier);
        
        if (0 == (header.flags & (EMI_SPLIT_NOT_FIRST_FLAG | EMI_SPLIT_NOT_LAST_FLAG))) {
            // This is a non-split message, so we don't need to worry
            // about reconstructing the split etc.
            
            _receiver.emitMessage(header.channelQualifier, data, offset, header.length);
            
            // Remove older messages from _tree and _messageSets
            _messageSets.removeMessageAndOlderMessages(header.channelQualifier,
                                                       guessedNonWrappedSequenceNumber);
            
            Entry mockEntry1;
            mockEntry1.guessedNonWrappedSequenceNumber = 0;
            mockEntry1.header.channelQualifier = header.channelQualifier;
            
            Entry mockEntry2;
            mockEntry2.guessedNonWrappedSequenceNumber = guessedNonWrappedSequenceNumber;
            mockEntry2.header.channelQualifier = header.channelQualifier;
            
            remove(_tree.lower_bound(&mockEntry1),
                   _tree.upper_bound(&mockEntry2));
            
            // Enqueue ack if this is a reliable sequenced channel
            if (EMI_CHANNEL_TYPE_RELIABLE_SEQUENCED == channelType) {
                _expectedSnMemo[header.channelQualifier] = guessedNonWrappedSequenceNumber;
                _receiver.enqueueAck(header.channelQualifier, header.sequenceNumber);
            }
        }
        else {
            bufferMessage(guessedNonWrappedSequenceNumber,
                          header, data, offset, header.length);
            
            EmiNonWrappingSequenceNumber firstSequenceNumberInSet = _messageSets.getFirstSequenceNumberInSet(header.channelQualifier,
                                                                                                             guessedNonWrappedSequenceNumber);
            
            Entry mockEntry1;
            mockEntry1.guessedNonWrappedSequenceNumber = firstSequenceNumberInSet;
            mockEntry1.header.channelQualifier = header.channelQualifier;
            BufferTreeIter iter = _tree.find(&mockEntry1);
            
            // Enqueue ack if this is a reliable sequenced channel.
            // Note that this needs to be done before we flush _messageSets.
            //
            // Also, we want to enqueue the ack before we do the sanity
            // checks that might return from the function, because the other
            // host is entitled to get an ack even if our receiver buffer is
            // full or if this happens to be a message that doesn't complete
            // a message group.
            if (EMI_CHANNEL_TYPE_RELIABLE_SEQUENCED == channelType) {
                EmiNonWrappingSequenceNumber sn = _messageSets.getLastSequenceNumberInSet(header.channelQualifier,
                                                                                          _expectedSnMemo[header.channelQualifier]);
                _expectedSnMemo[header.channelQualifier] = sn;
                _receiver.enqueueAck(header.channelQualifier, sn & EMI_HEADER_SEQUENCE_NUMBER_MASK);
            }
            
            if (_tree.end() == iter) {
                // This happens when the receiver buffer is full. Fail.
                return;
            }
            
            if ((*iter)->header.flags & EMI_SPLIT_NOT_FIRST_FLAG) {
                // The message we found is not the first in the set.
                // This means that the set is not complete. Fail.
                //
                // In fact, it is really important to not continue
                // here, because otherwise we would be removing
                // parts of still-in-use message groups later on.
                return;
            }
            
            int64_t largestProcessedMessageSet;
            
            BufferTreeIter processEnd = processBuffer(header.channelQualifier,
                                                      iter,
                                                      &largestProcessedMessageSet,
                                                      /*largestProcessedSn:*/NULL);
            
            // Remove processed and older messages from _tree and _messageSets
            if (-1 != largestProcessedMessageSet) {
                _messageSets.removeMessageAndOlderMessages(header.channelQualifier,
                                                           largestProcessedMessageSet);
            }
            
            Entry mockEntry2;
            mockEntry2.guessedNonWrappedSequenceNumber = 0;
            mockEntry2.header.channelQualifier = header.channelQualifier;
            BufferTreeIter removeFrom = _tree.lower_bound(&mockEntry2);
            
            remove(removeFrom, processEnd);
        }
    }
    
public:
    
    EmiReceiverBuffer(size_t size, Receiver &receiver) :
    _size(size), _bufferSize(0), _receiver(receiver) {}
    
    virtual ~EmiReceiverBuffer() {
        remove(_tree.begin(), _tree.end());
        
        _size = 0;
        _bufferSize = 0;
    }
    
#define EMI_GOT_INVALID_MESSAGE(err) do { /* NSLog(err); */ return false; } while (1)
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
            
            if (EMI_CHANNEL_TYPE_UNRELIABLE_SEQUENCED == channelType ||
                !(header.flags & EMI_SPLIT_NOT_FIRST_FLAG)) {
                // For reliable sequenced channels, only ever set
                // _expectedSnMemo to sequence numbers for messages
                // that are the first part in their split group.
                //
                // For why we're doing this, refer to the comment
                // above the declaration of _expectedSnMemo
                
                _expectedSnMemo[channelQualifier] = std::max(expectedSn,
                                                             guessedNonWrappedSequenceNumber+1);
            }
            
            int64_t snDiff = (int64_t)expectedSn - (int64_t)guessedNonWrappedSequenceNumber;
            
            if (snDiff > 0) {
                // The message arrived out of order; drop it
                return false;
            }
            
            // Packet loss notification only really make sense for
            // unreliable sequenced channels, since lost packets will
            // sometimes be resent on reliable channels (this happens
            // when messages are split).
            if (snDiff < 0 &&
                EMI_CHANNEL_TYPE_UNRELIABLE_SEQUENCED == channelType) {
                
                // We have lost one or more messages.
                _receiver.emitPacketLoss(channelQualifier, -snDiff);
            }
        }
        
        if (EMI_CHANNEL_TYPE_UNRELIABLE == channelType ||
            EMI_CHANNEL_TYPE_UNRELIABLE_SEQUENCED == channelType ||
            EMI_CHANNEL_TYPE_RELIABLE_SEQUENCED == channelType) {
            
            if (header.flags & EMI_SACK_FLAG) EMI_GOT_INVALID_MESSAGE("Got unreliable message with SACK flag");
            
            if (header.flags & EMI_ACK_FLAG) {
                if (EMI_CHANNEL_TYPE_RELIABLE_SEQUENCED == channelType) {
                    _receiver.gotReliableSequencedAck(now, channelQualifier, header.ack);
                }
                else {
                    EMI_GOT_INVALID_MESSAGE("Got unreliable message with ACK flag");
                }
            }
            
            if (0 != header.length) {
                processUnorderedMessage(guessedNonWrappedSequenceNumber,
                                        header,
                                        data, offset);
            }
        }
        else if (EMI_CHANNEL_TYPE_RELIABLE_ORDERED == channelType) {
            if (header.flags & EMI_SACK_FLAG) EMI_GOT_INVALID_MESSAGE("SACK is not implemented");
            
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
                    
                    _receiver.enqueueAck(channelQualifier,
                                         (0 == seqDiff ?
                                          guessedNonWrappedSequenceNumber :
                                          expectedSn-1) & EMI_HEADER_SEQUENCE_NUMBER_MASK);
                }
                
                if (0 == seqDiff && 0 == (header.flags & (EMI_SPLIT_NOT_FIRST_FLAG | EMI_SPLIT_NOT_LAST_FLAG))) {
                    // This is purely an optimization.
                    //
                    // When we receive a message that is not split, and that has
                    // the expected sequence number, we can bypass the buffering
                    // mechanism and emit it immediately, without touching the
                    // message split mechanism.
                    
                    EmiSequenceNumber newExpectedSn = expectedSn+1;
                    _expectedSnMemo[channelQualifier] = newExpectedSn;
                    
                    _receiver.emitMessage(channelQualifier, data, offset, header.length);
                    
                    // The connection might have been closed when invoking emitMessage
                    if (!_receiver.isClosed()) {
                        flushBuffer(channelQualifier, newExpectedSn);
                    }
                }
                else if (seqDiff <= 0) {
                    bufferMessage(guessedNonWrappedSequenceNumber,
                                  header, data, offset, header.length);
                    flushBuffer(channelQualifier, expectedSn);
                }
            }
        }
        else {
            ASSERT(0 && "Unknown channel type");
        }
        
        return true;
    }
#undef EMI_GOT_INVALID_MESSAGE
};

#endif
