//
//  EmiConnTime.cpp
//  roshambo
//
//  Created by Per Eckerdal on 2012-04-09.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#include "EmiConnTime.h"

#include "EmiNetUtil.h"
#include "EmiPacketHeader.h"

#include <algorithm>
#include <cmath>

void EmiConnTime::gotRttResponse(EmiTimeInterval rtt) {
    if (-1 == _srtt || -1 == _rttvar) {
        _srtt = rtt;
        _rttvar = rtt/2;
    }
    else {
        // Calculate _srtt and _rttvar using a smooth average
        static const EmiTimeInterval alpha = 0.125;
        
        _srtt   = (1-alpha)*_srtt   + alpha*rtt;
        _rttvar = (1-alpha)*_rttvar + alpha*std::abs(_srtt - rtt);
    }
    
    static const EmiTimeInterval K = 4;
    
    _rto = _srtt + K*_rttvar;
}

EmiConnTime::EmiConnTime() :
_rto(EMI_INIT_RTO), _srtt(-1),
_rttvar(-1), _expCount(0) {}

void EmiConnTime::swap(EmiConnTime& other) {
    EmiConnTime tmp(*this);
    *this = other;
    other = tmp;
}

void EmiConnTime::onRtoTimeout() {
    _expCount++;
}

void EmiConnTime::gotPacket(const EmiPacketHeader& header, EmiTimeInterval now) {
    _expCount = 0;
    
    if (header.flags & EMI_RTT_RESPONSE_PACKET_FLAG &&
        header.rttResponse == _rttRequestSequenceNumber) {
        gotRttResponse(now - _rttRequestTime);
    }
}

bool EmiConnTime::rttRequest(EmiTimeInterval now, EmiPacketSequenceNumber sequenceNumber) {
    EmiTimeInterval rto = getRto();
    
    if (-1 == _rttRequestSequenceNumber ||
        now-_rttRequestTime > rto) {
        _rttRequestTime = now;
        _rttRequestSequenceNumber = sequenceNumber;
        
        return true;
    }
    else {
        return false;
    }
}

EmiTimeInterval EmiConnTime::getRto() const {
    EmiTimeInterval rto = _rto*(1+_expCount) + EMI_TICK_TIME;
    
    // Min RTO:
    // Note:
    // * http://utopia.duth.gr/~ipsaras/minrto-networking07-psaras.pdf
    // * http://blog.jauu.net/2010/06/04/TCP-Minimum-RTO/
    // * http://www.jenkinssoftware.com/raknet/manual/congestioncontrol.html
    rto = std::max(EMI_MIN_RTO, rto);
    // Max RTO:
    rto = std::min(EMI_MAX_RTO, rto);
    
    return rto;
}
