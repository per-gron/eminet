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
#import "EmiDispatchQueueWrapper.h"

class EmiConnDelegate {
    EmiConnection *_conn;
public:
    
    EmiConnDelegate(EmiConnection *conn);
    
    void invalidate();
    
    void emiConnMessage(EmiChannelQualifier channelQualifier, NSData *data, NSUInteger offset, NSUInteger size);
    
    void emiConnLost();
    void emiConnRegained();
    void emiConnDisconnect(EmiDisconnectReason reason);
    void emiNatPunchthroughFinished(bool success);
    
    inline EmiConnection *getConn() { return _conn; }
    
    inline EmiDispatchQueueWrapper *getSocketCookie() {
        // TODO
        return nil;
    }
};

#endif
