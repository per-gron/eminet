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

EmiCongestionControl::EmiCongestionControl() :
_congestionWindow(EMI_INITIAL_CONGESTION_WINDOW),
_sendingRate(0),
_linkCapacity(),
_dataArrivalRate(),
_newestSeenSequenceNumber(-1),
_newestSentSequenceNumber(-1) {}

EmiCongestionControl::~EmiCongestionControl() {}

void EmiCongestionControl::gotPacket(EmiTimeInterval now, const EmiPacketHeader& packetHeader, size_t packetLength) {
    _linkCapacity.gotPacket(now, packetHeader.sequenceNumber, packetLength);
    _dataArrivalRate.gotPacket(now, packetLength);
    
    if (packetHeader.flags & EMI_NAK_PACKET_FLAG) {
        // TODO
    }
    
    if (-1 == _newestSeenSequenceNumber ||
        EmiNetUtil::cyclicDifference24Signed(packetHeader.sequenceNumber, _newestSeenSequenceNumber) > 0) {
        _newestSeenSequenceNumber = packetHeader.sequenceNumber;
    }
}

EmiSequenceNumber EmiCongestionControl::ack() {
    if (_newestSeenSequenceNumber == _newestSentSequenceNumber) {
        return -1;
    }
    
    _newestSentSequenceNumber = _newestSeenSequenceNumber;
    return _newestSeenSequenceNumber;
}
