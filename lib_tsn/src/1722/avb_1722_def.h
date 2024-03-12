// Copyright (c) 2011-2017, XMOS Ltd, All rights reserved

/**
 * \file avb_1722_def.h
 * \brief IEC 61883-6/AVB1722 Audio over 1722 AVB Transport.
 */

#pragma once

#include <inttypes.h>

#include "avb_1722_common.h"

// common definations
#define SW_REF_CLK_PERIOD_NS        (10)
#define IDEAL_NS_IN_SEC             (1000000000)
#define IDEAL_TIMER_TICKS_IN_SECOND (IDEAL_NS_IN_SEC / SW_REF_CLK_PERIOD_NS)

#define INVALID_ADJUST_NS_PER_SECOND (0xFFFFFFFF)

// AVB1722 default values
#define AVB1722_DEFAULT_AVB_PKT_DATA_LENGTH (0x38)

// Default 61883-6 values
#define AVB1722_DEFAULT_TAG     (1)
#define AVB1722_DEFAULT_CHANNEL (31)
#define AVB1722_DEFAULT_TCODE   (0xA)
#define AVB1722_DEFAULT_SY      (0)

#define AVB1722_DEFAULT_EOH1 (0)
#define AVB1722_DEFAULT_SID  (63)
#define AVB1722_DEFAULT_DBS  (2)
#define AVB1722_DEFAULT_FN   (0)
#define AVB1722_DEFAULT_QPC  (0)
#define AVB1722_DEFAULT_SPH  (0)
#define AVB1722_DEFAULT_DBC  (0)

#define AVB1722_DEFAULT_EOH2 (2)
#define AVB1722_DEFAULT_FMT  (0x10)
#define AVB1722_DEFAULT_FDF  (2)
#define AVB1722_DEFAULT_SYT  (0xFFFF)

#define AVB1722_PORT_UNINITIALIZED (0xDEAD)

#define AVB_DEFAULT_VLAN (2)

// 61883-6 CIP header
#define AVB_CIP_HDR_SIZE (8)

// 61883-6/AVB1722 CIP Header definitions.
typedef struct {
    uint8_t SID;        // bit 0,1 are fixed 00;
    uint8_t DBS;        // Data block size. In 61883 a data block is all information
                        // arriving at the transmitter in one sample period. Thus is
                        // our case it is the size of one sample multiplied by the
                        // number of channels (in quadlets)
    uint8_t FN_QPC_SPH; // bit 0-1 : Fraction Number
                        // bit 2-4 : quadlet padding count
                        // bit 5   : source packet header
    uint8_t DBC;        // data block count
    uint8_t FMT;        // stream format
    uint8_t FDF;        // format dependent field
    uint8_t SYT[2];     // synchronisation timing
} AVB_AVB1722_CIP_Header_t;

// NOTE: It is worth pointing out that the 'data block' in 61886-3 means a
// sample (or a collection of samples one for each channel)

//
// Macros for 61883 header
//
#define SET_AVB1722_CIP_TAG(x, a) (x->protocol_specific[0] |= (a & 0x3) << 6)

#define SET_AVB1722_CIP_CHANNEL(x, a) (x->protocol_specific[0] |= (a & 0x3F))
#define SET_AVB1722_CIP_TCODE(x, a)   (x->protocol_specific[1] |= (a & 0xF) << 4)
#define SET_AVB1722_CIP_SY(x, a)      (x->protocol_specific[1] |= (a & 0xF))

