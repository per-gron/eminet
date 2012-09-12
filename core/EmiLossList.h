//
//  EmiLossList.h
//  rock
//
//  Created by Per Eckerdal on 2012-05-23.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#ifndef rock_EmiLossList_h
#define rock_EmiLossList_h

#include "EmiTypes.h"
#include "EmiNetUtil.h"

#include <set>

// This class implements the logic required to know which NAKs to
// send out, if any.
//
// The NAK to send out is the oldest packet's sequence number that
// is in the newest packet range recorded to have been lost (we
// have received a newer packet) whose last feedback time is at
// least RTT*k ago, where k is initialized as 2 and increased by 1
// each time the number is fed back.
//
// Also, once a NAK has been sent, we will never send an older
// sequence number as a NAK. This allows EmiLossList to prune old
// lost packets.
class EmiLossList {
    
    struct LostPacketRange {
        LostPacketRange(EmiTimeInterval lastFeedbackTime_,
                        EmiPacketSequenceNumber oldestSequenceNumber_,
                        EmiPacketSequenceNumber newestSequenceNumber_) :
        oldestSequenceNumber(oldestSequenceNumber_),
        newestSequenceNumber(newestSequenceNumber_),
        lastFeedbackTime(lastFeedbackTime_),
        numFeedbacks(0) {}
        
        EmiPacketSequenceNumber oldestSequenceNumber;
        EmiPacketSequenceNumber newestSequenceNumber;
        EmiTimeInterval         lastFeedbackTime;
        uint32_t                numFeedbacks;
        
        inline bool operator<(const LostPacketRange& other) const {
            return newestSequenceNumber < other.newestSequenceNumber;
        }
    };
    
    typedef std::set<LostPacketRange> LossSet;
    typedef LossSet::iterator         LossSetIter;
    typedef LossSet::reverse_iterator LossSetRIter;
    
    EmiPacketSequenceNumber _newestSequenceNumber;
    EmiTimeInterval         _newestSequenceNumberTime;
    // EmiLossList maintains the invariant that any oldestSequenceNumber
    // of a LostPacketRange in the set is greater than the previous
    // LostPacketRange's newestSequenceNumber.
    LossSet                 _lossSet;
    
public:
    EmiLossList();
    virtual ~EmiLossList();
    
    // Amortized complexity of this method is at worst O(log n) where
    // n is the size of _lossSet. When all packets arrive exactly once
    // and in order, complexity is O(1).
    void gotPacket(EmiTimeInterval now, EmiPacketSequenceNumber sequenceNumber);
    
    // Should be called on NAK timeouts. Calculates the current value
    // to send as NAK. Returns -1 if no NAK should be sent.
    //
    // This method is potentially rather slow. It's intended to be
    // invoked exactly once per NAK timeout.
    //
    // Note that this method is not free of side effects; it increases
    // the numFeedbacks field of the LostPacketRange object in question.
    // It also prunes LostPacketRanges that are older than the one
    // returned.
    EmiPacketSequenceNumber calculateNak(EmiTimeInterval now, EmiTimeInterval rtt);
};

#endif
