// Copyright (c) 2011-2017, XMOS Ltd, All rights reserved
#include <inttypes.h>
#include <xs1.h>
#include "gptp.h"
#include "gptp_internal.h"

static void send_cmd(chanend c, char cmd) {
    outuchar(c, cmd);
    outuchar(c, cmd);
    outuchar(c, cmd);
    outct(c, XS1_CT_END);
}

void ptp_request_time_info(chanend c) { send_cmd(c, PTP_GET_TIME_INFO); }

void ptp_get_requested_time_info(chanend c, ptp_time_info &info) {
    timer tmr;
    signed thiscore_now, othercore_now;
    unsigned server_tile_id;
    slave {
        tmr :> thiscore_now;
        c :> othercore_now;
        c :> info.local_ts;
        c :> info.ptp_ts;
        c :> info.ptp_adjust;
        c :> info.inv_ptp_adjust;
        c :> server_tile_id;
    }
    if (server_tile_id != get_local_tile_id()) {
        info.local_ts = info.local_ts - (othercore_now - thiscore_now - 3);
    }
}

void ptp_get_time_info(chanend c, ptp_time_info &info) {
    ptp_request_time_info(c);
    ptp_get_requested_time_info(c, info);
}

void ptp_request_time_info_mod64(chanend c) { send_cmd(c, PTP_GET_TIME_INFO_MOD64); }

void ptp_get_requested_time_info_mod64(chanend c, ptp_time_info_mod64 &info) {
    timer tmr;
    signed thiscore_now, othercore_now;
    unsigned server_tile_id;
    slave {
        c<:0; tmr:> thiscore_now;
        c :> othercore_now;
        c :> info.local_ts;
        c :> info.ptp_ts_hi;
        c :> info.ptp_ts_lo;
        c :> info.ptp_adjust;
        c :> info.inv_ptp_adjust;
        c :> server_tile_id;
    }
    if (server_tile_id != get_local_tile_id()) {
        // 3 = protocol instruction cycle difference
        info.local_ts = info.local_ts - (othercore_now - thiscore_now - 3);
    }
}

void ptp_get_local_time_info_mod64(ptp_time_info_mod64 &info);

void ptp_get_time_info_mod64(chanend ?c,
                             ptp_time_info_mod64  &info)
{
    ptp_request_time_info_mod64(c);
    ptp_get_requested_time_info_mod64(c, info);
}

void ptp_get_current_grandmaster(chanend ptp_server, uint8_t grandmaster[8]) {
    send_cmd(ptp_server, PTP_GET_GRANDMASTER);
    slave {
        for (int i = 0; i < 8; i++) {
            ptp_server :> grandmaster[i];
        }
    }
}

ptp_port_role_t ptp_get_state(chanend ptp_server) {
    ptp_port_role_t state;
    send_cmd(ptp_server, PTP_GET_STATE);
    slave {
        ptp_server :> state;
    }

    return state;
}

void ptp_get_propagation_delay(chanend ptp_server, unsigned *pdelay) {
    send_cmd(ptp_server, PTP_GET_PDELAY);
    slave {
        ptp_server :> *pdelay;
    }
}

void ptp_get_as_path(chanend ptp_server,
                     n64_t pathSequence[PTP_MAXIMUM_PATH_TRACE_TLV],
                     uint16_t *count) {
    send_cmd(ptp_server, PTP_GET_AS_PATH);
    slave {
        ptp_server :> *count;
        for (uint16_t i = 0; i < *count; i++)
            ptp_server :> pathSequence[i];
    }
}