#define SET_AVB1722_CIP_SID(x, a)  (x->SID |= (a & 0x3F))
#define SET_AVB1722_CIP_EOH1(x, a) (x->SID |= (a & 0x3) << 6)
#define SET_AVB1722_CIP_DBS(x, a)  (x->DBS = a)
#define SET_AVB1722_CIP_FN(x, a)   (x->FN_QPC_SPH |= (a & 0x3) << 6)
#define SET_AVB1722_CIP_QPC(x, a)  (x->FN_QPC_SPH |= (a & 0x7) << 3)
#define SET_AVB1722_CIP_SPH(x, a)  (x->FN_QPC_SPH |= (a & 0x1) << 2)
#define SET_AVB1722_CIP_DBC(x, a)  (x->DBC = a)
#define SET_AVB1722_CIP_EOH2(x, a) (x->FMT |= (a & 0x3) << 6)
#define SET_AVB1722_CIP_FMT(x, a)  (x->FMT |= (a & 0x3F))
#define SET_AVB1722_CIP_FDF(x, a)  (x->FDF = a)
#define SET_AVB1722_CIP_SYT(x, a)                                                                  \
    do {                                                                                           \
        x->SYT[0] = a >> 8;                                                                        \
        x->SYT[1] = a & 0xFF;                                                                      \
    } while (0)

// Audio MBLA definitions (top 8 bits for easy combination with sample)
//   From 61883:
//   0100---- = multibit linear audio
//   ----00-- = raw audio sample
//   ------00 = 24 bit
//   ------01 = 20 bit
//   ------10 = 16 bit
//   ------11 = undefined
#define MBLA_24BIT (0x40000000)
#define MBLA_20BIT (0x41000000)
#define MBLA_16BIT (0x42000000)

#define AAF_FORMAT_USER        (0)
#define AAF_FORMAT_FLOAT_32BIT (1)
#define AAF_FORMAT_INT_32BIT   (2)
#define AAF_FORMAT_INT_24BIT   (3)
#define AAF_FORMAT_INT_16BIT   (4)
#define AAF_FORMAT_AES3_32BIT  (5)

static inline uint8_t bit_depth_from_format(uint8_t format) {
    switch (format) {
    case AAF_FORMAT_INT_32BIT:
        return 32;
    case AAF_FORMAT_INT_24BIT:
        return 24;
    case AAF_FORMAT_INT_16BIT:
        return 16;
    default:
        return 0;
    }
}

static inline uint8_t format_from_bit_depth(uint8_t bit_depth) {
    switch (bit_depth) {
    case 32:
        return AAF_FORMAT_INT_32BIT;
    case 24:
        return AAF_FORMAT_INT_24BIT;
    case 16:
        return AAF_FORMAT_INT_16BIT;
    default:
        return AAF_FORMAT_USER;
    }
}

#define AAF_NSR_USER_SPECIFIED (0)
#define AAF_NSR_8000           (1)
#define AAF_NSR_16000          (2)
#define AAF_NSR_32000          (3)
#define AAF_NSR_44100          (4)
#define AAF_NSR_48000          (5)
#define AAF_NSR_88200          (6)
#define AAF_NSR_96000          (7)
#define AAF_NSR_174000         (8)
#define AAF_NSR_192000         (9)
#define AAF_NSR_24000          (10)
#define AAF_NSR_RESERVED1      (11)
#define AAF_NSR_RESERVED2      (12)
#define AAF_NSR_RESERVED3      (13)
#define AAF_NSR_RESERVED4      (14)
#define AAF_NSR_RESERVED5      (15)

static inline uint8_t nsr_from_sampling_rate(int rate) {
    switch (rate) {
#define NSR_FROM_SAMPLING_RATE_CASE(_rate)                                                         \
    case _rate:                                                                                    \
        return AAF_NSR_##_rate
        NSR_FROM_SAMPLING_RATE_CASE(8000);
        NSR_FROM_SAMPLING_RATE_CASE(16000);
        NSR_FROM_SAMPLING_RATE_CASE(32000);
        NSR_FROM_SAMPLING_RATE_CASE(44100);
        NSR_FROM_SAMPLING_RATE_CASE(48000);
        NSR_FROM_SAMPLING_RATE_CASE(88200);
        NSR_FROM_SAMPLING_RATE_CASE(96000);
        NSR_FROM_SAMPLING_RATE_CASE(174000);
        NSR_FROM_SAMPLING_RATE_CASE(192000);
        NSR_FROM_SAMPLING_RATE_CASE(24000);
    default:
        return AAF_NSR_USER_SPECIFIED;
    }
}

