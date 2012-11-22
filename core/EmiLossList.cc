//
//  EmiLossList.cc
//  eminet
//
//  Created by Per Eckerdal on 2012-05-23.
//  Copyright (c) 2012 Per Eckerdal. All rights reserved.
//

#include "EmiLossList.h"

#include "EmiNetUtil.h"

EmiLossList::EmiLossList() :
_newestSequenceNumber(-1),
_newestSequenceNumberTime(0),
_lossSet() {}

EmiLossList::~EmiLossList() {}

void EmiLossList::gotPacket(EmiTimeInterval now, EmiPacketSequenceNumber wrappedSequenceNumber) {
    
    EmiNonWrappingPacketSequenceNumber expectedSn = _newestSequenceNumber+1;
    
    // Based on the last sequence number that we got, try to
    // guess the non-wrapped sequence number.
    EmiNonWrappingPacketSequenceNumber guessedNonWrappedSequenceNumber;
    {
        
        // positive diff means older than expected
        int32_t diff = EmiNetUtil::cyclicDifferenceSigned<EMI_PACKET_SEQUENCE_NUMBER_LENGTH>(expectedSn & EMI_PACKET_SEQUENCE_NUMBER_MASK,
                                                                                             wrappedSequenceNumber);
        if (diff > 0 && diff > expectedSn) {
            // Don't allow a negative guessedNonWrappedSequenceNumber
            guessedNonWrappedSequenceNumber = wrappedSequenceNumber;
        }
        else {
            guessedNonWrappedSequenceNumber = expectedSn - diff;
        }
    }
    
    if (-1 == _newestSequenceNumber ||
        expectedSn == guessedNonWrappedSequenceNumber) {
        // We received the expected sequence number.
        // No need to do anything with the loss set.
    }
    else if (_newestSequenceNumber > guessedNonWrappedSequenceNumber) {
        // We received an old sequence number, which presumably
        // arrived out of order. Remove it from the loss set if
        // it's present.
        
        LossSetIter iter = _lossSet.lower_bound(LostPacketRange(0, 0, guessedNonWrappedSequenceNumber));
        
        if (_lossSet.end() == iter) {
            // We did not find any matching LostPacketRange
            return;
        }
        
        const LostPacketRange& lpr(*iter);
        
        if (lpr.oldestSequenceNumber > guessedNonWrappedSequenceNumber) {
            // The LostPacketRange we found doesn't contain this sequence number
            return;
        }
        
        // We only have a reference to lp, so we must use it before
        // we erase it from _lossSet.
        LostPacketRange lowerBound(lpr);
        lowerBound.newestSequenceNumber = guessedNonWrappedSequenceNumber-1;
        LostPacketRange upperBound(lpr);
        upperBound.oldestSequenceNumber = guessedNonWrappedSequenceNumber+1;
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
    else if (guessedNonWrappedSequenceNumber - _newestSequenceNumber > 1) {
        // We received a newer sequence number than what we
        // expected. Add the lost range to the loss set.
        
        LostPacketRange lpr(now, 
                            _newestSequenceNumber+1,
                            guessedNonWrappedSequenceNumber-1);
        _lossSet.insert(lpr);
    }
    
    _newestSequenceNumber = guessedNonWrappedSequenceNumber;
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
            newLpr.oldestSequenceNumber = newLpr.oldestSequenceNumber+1;
            newLpr.lastFeedbackTime = now;
            EmiNonWrappingPacketSequenceNumber nak = lpr.oldestSequenceNumber;
            
            // Remove all LostPackets that are older or as old as lpr in _lossSet
            _lossSet.erase(_lossSet.begin(), ++_lossSet.find(*iter));
            
            // Replace lp with a new object with incremented
            // numFeedbacks and oldestSequenceNumber, and updated
            // lastFeedbackTime
            if (newLpr.oldestSequenceNumber <= newLpr.newestSequenceNumber) {
                _lossSet.insert(newLpr);
            }
            
            // Exit the loop early and return the sequence number we found
            return nak & EMI_PACKET_SEQUENCE_NUMBER_MASK;
        }
        
        ++iter;
    }
    
    // We did not find any applicable packet.
    return -1;
}
