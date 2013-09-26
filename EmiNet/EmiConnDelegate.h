//
//  EmiConnDelegate.h
//  eminet
//
//  Created by Per Eckerdal on 2012-04-11.
//  Copyright (c) 2012 Per Eckerdal. All rights reserved.
//

#ifndef eminet_EmiConnDelegate_h
#define eminet_EmiConnDelegate_h

#import "EmiConnection.h"

class EmiConnDelegate {
    EmiConnection *_conn;
    dispatch_queue_t _queue;
    dispatch_group_t _dispatchGroup;
    
public:
    
    EmiConnDelegate(EmiConnection *conn);
    virtual ~EmiConnDelegate();
    
    EmiConnDelegate(const EmiConnDelegate& other);
    EmiConnDelegate& operator=(const EmiConnDelegate& other);
    
    void invalidate();
    
    void emiConnPacketLoss(EmiChannelQualifier channelQualifier,
                           EmiSequenceNumber packetsLost);
    void emiConnMessage(EmiChannelQualifier channelQualifier, NSData *data, NSUInteger offset, NSUInteger size);
    
    void emiConnLost();
    void emiConnRegained();
    void emiConnDisconnect(EmiDisconnectReason reason);
    void emiNatPunchthroughFinished(bool success);
    
    inline EmiConnection *getConn() { return _conn; }
    
    inline dispatch_queue_t getSocketCookie() {
        return _queue;
    }
    
    inline dispatch_queue_t getTimerCookie() {
        return _queue;
    }
    
    void waitForDelegateBlocks();
};

#endif