static inline int sampling_rate_from_nsr(uint8_t nsr) {
    switch (nsr) {
#define SAMPLING_RATE_FROM_NSR_CASE(_rate)                                                         \
    case AAF_NSR_##_rate:                                                                          \
        return _rate
        SAMPLING_RATE_FROM_NSR_CASE(8000);
        SAMPLING_RATE_FROM_NSR_CASE(16000);
        SAMPLING_RATE_FROM_NSR_CASE(32000);
        SAMPLING_RATE_FROM_NSR_CASE(44100);
        SAMPLING_RATE_FROM_NSR_CASE(48000);
        SAMPLING_RATE_FROM_NSR_CASE(88200);
        SAMPLING_RATE_FROM_NSR_CASE(96000);
        SAMPLING_RATE_FROM_NSR_CASE(174000);
        SAMPLING_RATE_FROM_NSR_CASE(192000);
        SAMPLING_RATE_FROM_NSR_CASE(24000);
    default:
        return 0;
    }
}

#define AAF_PACKETS_PER_TIMESTAMP_SPARSE (8)
#define AAF_PACKETS_PER_TIMESTAMP_NORMAL (1)

#define AAF_PROTOCOL_SPECIFIC_SPARSE (0x8)

#define AVB_CRF_HDR_SIZE (20)

#define CRF_TYPE_USER          (0)
#define CRF_TYPE_AUDIO_SAMPLE  (1)
#define CRF_TYPE_VIDEO_FRAME   (2)
#define CRF_TYPE_VIDEO_LINE    (3)
#define CRF_TYPE_MACHINE_CYCLE (4)

#define CRF_PULL_1_1       (0)
#define CRF_PULL_1000_1001 (1)
#define CRF_PULL_1001_1000 (2)
#define CRF_PULL_24_25     (3)
#define CRF_PULL_25_24     (4)
#define CRF_PULL_1_8       (5)

// Generic configuration

enum {
    AVB1722_CONFIGURE_TALKER_STREAM = 0,
    AVB1722_CONFIGURE_LISTENER_STREAM,
    AVB1722_ADJUST_TALKER_STREAM,
    AVB1722_ADJUST_LISTENER_STREAM,
    AVB1722_DISABLE_TALKER_STREAM,
    AVB1722_DISABLE_LISTENER_STREAM,
    AVB1722_GET_ROUTER_LINK,
    AVB1722_SET_VLAN,
    AVB1722_TALKER_GO,
    AVB1722_TALKER_STOP,
    AVB1722_SET_PORT,
    AVB1722_ADJUST_LISTENER_CHANNEL_MAP,
    AVB1722_ADJUST_LISTENER_VOLUME,
    AVB1722_GET_COUNTERS
};

// The rate of 1722 packets (8kHz)
#define AVB1722_PACKET_RATE (8000)

// The number of samples per stream in each 1722 packet
#define AVB1722_LISTENER_MAX_NUM_SAMPLES_PER_CHANNEL                                               \
    ((AVB_MAX_AUDIO_SAMPLE_RATE / AVB1722_PACKET_RATE) + 1)
#define AVB1722_TALKER_MAX_NUM_SAMPLES_PER_CHANNEL (AVB_MAX_AUDIO_SAMPLE_RATE / AVB1722_PACKET_RATE)

// We add a 2% fudge factor to handle clock difference in the stream
// transmission shaping
#define AVB1722_PACKET_PERIOD_TIMER_TICKS (((100000000 / AVB1722_PACKET_RATE) * 98) / 100)

#define AVB1722_PLUS_SIP_HEADER_SIZE (24 + AVB_CIP_HDR_SIZE)
