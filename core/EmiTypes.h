//
//  EmiTypes.h
//  roshambo
//
//  Created by Per Eckerdal on 2012-02-20.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#ifndef emilir_EmiTypes_h
#define emilir_EmiTypes_h

#include <stdint.h>

#define EMI_MINIMAL_MTU                  (576)
#define EMI_DEFAULT_HEARTBEAT_FREQUENCY  (0.3)
#define EMI_DEFAULT_HEARTBEATS_BEFORE_CONNECTION_WARNING (2.5)
#define EMI_DEFAULT_CONNECTION_TIMEOUT   (30)
// The receiver buffer needs to be able contain split messages as
// they are reconstructed, so this setting essentially sets an
// upper bound on how large a single message can be.
#define EMI_DEFAULT_RECEIVER_BUFFER_SIZE (131072)
#define EMI_DEFAULT_SENDER_BUFFER_SIZE   (8192)

#define EMI_UDP_HEADER_SIZE           (8)
#define EMI_MESSAGE_HEADER_MIN_LENGTH (4)
#define EMI_PACKET_HEADER_MAX_LENGTH  (21)

#define EMI_MIN_CONGESTION_WINDOW         ((size_t)(1024))
#define EMI_MAX_CONGESTION_WINDOW         ((size_t)(1024*1024*10))
#define EMI_PACKET_PAIR_INTERVAL          (16)
#define EMI_PACKET_SEQUENCE_NUMBER_LENGTH (3)
#define EMI_PACKET_SEQUENCE_NUMBER_MASK   ((1 << (8*EMI_PACKET_SEQUENCE_NUMBER_LENGTH))-1)
#define EMI_HEADER_SEQUENCE_NUMBER_LENGTH (3)
#define EMI_HEADER_SEQUENCE_NUMBER_MASK   ((1 << (8*EMI_HEADER_SEQUENCE_NUMBER_LENGTH))-1)
#define EMI_TICK_TIME        (0.01)
#define EMI_MIN_RTO          (0.1)
#define EMI_MAX_RTO          (20.0)
#define EMI_INIT_RTO         (1.0)

#define EMI_IS_VALID_CHANNEL_QUALIFIER(cq)  (0 == ((cq) & 0x20))
#define EMI_CHANNEL_QUALIFIER_TYPE(cq)      ((EmiChannelType) (((cq) & 0xc0) >> 6))
#define EMI_CHANNEL_QUALIFIER(type, number) (((number) & 0x1f) | (((type) & 0x3) << 6))

// To be changed when priorities are actually implemented
#define EMI_PRIORITY_DEFAULT          (EMI_PRIORITY_MEDIUM)
#define EMI_PRIORITY_CONTROL          (EMI_PRIORITY_HIGH)
#define EMI_CHANNEL_TYPE_DEFAULT      (EMI_CHANNEL_TYPE_RELIABLE_ORDERED)
#define EMI_DEFAULT_CHANNEL           (0)
#define EMI_CONTROL_CHANNEL           (-1) // Special SYN/RST message channel. SenderBuffer requires this to be an integer
#define EMI_CHANNEL_QUALIFIER_DEFAULT EMI_CHANNEL_QUALIFIER(EMI_CHANNEL_TYPE_DEFAULT, EMI_DEFAULT_CHANNEL)

typedef enum {
    EMI_PRIORITY_IMMEDIATE   = 0,
    EMI_PRIORITY_HIGH        = 1,
    EMI_PRIORITY_MEDIUM      = 2,
    EMI_PRIORITY_LOW         = 3,
    EMI_NUMBER_OF_PRIORITIES = 4
} EmiPriority;

typedef enum {
    EMI_CHANNEL_TYPE_UNRELIABLE           = 0,
    EMI_CHANNEL_TYPE_UNRELIABLE_SEQUENCED = 1,
    EMI_CHANNEL_TYPE_RELIABLE_SEQUENCED   = 2,
    EMI_CHANNEL_TYPE_RELIABLE_ORDERED     = 3
} EmiChannelType;

typedef enum {
    EMI_REASON_NO_ERROR                   = 0,
    EMI_REASON_THIS_HOST_CLOSED           = 1,
    EMI_REASON_OTHER_HOST_CLOSED          = 2,
    EMI_REASON_CONNECTION_TIMED_OUT       = 3,
    EMI_REASON_OTHER_HOST_DID_NOT_RESPOND = 4
} EmiDisconnectReason;

typedef enum {
    EMI_CONNECTION_TYPE_SERVER,
    EMI_CONNECTION_TYPE_CLIENT,
    EMI_CONNECTION_TYPE_P2P
} EmiConnectionType;

typedef enum {
    // This is the state for non-EMI_CONNECTION_TYPE_P2P connections
    EMI_P2P_STATE_NOT_ESTABLISHING = 0,
    EMI_P2P_STATE_ESTABLISHING     = 1,
    EMI_P2P_STATE_ESTABLISHED      = 2,
    EMI_P2P_STATE_FAILED           = 3
} EmiP2PState;

// Represents a 24 bit number
typedef uint32_t EmiSequenceNumber;
// Like EmiSequenceNumber, but does not wrap at 24 bits. Its purpose is to
// be able to implement correct less-than predicates for sequence numbers
// for use with binary trees.
typedef uint64_t EmiNonWrappingSequenceNumber;
#define EMI_NON_WRAPPING_SEQUENCE_NUMBER_MAX UINT64_MAX
// Represents a 24 bit number. -1 means no value
typedef int32_t  EmiPacketSequenceNumber;
// Like EmiPacketSequenceNumber, but does not wrap at 24 bits. Its purpose
// is to be able to implement correct less-than predicates for sequence
// numbers for use with binary trees.
typedef int64_t  EmiNonWrappingPacketSequenceNumber;
typedef uint8_t  EmiChannelQualifier;
typedef uint16_t EmiTimestamp;
typedef uint8_t  EmiMessageFlags;
typedef uint8_t  EmiPacketFlags;
typedef double   EmiTimeInterval;

typedef enum {
    EMI_SPLIT_NOT_FIRST_FLAG = 0x40, // This flag means that this is a split message, and it's not the first part
    EMI_SPLIT_NOT_LAST_FLAG  = 0x20, // This flag means that this is a split message, and it's not the last part
    EMI_PRX_FLAG             = 0x10,
    EMI_RST_FLAG             = 0x08,
    EMI_SYN_FLAG             = 0x04,
    EMI_ACK_FLAG             = 0x02,
    EMI_SACK_FLAG            = 0x01
} EmiMessageFlag;

typedef enum {
    EMI_SEQUENCE_NUMBER_PACKET_FLAG = 0x01,
    EMI_ACK_PACKET_FLAG             = 0x02,
    EMI_NAK_PACKET_FLAG             = 0x04,
    EMI_LINK_CAPACITY_PACKET_FLAG   = 0x08,
    EMI_ARRIVAL_RATE_PACKET_FLAG    = 0x10,
    EMI_RTT_REQUEST_PACKET_FLAG     = 0x20,
    EMI_RTT_RESPONSE_PACKET_FLAG    = 0x40,
    EMI_EXTRA_FLAGS_PACKET_FLAG     = 0x80
} EmiPacketFlag;

typedef enum {
    EMI_1_BYTE_FILLER_EXTRA_PACKET_FLAG = 0x01,
    EMI_2_BYTE_FILLER_EXTRA_PACKET_FLAG = 0x02
} EmiPacketExtraFlags;

#endif
