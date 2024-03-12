// Copyright (c) 2011-2017, XMOS Ltd, All rights reserved
#ifndef _avb_srp_pdu_h_
#define _avb_srp_pdu_h_

#define AVB_SRP_ATTRIBUTE_TYPE_TALKER_ADVERTISE 1
#define AVB_SRP_ATTRIBUTE_TYPE_TALKER_FAILED    2
#define AVB_SRP_ATTRIBUTE_TYPE_LISTENER         3

typedef struct srp_talker_first_value {
    uint8_t StreamId[8];
    uint8_t DestMacAddr[6];
    uint8_t VlanID[2];
    uint8_t TSpecMaxFrameSize[2];
    uint8_t TSpecMaxIntervalFrames[2];
    uint8_t TSpec;
    uint8_t AccumulatedLatency[4];
} srp_talker_first_value;

typedef struct srp_talker_failed_first_value {
    uint8_t StreamId[8];
    uint8_t DestMacAddr[6];
    uint8_t VlanID[2];
    uint8_t TSpecMaxFrameSize[2];
    uint8_t TSpecMaxIntervalFrames[2];
    uint8_t TSpec;
    uint8_t AccumulatedLatency[4];
    uint8_t FailureBridgeId[8];
    uint8_t FailureCode;
} srp_talker_failed_first_value;

#define AVB_SRP_MAX_INTERVAL_FRAMES_DEFAULT 1
#define AVB_SRP_TSPEC_RANK_DEFAULT          1
#define AVB_SRP_TSPEC_PRIORITY_DEFAULT      3
#define AVB_SRP_TSPEC_RESERVED_VALUE        0

// Initial guess at 150us
#define AVB_SRP_ACCUMULATED_LATENCY_DEFAULT (150000U)

typedef struct srp_listener_first_value {
    uint8_t StreamId[8];
} srp_listener_first_value;

#define AVB_SRP_FOUR_PACKED_EVENT_IGNORE        0
#define AVB_SRP_FOUR_PACKED_EVENT_ASKING_FAILED 1
#define AVB_SRP_FOUR_PACKED_EVENT_READY         2
#define AVB_SRP_FOUR_PACKED_EVENT_READY_FAILED  3

typedef struct srp_domain_first_value {
    uint8_t SRclassID;
    uint8_t SRclassPriority;
    uint8_t SRclassVID[2];
} srp_domain_first_value;

#define AVB_SRP_ATTRIBUTE_TYPE_DOMAIN 4
#define AVB_SRP_SRCLASS_DEFAULT       6
#endif // _avb_srp_pdu_h_
