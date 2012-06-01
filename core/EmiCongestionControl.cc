//
//  EmiCongestionControl.cpp
//  rock
//
//  Created by Per Eckerdal on 2012-05-24.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#include "EmiCongestionControl.h"

#include "EmiPacketHeader.h"
#include "EmiNetUtil.h"

#include <algorithm>
#include <cmath>

void EmiCongestionControl::endSlowStartPhase() {
    _sendingRate = _remoteDataArrivalRate;
}

void EmiCongestionControl::onAck(EmiTimeInterval rtt) {
    if (0 == _sendingRate) {
        // We're in the slow start phase
        _congestionWindow = std::max(EMI_MIN_CONGESTION_WINDOW, _totalDataSentInSlowStart);
        
        if (_congestionWindow >= EMI_MAX_CONGESTION_WINDOW) {
            _congestionWindow = EMI_MAX_CONGESTION_WINDOW;
            
            endSlowStartPhase();
        }
    }
    else {
        // We're not in the slow start phase
        
        float inc = 1;
        
        if (_remoteLinkCapacity > _sendingRate) {
            // These are constants as specified by UDT. I have no
            // idea of why they have this particular value.
            static const double ALPHA = 8;
            static const double BETA  = 0.0000015;
            inc = std::max(std::pow(10, std::ceil(std::log10((_remoteLinkCapacity-_sendingRate)*ALPHA))) * BETA,
                           1.0);
        }
        
        _sendingRate += inc/EMI_TICK_TIME;
        
        _congestionWindow = _remoteDataArrivalRate * (rtt + EMI_TICK_TIME) + 10*1024;
        _congestionWindow = std::min(EMI_MAX_CONGESTION_WINDOW, _congestionWindow);
    }
}

void EmiCongestionControl::onNak(EmiPacketSequenceNumber nak,
                                 EmiPacketSequenceNumber largestSNSoFar) {
    if (0 == _sendingRate) {
        // We're in the slow start phase.
        
        if (-1 == _remoteLinkCapacity ||
            -1 == _remoteDataArrivalRate) {
            // We got a NAK, but we have not yet received
            // data about the link capacity and the arrival
            // rate. Ignore this packet.
            return;
        }
        
        endSlowStartPhase();
    }
    else {
        // We're not in the slow start phase
        
        static const float SENDING_RATE_DECREASE = 1.125;
        
        if (nak > _lastDecSeq) {
            // This NAK starts a new congestion period
            
            _sendingRate /= SENDING_RATE_DECREASE;
            
            static const float SMOOTH = 0.125;
            _avgNakCount = (1-SMOOTH)*_avgNakCount + SMOOTH*_nakCount;
            _nakCount = 1;
            _decRandom = (arc4random() % (((int)std::floor(_avgNakCount))+1)) + 1;
            _decCount = 1;
            _lastDecSeq = largestSNSoFar;
        }
        else {
            // This NAK does not start a new congestion period
            if (_decCount <= 5 && _nakCount == _decCount*_decRandom) {
                // The _decCount <= 5 ensures that the sending rate is not
                // decreased by more than 50% per congestion period (1.125^6≈2)
                
                _sendingRate /= SENDING_RATE_DECREASE;
                _decCount++;
                _lastDecSeq = largestSNSoFar;
            }
            
            _nakCount++;
        }
    }
}

EmiCongestionControl::EmiCongestionControl() :
_congestionWindow(EMI_MIN_CONGESTION_WINDOW),
_sendingRate(0),
_totalDataSentInSlowStart(0),

_linkCapacity(),
_dataArrivalRate(),

_avgPacketSize(-1),

_avgNakCount(1),
_nakCount(1),
_decRandom(2),
_decCount(1),
_lastDecSeq(-1),

_newestSentSN(-1),
_newestSeenAckSN(-1),

_newestSeenSN(-1),
_newestSentAckSN(-1),

_remoteLinkCapacity(-1),
_remoteDataArrivalRate(-1) {}

EmiCongestionControl::~EmiCongestionControl() {}

