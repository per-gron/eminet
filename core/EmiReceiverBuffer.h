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
    
    typedef std::map<EmiChannelQualifier, EmiSequenceNumber> EmiSequenceNumberMemo;
    
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
        
        struct DisjointSet {
            EmiSequenceNumber parent;
            unsigned rank;
            int32_t firstMessage; // Represents a sequence number. -1 means no value
            int32_t lastMessage;  // Represents a sequence number. -1 means no value
            
            DisjointSet(EmiSequenceNumber i) :
            parent(i),
            rank(0),
            firstMessage(-1),
            lastMessage(-1) { }
        };
        
        typedef std::pair<EmiChannelQualifier, EmiSequenceNumber> ForestKey;
        
        struct ForestCmp {
            inline bool operator()(const ForestKey& a, const ForestKey& b) const {
                return (a.first  != b.first ?
                        a.first  <  b.first :
                        a.second <  b.second);
            }
        };
        
        typedef std::map<ForestKey, DisjointSet, ForestCmp> ForestMap;
        typedef typename ForestMap::iterator ForestIterator;
        
        ForestMap _forest;
        
        typedef std::pair<ForestKey, DisjointSet&> FindResult;
        
        FindResult find(EmiChannelQualifier cq, EmiSequenceNumber sn) {
            ForestKey key(cq, sn);
            
            // If _forest does not contain the message, this will automatically
            // create insert a default-constructed DisjointSet object, which is
            // exactly what we want.
            DisjointSet& ds = _forest[key];
            
            if (ds.parent == sn) {
                return std::make_pair(key, ds);
            }
            else {
                FindResult result = find(ds.parent);
                ds.parent = result.first;
                return result;
            }
        }
        
        // Returns the new root
        FindResult merge(EmiChannelQualifier cq, EmiSequenceNumber i, EmiSequenceNumber j) {
            FindResult findIResult = find(cq, i);
            FindResult findJResult = find(cq, j);
            
            EmiSequenceNumber rootISn = findIResult.first.second;
            EmiSequenceNumber rootJSn = findJResult.first.second;
            
            DisjointSet& rootI = findIResult.last;
            DisjointSet& rootJ = findJResult.last;
            
            if (rootISn != rootJSn) {
                bool iIsTheNewRoot = rootI.rank > rootJ.rank;
                
                EmiSequenceNumber newRootSn = (iIsTheNewRoot ? rootISn : rootJSn);
                
                DisjointSet& newRoot = (iIsTheNewRoot ? rootI : rootJ);
                DisjointSet& nonRoot = (iIsTheNewRoot ? rootJ : rootI);
                
                if (rootI.rank == rootJ.rank) {
                    newRoot.rank += 1;
                }
                
                nonRoot.parent = newRootSn;
                
                if (-1 == newRoot.firstMessage) {
                    newRoot.firstMessage = nonRoot.firstMessage;
                }
                if (-1 == newRoot.lastMessage) {
                    newRoot.lastMessage = nonRoot.lastMessage;
                }
                
                return (iIsTheNewRoot ? findIResult : findJResult);
            }
            else {
                return findIResult;
            }
        }
        
    public:
        // When a full set of split messages have been received, this method
        // returns the first message in the set's sequence number. Otherwise,
        // it returns -1.
        int32_t gotMessage(EmiChannelQualifier cq, EmiSequenceNumber i,
                           bool first, bool last) {
            if (first && last) {
                // This is a non-split message.
                return i;
            }
            
            // Merge the message with sequence number i with the following
            // message, but only for messages that are not last in a split.
            DisjointSet& root = (last ?
                                 find(cq, i).last :
                                 merge(cq, i, (i+1) & EMI_HEADER_SEQUENCE_NUMBER_MASK).last);
            
            if (first) root.firstMessage = i;
            if (last)  root.lastMessage = i;
            
            if (-1 != root.firstMessage && -1 != root.lastMessage) {
                return root.firstMessage;
            }
            else {
                return -1;
            }
        }
        
        // Removes a message from the disjoint sets data structure.
        // The sequence number parameter can be the sequence number
        // of any message that is in the split group to be removed.
        //
        // Note: This method must only be used for messages that are
        // complete, that is, all parts of the split group have been
        // registered.
        void removeMessage(EmiChannelQualifier cq, EmiSequenceNumber i) {
            DisjointSet& root = find(cq, i);
            ASSERT(-1 != root.firstMessage && -1 != root.lastMessage);
            _forest.erase(_forest.find(ForestKey(cq, root.firstMessage)),
                          _forest.find(ForestKey(cq, root.lastMessage)));
        }
        
        void clearChannel(EmiChannelQualifier cq) {
            _forest.erase(_forest.lower_bound(ForestKey(cq, 0)),
                          _forest.upper_bound(ForestKey(cq, -1 & EMI_HEADER_SEQUENCE_NUMBER_MASK)));
        }
    };

    
