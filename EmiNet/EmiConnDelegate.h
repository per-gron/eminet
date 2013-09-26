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
#import "EmiDispatchQueueWrapper.h"

class EmiConnDelegate {
    EmiConnection *_conn;
    EmiDispatchQueueWrapper *_queueWrapper;
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
    
    inline EmiDispatchQueueWrapper *getSocketCookie() {
        return _queueWrapper;
    }
    
    inline EmiDispatchQueueWrapper *getTimerCookie() {
        return _queueWrapper;
    }
    
    void waitForDelegateBlocks();
};

#endif
