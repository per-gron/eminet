//
//  EmiMessageHeader.h
//  eminet
//
//  Created by Per Eckerdal on 2012-02-16.
//  Copyright (c) 2012 Per Eckerdal. All rights reserved.
//

#ifndef eminet_EmiMessageHeader_h
#define eminet_EmiMessageHeader_h

#include "EmiTypes.h"

#include <stdint.h>
#include <cstddef>
#include <netinet/in.h>

// A message header, as it is represented in the receiver side of things,
// in a computation friendly format (the actual wire format is more
// condensed)
struct EmiMessageHeader {
    EmiMessageFlags flags;
    EmiChannelQualifier channelQualifier;
    // This is int32_t and not EmiSequenceNumber because it has to be capable of
    // holding -1, which means that the header had no sequence number
    int32_t sequenceNumber;
    size_t headerLength;
    // length is 0 if the message has no content
    size_t length;
    // This is int32_t and not EmiSequenceNumber because it has to be capable of
    // holding -1, which means that the header had no ack
    int32_t ack;
    
    // Returns true if the parse was successful
    //
    // Note that this method does not check that the entire
    // message fits in the buffer, only that the header fits.
    static bool parse(const uint8_t *buf, size_t bufSize, EmiMessageHeader& header);
    
    // Returns true if the parse was successful
    static bool parseNextMessage(const uint8_t *buf, size_t bufSize,
                                 size_t *offset,
                                 size_t *dataOffset,
                                 EmiMessageHeader *header);
};

#endif
