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

void EmiCongestionControl::onAck(EmiTimeInterval rtt) {
    if (0 == _sendingRate) {
        // We're in the slow start phase
        _congestionWindow = _totalDataSentInSlowStart;
        
        _congestionWindow = std::max(EMI_MIN_CONGESTION_WINDOW, _congestionWindow);
        _congestionWindow = std::min(EMI_MAX_CONGESTION_WINDOW, _congestionWindow);
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
        
        _congestionWindow = _remoteDataArrivalRate * (rtt + EMI_TICK_TIME) + EMI_MIN_CONGESTION_WINDOW;
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
        
        // End the slow start phase
        _sendingRate = _remoteDataArrivalRate;
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

_avgNakCount(1),
_nakCount(1),
_decRandom(2),
_decCount(1),
_lastDecSeq(-1),

_newestSeenSequenceNumber(-1),
_newestSentSequenceNumber(-1),

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
        onAck(rtt);
    }
    
    if (packetHeader.flags & EMI_NAK_PACKET_FLAG) {
        onNak(packetHeader.nak, largestSNSoFar);
    }
    
    if (-1 == _newestSeenSequenceNumber ||
        EmiNetUtil::cyclicDifference24Signed(packetHeader.sequenceNumber, _newestSeenSequenceNumber) > 0) {
        _newestSeenSequenceNumber = packetHeader.sequenceNumber;
    }
}

void EmiCongestionControl::onRto() {
    _sendingRate /= 2;
}

void EmiCongestionControl::onDataSent(size_t size) {
    if (0 == _sendingRate) {
        // We're in slow start mode
        _totalDataSentInSlowStart += size;
    }
}

EmiPacketSequenceNumber EmiCongestionControl::ack() {
    if (_newestSeenSequenceNumber == _newestSentSequenceNumber) {
        return -1;
    }
    
    _newestSentSequenceNumber = _newestSeenSequenceNumber;
    return _newestSeenSequenceNumber;
}
