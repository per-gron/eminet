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

#include <set>
#include <vector>

template<class Data, class Receiver>
class EmiReceiverBuffer {
public:
    class Entry {
    public:
        Entry(const Data &data_) : data(data_) {}
        Entry() {}
        
        EmiMessageHeader header;
        Data data;
        size_t offset;
    };
    
protected:
    
    struct EmiReceiverBufferTreeCmp {
        bool operator()(Entry *a, Entry *b) const {
            EmiChannelQualifier acq = a->header.channelQualifier;
            EmiChannelQualifier bcq = b->header.channelQualifier;
            
            if (acq < bcq) return true;
            else if (acq > bcq) return false;
            else {
                return emiCyclicDifference16Signed(a->header.sequenceNumber, b->header.sequenceNumber) < 0;
            }
        }
    };
    
    typedef std::set<Entry *, EmiReceiverBufferTreeCmp> EmiReceiverBufferTree;
    typedef typename std::set<Entry *, EmiReceiverBufferTreeCmp>::iterator EmiReceiverBufferTreeIter;
    // Buffer max size
    size_t _size;
    
    EmiReceiverBufferTree _tree;
    size_t _bufferSize;
    
    Receiver &_receiver;
    
private:
    // Private copy constructor and assignment operator
    inline EmiReceiverBuffer(const EmiReceiverBuffer& other);
    inline EmiReceiverBuffer& operator=(const EmiReceiverBuffer& other);
    
    static size_t bufferEntrySize(Entry *entry) {
        return entry->header.headerLength + entry->header.length;
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
    
public:
    
    EmiReceiverBuffer(size_t size, Receiver &receiver) :
    _size(size), _bufferSize(0), _receiver(receiver) {}
    
    virtual ~EmiReceiverBuffer() {
        EmiReceiverBufferTreeIter iter = _tree.begin();
        EmiReceiverBufferTreeIter end = _tree.end();
        while (iter != end) {
            remove(*iter);
            ++iter;
        }
        
        _size = 0;
        _bufferSize = 0;
    }
    
    void bufferMessage(EmiMessageHeader *header, const Data& buf, int offset) {
        Entry *entry = new Entry(buf);
        memcpy(&entry->header, header, sizeof(EmiMessageHeader));
        entry->offset = offset;
        
        insert(entry);
    }
    void flushBuffer(EmiChannelQualifier channelQualifier, EmiSequenceNumber sequenceNumber) {
        if (_tree.empty()) return;
        
        Entry mockEntry;
        mockEntry.header.channelQualifier = channelQualifier;
        mockEntry.header.sequenceNumber = sequenceNumber;
        
        EmiReceiverBufferTreeIter iter = _tree.lower_bound(&mockEntry);
        EmiReceiverBufferTreeIter end = _tree.end();
        
        std::vector<Entry *> toBeRemoved;
        EmiSequenceNumber expectedSequenceNumber = sequenceNumber+1;
        
        Entry *entry;
        while (iter != end &&
               (entry = *iter) &&
               entry->header.channelQualifier == channelQualifier &&
               entry->header.sequenceNumber <= expectedSequenceNumber) {
            
            // The iterator gets messed up if we modify _tree from
            // within the loop, so do it after the loop has finished.
            toBeRemoved.push_back(entry);
            
            if (entry->header.sequenceNumber == expectedSequenceNumber) {
                _receiver.gotReceiverBufferMessage(entry);
                expectedSequenceNumber++;
            }
            
            ++iter;
        }
        
        typename std::vector<Entry *>::iterator viter = toBeRemoved.begin();
        typename std::vector<Entry *>::iterator vend = toBeRemoved.end();
        while (viter != vend) {
            remove(*viter);
            ++viter;
        }
    }
};

#endif
