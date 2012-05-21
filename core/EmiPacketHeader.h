//
//  EmiPacketHeader.h
//  roshambo
//
//  Created by Per Eckerdal on 2012-02-16.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#ifndef emilir_EmiPacketHeader_h
#define emilir_EmiPacketHeader_h

#include "EmiTypes.h"

#include <stdint.h>
#include <cstddef>

// A message header, as it is represented in the receiver side of things,
// in a computation friendly format (the actual wire format is more
// condensed)
struct EmiPacketHeader {
    EmiPacketFlags flags;
    EmiTimestamp timestamp;
    EmiPacketSequenceNumber sequenceNumber;
    EmiPacketSequenceNumber nak; // Set if (flags & EMI_NAK_PACKET_FLAG)
    uint32_t linkCapacity; // Set if (flags & EMI_LINK_CAPACITY_PACKET_FLAG)
    uint32_t arrivalRate; // Set if (flags & EMI_ARRIVAL_RATE_PACKET_FLAG)
    EmiRttSequenceNumber rttRequest; // Set if (flags & EMI_RTT_REQUEST_PACKET_FLAG)
    EmiRttSequenceNumber rttResponse; // Set if (flags & EMI_RTT_RESPONSE_PACKET_FLAG)
    
    // Returns true if the parse was successful
    //
    // Note that this method does not check that the entire
    // packet fits in the buffer, only that the header fits.
    //
    // headerLength will be set to the header length. headerLength
    // can be NULL, in which case it's not set.
    static bool parse(const uint8_t *buf, size_t bufSize, EmiPacketHeader& header, size_t *headerLength);
};

#endif
