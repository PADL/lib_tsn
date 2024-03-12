// Copyright (c) 2011-2017, XMOS Ltd, All rights reserved

#pragma once

#include "default_avb_conf.h"
#include "ethernet_conf.h"

#define PTP_NUM_PORTS (NUM_ETHERNET_MASTER_PORTS)

#define PTP_LOG_MIN_PDELAY_REQ_INTERVAL (0)
#define PTP_LOG_SYNC_INTERVAL           (-3)
#define PTP_LOG_ANNOUNCE_INTERVAL       (0)

#define PTP_LEAP61                   (0)
#define PTP_LEAP59                   (1)
#define PTP_CURRENT_UTC_OFFSET_VALID (1)
#define PTP_TIMESCALE                (1)
#define PTP_TIME_TRACEABLE           (0)
#define PTP_FREQUENCY_TRACEABLE      (0)
#define PTP_CURRENT_UTC_OFFSET       (35)
#ifndef PTP_DEFAULT_GM_CAPABLE_PRIORITY1
#define PTP_DEFAULT_GM_CAPABLE_PRIORITY1 (250) // default for portable timeaware-system

#endif

#define PTP_DEFAULT_NON_GM_CAPABLE_PRIORITY1 (255)
#define PTP_DEFAULT_PRIORITY2                (248)
#define PTP_CLOCK_CLASS                      (248)    // Default
#define PTP_CLOCK_ACCURACY                   (0x20)   // The time is accurate to within 25 ns
#define PTP_OFFSET_SCALED_LOG_VARIANCE       (0x436A) // Not specified or unknown
#define PTP_TIMESOURCE                       (0xA0)   // internal oscillator

#define PTP_MAXIMUM_PATH_TRACE_TLV (8)

#define TIMER_TICKS_PER_SEC (100000000)
#define LOG_SEC_TO_TIMER_TICKS(x)                                                                  \
    (x < 0 ? (TIMER_TICKS_PER_SEC >> (-x)) : TIMER_TICKS_PER_SEC << (x))

#define PDELAY_REQ_PERIOD LOG_SEC_TO_TIMER_TICKS(PTP_LOG_MIN_PDELAY_REQ_INTERVAL)

#define SYNC_PERIOD LOG_SEC_TO_TIMER_TICKS(PTP_LOG_SYNC_INTERVAL)

#define PTP_SYNC_RECEIPT_TIMEOUT_MULTIPLE (3)

#define ANNOUNCE_PERIOD LOG_SEC_TO_TIMER_TICKS(PTP_LOG_ANNOUNCE_INTERVAL)

#define PTP_ANNOUNCE_RECEIPT_TIMEOUT_MULTIPLE (3)

#define RECV_ANNOUNCE_TIMEOUT (PTP_ANNOUNCE_RECEIPT_TIMEOUT_MULTIPLE * ANNOUNCE_PERIOD)

#define PTP_SYNC_LOCK_ACCEPTABLE_VARIATION 0x100000
#define PTP_SYNC_LOCK_STABILITY_COUNT      5

#ifndef PTP_THROW_AWAY_SYNC_OUTLIERS
#define PTP_THROW_AWAY_SYNC_OUTLIERS 0
#endif

#define PTP_ALLOWED_LOST_RESPONSES 3

#define PTP_NEIGHBOR_PROP_DELAY_THRESH_NS 800
