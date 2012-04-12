//
//  EmiConnTime.cpp
//  roshambo
//
//  Created by Per Eckerdal on 2012-04-09.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#include "EmiConnTime.h"

#include "EmiNetUtil.h"

#include <algorithm>
#include <cmath>

EmiConnTime::EmiConnTime() :
_initTime(-1), _largestReceivedTime(0), _hasReceivedTime(false),
_gotLargestReceivedTimeAt(-1), _rto(EMI_INIT_RTO), _srtt(-1), _rttvar(-1) {}

void EmiConnTime::onRtoTimeout() {
    _rto *= 2; // See http://www.ietf.org/rfc/rfc2988.txt (5.4)
    _rto = std::min(_rto, EMI_MAX_RTO);
    
    // See the end of section 5 at http://www.ietf.org/rfc/rfc2988.txt
    _srtt = -1;
    _rttvar = -1;
}

void EmiConnTime::gotTimestamp(float heartbeatFrequency, EmiTimeInterval now, const void *buf, size_t bufSize) {
    if (bufSize < 3*sizeof(EmiTimestamp)) return;
    
    const EmiTimestamp *stamps = (const EmiTimestamp *)buf;
    
    EmiTimestamp timestamp = ntohs(stamps[0]);
    EmiTimestamp myTimestamp = ntohs(stamps[1]);
    EmiTimestamp responseDelay = ntohs(stamps[2]);
    
    // Update the largest received time if it was updated more than a heartbeat interval ago or
    // if timestamp is bigger than this._largestReceivedTime.
    if (now - _gotLargestReceivedTimeAt > 1/heartbeatFrequency ||
        emiCyclicDifference16Signed(timestamp, _largestReceivedTime) >= 0) {
        _largestReceivedTime = timestamp;
        _gotLargestReceivedTimeAt = now;
    }
    
    if (-1 != _srtt && now - _gotLargestReceivedTimeAt < _srtt) {
        // As per http://www.ietf.org/rfc/rfc2988.txt section 3, we don't want to measure
        // the RTT more than once per RTT
        return;
    }
    
    // See http://www.ietf.org/rfc/rfc2988.txt section 2
    EmiTimestamp currentTimestamp = floor(getCurrentTime(now)*1000);
    EmiTimeInterval rtt = ((EmiTimeInterval) (emiCyclicDifference16(currentTimestamp, myTimestamp) - responseDelay))/1000.0;
    if (-1 == _srtt || -1 == _rttvar) {
        _srtt = rtt;
        _rttvar = rtt/2;
    }
    else {
        const EmiTimeInterval alpha = 0.125;
        const EmiTimeInterval beta = 0.25;
        
        _rttvar = (1-beta) * _rttvar + beta * std::abs(_srtt - rtt);
        _srtt = (1-alpha) * _srtt + alpha * rtt;
    }
    
    const EmiTimeInterval K = 4;
    const EmiTimeInterval G = 0.1; // Clock granularity
    _rto = _srtt + std::max(G, K*_rttvar);
    
    // Min RTO:
    // Note:
    // * http://utopia.duth.gr/~ipsaras/minrto-networking07-psaras.pdf
    // * http://blog.jauu.net/2010/06/04/TCP-Minimum-RTO/
    _rto = std::max(EMI_MIN_RTO, _rto);
    // Max RTO:
    _rto = std::min(EMI_MAX_RTO, _rto);
    
    _hasReceivedTime = true;
}

EmiTimeInterval EmiConnTime::getCurrentTime(EmiTimeInterval now) {
    if (-1 != _initTime) {
        return now - _initTime;
    }
    else {
        _initTime = now;
        return 0;
    }
}

EmiTimeInterval EmiConnTime::getRto() const {
    return _rto;
}

EmiTimestamp EmiConnTime::getLargestReceivedTime() const {
    return _largestReceivedTime;
}

bool EmiConnTime::hasReceivedTime() const {
    return _hasReceivedTime;
}

EmiTimeInterval EmiConnTime::gotLargestReceivedTimeAt() const {
    return _gotLargestReceivedTimeAt;
}
