//
//  EmiLossList.cc
//  rock
//
//  Created by Per Eckerdal on 2012-05-23.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#include "EmiLossList.h"

#include "EmiNetUtil.h"

EmiLossList::EmiLossList() :
_newestSequenceNumber(-1),
_newestSequenceNumberTime(0),
_lossSet() {}

EmiLossList::~EmiLossList() {}

void EmiLossList::gotPacket(EmiTimeInterval now, EmiPacketSequenceNumber sequenceNumber) {
    if (-1 == _newestSequenceNumber ||
        ((_newestSequenceNumber+1) & EMI_PACKET_SEQUENCE_NUMBER_MASK) == sequenceNumber) {
        // We received the expected sequence number.
        // No need to do anything with the loss set.
    }
    else if (/* Like _newestSequenceNumber > sequenceNumber, but supports sequence number cycling */
             EmiNetUtil::cyclicDifference24Signed(_newestSequenceNumber, sequenceNumber) > 0) {
        // We received an old sequence number, which presumably
        // arrived out of order. Remove it from the loss set if
        // it's present.
        
        LossSetIter iter = _lossSet.lower_bound(LostPacketRange(0, 0, sequenceNumber));
        
        if (_lossSet.end() == iter) {
            // We did not find any matching LostPacketRange
            return;
        }
        
        const LostPacketRange& lpr(*iter);
        
        if (/* Like lpr.oldestSequenceNumber > sequenceNumber, but supports sequence number cycling */
            EmiNetUtil::cyclicDifference24Signed(lpr.oldestSequenceNumber, sequenceNumber) > 0) {
            // The LostPacketRange we found doesn't contain this sequence number
            return;
        }
        
        // We only have a reference to lp, so we must use it before
        // we erase it from _lossSet.
        LostPacketRange lowerBound(lpr);
        lowerBound.newestSequenceNumber = (sequenceNumber-1) & EMI_PACKET_SEQUENCE_NUMBER_MASK;
        LostPacketRange upperBound(lpr);
        upperBound.oldestSequenceNumber = (sequenceNumber+1) & EMI_PACKET_SEQUENCE_NUMBER_MASK;
        upperBound.numFeedbacks = 0;
        
        // To make sure that we don't get problems with duplicates in the set,
        // we must erase lp before we insert the split versions of it.
        _lossSet.erase(iter);
        
        if (/* Like lowerBound.oldestSequenceNumber <= lowerBound.newestSequenceNumber,
               but supports sequence number cycling */
            EmiNetUtil::cyclicDifference24Signed(lowerBound.oldestSequenceNumber,
                                                 lowerBound.newestSequenceNumber) <= 0) {
            _lossSet.insert(lowerBound);
        }
        if (/* Like upperBound.oldestSequenceNumber <= upperBound.newestSequenceNumber,
               but supports sequence number cycling */
            EmiNetUtil::cyclicDifference24Signed(upperBound.oldestSequenceNumber,
                                                 upperBound.newestSequenceNumber) <= 0) {
            _lossSet.insert(upperBound);
        }
    }
    else if (EmiNetUtil::cyclicDifference24Signed(sequenceNumber, _newestSequenceNumber) > 1) {
        // We received a newer sequence number than what we
        // expected. Add the lost range to the loss set.
        
        LostPacketRange lpr(now, 
                            (_newestSequenceNumber+1) & EMI_PACKET_SEQUENCE_NUMBER_MASK,
                            (sequenceNumber-1) & EMI_PACKET_SEQUENCE_NUMBER_MASK);
        _lossSet.insert(lpr);
    }
    
    _newestSequenceNumber = sequenceNumber;
    _newestSequenceNumberTime = now;
}

EmiPacketSequenceNumber EmiLossList::calculateNak(EmiTimeInterval now, EmiTimeInterval rtt) {
    LossSetRIter iter = _lossSet.rbegin();
    LossSetRIter end  = _lossSet.rend();
    
    while (iter != end) {
        const LostPacketRange& lpr(*iter);
        
        if (lpr.lastFeedbackTime + rtt*(2+lpr.numFeedbacks) > now) {
            // Bingo! We found the LostPacket we wanted.
            LostPacketRange newLpr(lpr);
            newLpr.numFeedbacks++;
            newLpr.oldestSequenceNumber = (newLpr.oldestSequenceNumber+1) & EMI_PACKET_SEQUENCE_NUMBER_MASK;
            newLpr.lastFeedbackTime = now;
            EmiPacketSequenceNumber nak = lpr.oldestSequenceNumber;
            
            // Remove all LostPackets that are older or as old as lpr in _lossSet
            _lossSet.erase(_lossSet.begin(), ++_lossSet.find(*iter));
            
            // Replace lp with a new object with incremented
            // numFeedbacks and oldestSequenceNumber, and updated
            // lastFeedbackTime
            if (/* Like newLpr.oldestSequenceNumber <= newLpr.newestSequenceNumber,
                   but supports sequence number cycling */
                EmiNetUtil::cyclicDifference24Signed(newLpr.oldestSequenceNumber,
                                                     newLpr.newestSequenceNumber) <= 0) {
                _lossSet.insert(newLpr);
            }
            
            // Exit the loop early and return the sequence number we found
            return nak;
        }
        
        ++iter;
    }
    
    // We did not find any applicable packet.
    return -1;
}
