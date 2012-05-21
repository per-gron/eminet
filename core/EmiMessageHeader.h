//
//  EmiMessageHeader.h
//  roshambo
//
//  Created by Per Eckerdal on 2012-02-16.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#ifndef emilir_EmiMessageHeader_h
#define emilir_EmiMessageHeader_h

#include "EmiTypes.h"

#include <stdint.h>
#include <cstddef>
#include <netinet/in.h>

// A message header, as it is represented in the receiver side of things,
// in a computation friendly format (the actual wire format is more
// condensed)
struct EmiMessageHeader {
    typedef bool (^EmiParseMessageBlock)(const EmiMessageHeader& header, size_t dataOffset);
    
    EmiMessageFlags flags;
    EmiChannelQualifier channelQualifier;
    // This is int32_t and not EmiChannelQualifier because it has to be capable of
    // holding -1, which means that the header had no sequence number
    int32_t sequenceNumber;
    // This is int32_t and not EmiSplitId because it has to be capable of
    // holding -1, which means that the header had no split id
    int32_t splitId;
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
    
    // Returns true if the entire parse was successful
    static bool parseMessages(const uint8_t *buf, size_t bufSize, EmiParseMessageBlock block);
};

#endif