public:
    class Entry {
    private:
        // Private copy constructor and assignment operator
        inline Entry(const Entry& other);
        inline Entry& operator=(const Entry& other);
        
    public:
        Entry(const EmiMessageHeader& header_, const TemporaryData &data_, size_t offset, size_t length) :
        header(header_),
        data(Binding::makePersistentData(Binding::extractData(data_)+offset, length)) {}
        Entry() {}
        ~Entry() {
            Binding::releasePersistentData(data);
        }
        
        EmiMessageHeader header;
        PersistentData data;
    };
    
protected:
    
    struct EmiReceiverBufferTreeCmp {
        bool operator()(Entry *a, Entry *b) const {
            EmiChannelQualifier acq = a->header.channelQualifier;
            EmiChannelQualifier bcq = b->header.channelQualifier;
            
            if (acq < bcq) return true;
            else if (acq > bcq) return false;
            else {
                return EmiNetUtil::cyclicDifference24Signed(a->header.sequenceNumber, b->header.sequenceNumber) < 0;
            }
        }
    };
    
    typedef std::set<Entry *, EmiReceiverBufferTreeCmp> EmiReceiverBufferTree;
    typedef typename std::set<Entry *, EmiReceiverBufferTreeCmp>::iterator EmiReceiverBufferTreeIter;
    // Buffer max size
    size_t _size;
    
    EmiReceiverBufferTree _tree;
    size_t _bufferSize;
    
    EmiSequenceNumberMemo _otherHostSequenceMemo;
    
    Receiver &_receiver;
    
