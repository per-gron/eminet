//
//  EmiLossList.cc
//  rock
//
//  Created by Per Eckerdal on 2012-05-23.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#include "EmiLossList.h"

EmiLossList::EmiLossList() :
_newestSequenceNumber(-1),
_newestSequenceNumberTime(0),
_lossSet() {}

EmiLossList::~EmiLossList() {}

void EmiLossList::gotPacket(EmiTimeInterval now, EmiPacketSequenceNumber sequenceNumber) {
    if (-1 == _newestSequenceNumber ||
        _newestSequenceNumber+1 == sequenceNumber) {
        // We received the expected sequence number.
        // No need to do anything with the loss set.
        _newestSequenceNumber = sequenceNumber;
        _newestSequenceNumberTime = now;
    }
    else if (_newestSequenceNumber > sequenceNumber) {
        // We received an old sequence number, which presumably
        // arrived out of order. Remove it from the loss set if
        // it's present.
        
        LossSetIter iter = _lossSet.lower_bound(LostPacketRange(0, 0, sequenceNumber));
        
        if (_lossSet.end() == iter) {
            // We did not find any matching LostPacketRange
            return;
        }
        
        const LostPacketRange& lpr(*iter);
        
        if (lpr.oldestSequenceNumber > sequenceNumber) {
            // The LostPacketRange we found doesn't contain this sequence number
            return;
        }
        
        // We only have a reference to lp, so we must use it before
        // we erase it from _lossSet.
        LostPacketRange lowerBound(lpr);
        lowerBound.newestSequenceNumber = sequenceNumber-1;
        LostPacketRange upperBound(lpr);
        upperBound.oldestSequenceNumber = sequenceNumber+1;
        upperBound.numFeedbacks = 0;
        
        // To make sure that we don't get problems with duplicates in the set,
        // we must erase lp before we insert the split versions of it.
        _lossSet.erase(iter);
        
        if (lowerBound.oldestSequenceNumber <= lowerBound.newestSequenceNumber) {
            _lossSet.insert(lowerBound);
        }
        if (upperBound.oldestSequenceNumber <= upperBound.newestSequenceNumber) {
            _lossSet.insert(upperBound);
        }
    }
    else if (sequenceNumber-_newestSequenceNumber > 1) {
        // We received a newer sequence number than what we
        // expected. Add the lost range to the loss set.
        
        LostPacketRange lpr(now, _newestSequenceNumber+1, sequenceNumber-1);
        _lossSet.insert(lpr);
    }
}

EmiPacketSequenceNumber EmiLossList::calculateNak(EmiTimeInterval now, EmiTimeInterval rtt) {
    LossSetRIter iter = _lossSet.rbegin();
    LossSetRIter end  = _lossSet.rend();
    
    while (iter != end) {
        const LostPacketRange& lpr(*iter);
        
        if (lpr.lastFeedbackTime + rtt*(2+lpr.numFeedbacks) > now) {
            // Bingo! We found the LostPacket we wanted.
            
            // Remove all older LostPackets in _lossSet
            ++iter;
            if (iter != end) {
                _lossSet.erase(_lossSet.begin(), _lossSet.find(*iter));
            }
            
            // Replace lp with a new object with incremented numFeedbacks
            LostPacketRange lprWithIncrementedNumFeedbacks(lpr);
            lprWithIncrementedNumFeedbacks.numFeedbacks++;
            _lossSet.erase(lpr);
            _lossSet.insert(lprWithIncrementedNumFeedbacks);
            
            // Exit the loop early and return the sequence number we found
            return lprWithIncrementedNumFeedbacks.oldestSequenceNumber;
        }
        
        ++iter;
    }
    
    // We did not find any applicable packet.
    return -1;
}
