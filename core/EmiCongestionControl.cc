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

void EmiCongestionControl::onAck() {
    if (0 == _sendingPeriod) {
        // We're in the slow start phase
        _congestionWindow = std::max(EMI_INITIAL_CONGESTION_WINDOW, _totalDataSentInSlowStart);
        
        _congestionWindow = std::min(EMI_MAX_CONGESTION_WINDOW, _congestionWindow);
    }
    else {
        // We're not in the slow start phase
    }
}

void EmiCongestionControl::onNak() {
    
}

EmiCongestionControl::EmiCongestionControl() :
_congestionWindow(EMI_INITIAL_CONGESTION_WINDOW),
_sendingPeriod(0),
_totalDataSentInSlowStart(0),
_linkCapacity(),
_dataArrivalRate(),
_newestSeenSequenceNumber(-1),
_newestSentSequenceNumber(-1),
_remoteLinkCapacity(-1),
_remoteDataArrivalRate(-1) {}

EmiCongestionControl::~EmiCongestionControl() {}

void EmiCongestionControl::gotPacket(EmiTimeInterval now, const EmiPacketHeader& packetHeader, size_t packetLength) {
    static const float SMOOTH = 0.125;
    
    _linkCapacity.gotPacket(now, packetHeader.sequenceNumber, packetLength);
    _dataArrivalRate.gotPacket(now, packetLength);
    
    if (packetHeader.flags & EMI_LINK_CAPACITY_PACKET_FLAG) {
        if (-1 == _remoteLinkCapacity) {
            _remoteLinkCapacity = packetHeader.linkCapacity;
        }
        else {
            _remoteLinkCapacity = (1-SMOOTH)*_remoteLinkCapacity + SMOOTH*packetHeader.linkCapacity;
        }
    }
    
    if (packetHeader.flags & EMI_ARRIVAL_RATE_PACKET_FLAG) {
        if (-1 == _remoteDataArrivalRate) {
            _remoteDataArrivalRate = packetHeader.arrivalRate;
        }
        else {
            _remoteDataArrivalRate = (1-SMOOTH)*_remoteDataArrivalRate + SMOOTH*packetHeader.arrivalRate;
        }
    }
    
    if (packetHeader.flags & EMI_ACK_PACKET_FLAG) {
        onAck();
    }
    
    if (packetHeader.flags & EMI_NAK_PACKET_FLAG) {
        onNak();
    }
    
    if (-1 == _newestSeenSequenceNumber ||
        EmiNetUtil::cyclicDifference24Signed(packetHeader.sequenceNumber, _newestSeenSequenceNumber) > 0) {
        _newestSeenSequenceNumber = packetHeader.sequenceNumber;
    }
}

void EmiCongestionControl::onRto() {
    _sendingPeriod *= 2;
}

EmiSequenceNumber EmiCongestionControl::ack() {
    if (_newestSeenSequenceNumber == _newestSentSequenceNumber) {
        return -1;
    }
    
    _newestSentSequenceNumber = _newestSeenSequenceNumber;
    return _newestSeenSequenceNumber;
}