private:
    // Private copy constructor and assignment operator
    inline EmiReceiverBuffer(const EmiReceiverBuffer& other);
    inline EmiReceiverBuffer& operator=(const EmiReceiverBuffer& other);
    
    static size_t bufferEntrySize(Entry *entry) {
        return entry->header.headerLength + entry->header.length;
    }
    
    EmiSequenceNumber expectedSequenceNumber(const EmiMessageHeader& header) {
        EmiSequenceNumberMemo::iterator cur = _otherHostSequenceMemo.find(header.channelQualifier);
        EmiSequenceNumberMemo::iterator end = _otherHostSequenceMemo.end();
        
        return (end == cur ? _receiver.getOtherHostInitialSequenceNumber() : (*cur).second);
    }
    
    void insert(Entry *entry) {
        size_t msgSize = EmiReceiverBuffer::bufferEntrySize(entry);
        
        // Discard the message if it doesn't fit in the buffer
        if (_bufferSize + msgSize <= _size) {
            bool wasInserted = _tree.insert(entry).second;
            
            // Only increment this._bufferSize if the message wasn't already in the buffer
            if (wasInserted) {
                _bufferSize += msgSize;
            }
        }
    }
    void remove(Entry *entry) {
        size_t msgSize = EmiReceiverBuffer::bufferEntrySize(entry);
        
        size_t numErasedElements = _tree.erase(entry);
        if (0 != numErasedElements) {
            // Only decrement this._bufferSize if the message was in the buffer
            _bufferSize -= msgSize;
        }
        
        delete entry;
    }
    
    void bufferMessage(const EmiMessageHeader& header, const TemporaryData& buf, size_t offset, size_t length) {
        insert(new Entry(header, buf, offset, length));
    }
    
    void flushBuffer(EmiTimeInterval now, EmiChannelQualifier channelQualifier, EmiSequenceNumber expectedSequenceNumber) {
        if (_tree.empty()) return;
        
        Entry mockEntry;
        mockEntry.header.channelQualifier = channelQualifier;
        mockEntry.header.sequenceNumber = (expectedSequenceNumber-1) & EMI_HEADER_SEQUENCE_NUMBER_LENGTH;
        
        EmiReceiverBufferTreeIter iter = _tree.lower_bound(&mockEntry);
        EmiReceiverBufferTreeIter end = _tree.end();
        
        std::vector<Entry *> toBeRemoved;
        
        Entry *entry;
        while (iter != end &&
               (entry = *iter) &&
               entry->header.channelQualifier == channelQualifier &&
               entry->header.sequenceNumber <= expectedSequenceNumber) {
            
            // The iterator gets messed up if we modify _tree from
            // within the loop, so do it after the loop has finished.
            toBeRemoved.push_back(entry);
            
            if (entry->header.sequenceNumber == expectedSequenceNumber) {
                expectedSequenceNumber = (entry->header.sequenceNumber+1) & EMI_HEADER_SEQUENCE_NUMBER_MASK;
                
                _otherHostSequenceMemo[channelQualifier] = expectedSequenceNumber;
                
                _receiver.enqueueAck(channelQualifier, entry->header.sequenceNumber);
                _receiver.emitMessage(channelQualifier, Binding::castToTemporary(entry->data), /*offset:*/0, entry->header.length);
            }
            
            ++iter;
        }
        
        typename std::vector<Entry *>::iterator viter = toBeRemoved.begin();
        typename std::vector<Entry *>::iterator vend  = toBeRemoved.end();
        while (viter != vend) {
            remove(*viter);
            ++viter;
        }
    }
    
public:
    
    EmiReceiverBuffer(size_t size, Receiver &receiver) :
    _size(size), _bufferSize(0), _receiver(receiver) {}
    
    virtual ~EmiReceiverBuffer() {
        EmiReceiverBufferTreeIter iter = _tree.begin();
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
                    const TemporaryData &data, size_t offset) {
        EmiChannelQualifier channelQualifier = header.channelQualifier;
        EmiChannelType channelType = EMI_CHANNEL_QUALIFIER_TYPE(channelQualifier);
        
        if ((EMI_CHANNEL_TYPE_UNRELIABLE_SEQUENCED == channelType ||
             EMI_CHANNEL_TYPE_RELIABLE_SEQUENCED   == channelType) &&
            -1 != header.sequenceNumber) {
            
            EmiSequenceNumber esn = expectedSequenceNumber(header);
            
            _otherHostSequenceMemo[header.channelQualifier] =
            EmiNetUtil::cyclicMax24(esn,
                                    (header.sequenceNumber+1) & EMI_HEADER_SEQUENCE_NUMBER_MASK);
            
            int32_t snDiff = EmiNetUtil::cyclicDifference24Signed(esn, header.sequenceNumber);
            
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
                _receiver.deregisterReliableMessages(now, channelQualifier, header.ack);
            }
            
            if (-1 != header.sequenceNumber) {
                ASSERT(0 != header.length);
                
                EmiSequenceNumber expectedSn = expectedSequenceNumber(header);
                int32_t seqDiff = EmiNetUtil::cyclicDifference24Signed(expectedSn,
                                                                       header.sequenceNumber);
                
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
                    // mechanism and emit it immediately.
                    
                    EmiSequenceNumber newExpectedSn = (header.sequenceNumber+1) & EMI_HEADER_SEQUENCE_NUMBER_MASK;
                    _otherHostSequenceMemo[channelQualifier] = newExpectedSn;
                    
                    _receiver.emitMessage(channelQualifier, data, offset, header.length);
                    
                    // The connection might have been closed when invoking emitMessage
                    if (!_receiver.isClosed()) {
                        flushBuffer(now, channelQualifier, newExpectedSn);
                    }
                }
                else if (seqDiff <= 0) {
                    bufferMessage(header, data, offset, header.length);
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
