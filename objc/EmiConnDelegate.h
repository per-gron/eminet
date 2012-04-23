//
//  EmiConnDelegate.h
//  roshambo
//
//  Created by Per Eckerdal on 2012-04-11.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#ifndef emilir_EmiConnDelegate_h
#define emilir_EmiConnDelegate_h

#import "EmiConnection.h"

class EmiConnDelegate {
    NSTimer *_tickTimer;
    NSTimer *_heartbeatTimer;
    NSTimer *_rtoTimer;
    NSTimer *_connectionTimer;
    NSTimer *_rateLimitTimer;
    
    EmiConnection *_conn;
public:
    
    EmiConnDelegate(EmiConnection *conn);
    
    void invalidate();
    
    void emiConnMessage(EmiChannelQualifier channelQualifier, NSData *data, NSUInteger offset, NSUInteger size);
    
    void startRateLimitTimer(EmiTimeInterval rate);
    
    void scheduleConnectionWarning(EmiTimeInterval warningTimeout);
    void scheduleConnectionTimeout(EmiTimeInterval interval);
    
    void ensureTickTimeout(EmiTimeInterval interval);
    
    void scheduleHeartbeatTimeout(EmiTimeInterval interval);
    
    void ensureRtoTimeout(EmiTimeInterval rto);
    void invalidateRtoTimeout();
    
    void emiConnLost();
    void emiConnRegained();
    void emiConnDisconnect(EmiDisconnectReason reason);
    
    inline EmiConnection *getConn() { return _conn; }
};

#endif
