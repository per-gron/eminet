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

#define EMI_DEFAULT_MTU                  (576)
#define EMI_DEFAULT_HEARTBEAT_FREQUENCY  (0.3)
#define EMI_DEFAULT_TICK_FREQUENCY       (10)
#define EMI_DEFAULT_HEARTBEATS_BEFORE_CONNECTION_WARNING (2.5)
#define EMI_DEFAULT_CONNECTION_TIMEOUT   (30)
#define EMI_DEFAULT_RECEIVER_BUFFER_SIZE (2048)
#define EMI_DEFAULT_SENDER_BUFFER_SIZE   (8192)

#define EMI_TIMESTAMP_LENGTH (6)
#define EMI_HEADER_LENGTH    (4)
#define EMI_MIN_RTO          (0.25)
#define EMI_MAX_RTO          (20.0)
#define EMI_INIT_RTO         (1.0)

#define EMI_IS_VALID_CHANNEL_QUALIFIER(cq)  (0 == ((cq) & 0x20))
#define EMI_CHANNEL_QUALIFIER_TYPE(cq)      ((EmiChannelType) (((cq) & 0xc0) >> 6))
#define EMI_CHANNEL_QUALIFIER(type, number) (((number) & 0x1f) | (((type) & 0x3) << 6))

// To be changed when priorities are actually implemented
#define EMI_PRIORITY_DEFAULT          (EMI_PRIORITY_HIGH)
#define EMI_CHANNEL_TYPE_DEFAULT      (EMI_CHANNEL_TYPE_RELIABLE_ORDERED)
#define EMI_DEFAULT_CHANNEL           (0)
#define EMI_CHANNEL_QUALIFIER_DEFAULT EMI_CHANNEL_QUALIFIER(EMI_CHANNEL_TYPE_DEFAULT, EMI_DEFAULT_CHANNEL)

typedef enum {
    EMI_PRIORITY_IMMEDIATE = 0,
    EMI_PRIORITY_HIGH      = 1,
    EMI_PRIORITY_MEDIUM    = 2,
    EMI_PRIORITY_LOW       = 3
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

typedef uint16_t EmiSequenceNumber;
typedef uint8_t  EmiChannelQualifier;
typedef uint16_t EmiTimestamp;
typedef uint8_t  EmiFlags;
typedef uint8_t  EmiSplitId;
typedef double   EmiTimeInterval;

typedef enum {
    EMI_PRX_FLAG  = 0x10,
    EMI_RST_FLAG  = 0x08,
    EMI_SYN_FLAG  = 0x04,
    EMI_ACK_FLAG  = 0x02,
    EMI_SACK_FLAG = 0x01
} EmiFlag;

#endif