void EmiCongestionControl::gotPacket(EmiTimeInterval now, EmiTimeInterval rtt,
                                     EmiPacketSequenceNumber largestSNSoFar,
                                     const EmiPacketHeader& packetHeader, size_t packetLength) {
    static const float SMOOTH = 0.125;
    
    _linkCapacity.gotPacket(now, packetHeader.sequenceNumber, packetLength);
    _dataArrivalRate.gotPacket(now, packetLength);
    
    if (packetHeader.flags & EMI_LINK_CAPACITY_PACKET_FLAG &&
        // Make sure we don't save bogus data
        packetHeader.linkCapacity > 0) {
        if (-1 == _remoteLinkCapacity) {
            _remoteLinkCapacity = packetHeader.linkCapacity;
        }
        else {
            _remoteLinkCapacity = (1-SMOOTH)*_remoteLinkCapacity + SMOOTH*packetHeader.linkCapacity;
        }
    }
    
    if (packetHeader.flags & EMI_ARRIVAL_RATE_PACKET_FLAG &&
        // Make sure we don't save bogus data
        packetHeader.arrivalRate > 0) {
        if (-1 == _remoteDataArrivalRate) {
            _remoteDataArrivalRate = packetHeader.arrivalRate;
        }
        else {
            _remoteDataArrivalRate = (1-SMOOTH)*_remoteDataArrivalRate + SMOOTH*packetHeader.arrivalRate;
        }
    }
    
    if (packetHeader.flags & EMI_ACK_PACKET_FLAG) {
        if (-1 == _newestSeenAckSN ||
            EmiNetUtil::cyclicDifference24Signed(packetHeader.ack,
                                                 _newestSeenAckSN) > 0) {
            _newestSeenAckSN = packetHeader.ack;
        }
        
        onAck(rtt);
    }
    
    if (packetHeader.flags & EMI_NAK_PACKET_FLAG) {
        onNak(packetHeader.nak, largestSNSoFar);
    }
    
    if (packetHeader.flags & EMI_SEQUENCE_NUMBER_PACKET_FLAG) {
        if (-1 == _newestSeenSN ||
            EmiNetUtil::cyclicDifference24Signed(packetHeader.sequenceNumber, _newestSeenSN) > 0) {
            ASSERT(-1 == _newestSeenSN ||
                   EmiNetUtil::cyclicDifference24Signed(packetHeader.sequenceNumber,
                                                        _newestSeenSN) < 100);
            _newestSeenSN = packetHeader.sequenceNumber;
        }
    }
}

void EmiCongestionControl::onRto() {
    _sendingRate /= 2;
}

void EmiCongestionControl::onDataSent(EmiPacketSequenceNumber sequenceNumber, size_t size) {
    if (-1 == _newestSentSN) {
        _newestSeenAckSN = ((sequenceNumber-1) & EMI_PACKET_SEQUENCE_NUMBER_MASK);
    }
    _newestSentSN = sequenceNumber;
    
    if (0 == _sendingRate) {
        // We're in slow start mode
        _totalDataSentInSlowStart += size;
    }
    
    if (-1 == _avgPacketSize) {
        _avgPacketSize = size;
    }
    else {
        static const float SMOOTH = 0.125;
        _avgPacketSize = (1-SMOOTH)*_avgPacketSize + SMOOTH*size;
    }
}

EmiPacketSequenceNumber EmiCongestionControl::ack() {
    if (_newestSeenSN == _newestSentAckSN) {
        return -1;
    }
    
    _newestSentAckSN = _newestSeenSN;
    return _newestSeenSN;
}

size_t EmiCongestionControl::tickAllowance() const {
    int packetsInTransit;
    
    if (-1 == _newestSentSN) {
        packetsInTransit = 0;
    }
    else {
        // /2 because presumably half of the packets are
        // in transit, the other half's ACKs are in transit
        packetsInTransit = EmiNetUtil::cyclicDifference24(_newestSentSN,
                                                          _newestSeenAckSN)/2;
    }
    
    size_t cwndAllowance = _congestionWindow - packetsInTransit*_avgPacketSize;
    size_t rateAllowance = _sendingRate * EMI_TICK_TIME;
    
    if (0 == rateAllowance) {
        return cwndAllowance;
    }
    else {
        return std::min(cwndAllowance, rateAllowance);
    }
}
