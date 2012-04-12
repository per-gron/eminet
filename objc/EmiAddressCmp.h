//
//  EmiAddressCmp.h
//  roshambo
//
//  Created by Per Eckerdal on 2012-04-12.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#ifndef roshambo_EmiAddressCmp_h
#define roshambo_EmiAddressCmp_h

#import "GCDAsyncUdpSocket.h"

class EmiAddressCmp {
public:
    inline NSComparisonResult operator()(NSData *a, NSData *b) const {
        NSString *aHost;
        NSString *bHost;
        uint16_t aPort;
        uint16_t bPort;
        int aFamily;
        int bFamily;
        
        [GCDAsyncUdpSocket getHost:&aHost port:&aPort family:&aFamily fromAddress:a];
        [GCDAsyncUdpSocket getHost:&bHost port:&bPort family:&bFamily fromAddress:b];
        
        if (aPort < bPort) return -1;
        else if (aPort > bPort) return 1;
        else {
            if (aFamily < bFamily) return -1;
            else if (aFamily > bFamily) return 1;
            else {
                return [aHost compare:bHost];
            }
        }
    }
};

#endif
