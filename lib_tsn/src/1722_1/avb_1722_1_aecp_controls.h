// Copyright (c) 2013-2017, XMOS Ltd, All rights reserved

#pragma once

#include "avb_1722_1_aecp_pdu.h"
#include "xc2compat.h"
#include "avb.h"
#include "avb_1722_1_callbacks.h"

unsafe void set_current_fields_in_descriptor(uint8_t *unsafe descriptor,
                                             unsigned int desc_size_bytes,
                                             unsigned int read_type,
                                             unsigned int read_id,
                                             CLIENT_INTERFACE(avb_interface, i_avb_api),
                                             CLIENT_INTERFACE(avb_1722_1_control_callbacks,
                                                              i_1722_1_entity));

unsafe uint16_t process_aem_cmd_getset_control(avb_1722_1_aecp_packet_t *unsafe pkt,
                                               REFERENCE_PARAM(uint8_t, status),
                                               uint16_t command_type,
                                               CLIENT_INTERFACE(avb_1722_1_control_callbacks,
                                                                i_1722_1_entity));

unsafe void process_aem_cmd_getset_signal_selector(avb_1722_1_aecp_packet_t *unsafe pkt,
                                                   REFERENCE_PARAM(uint8_t, status),
                                                   uint16_t command_type,
                                                   CLIENT_INTERFACE(avb_1722_1_control_callbacks,
                                                                    i_1722_1_entity));

unsafe void process_aem_cmd_getset_stream_info(avb_1722_1_aecp_packet_t *unsafe pkt,
                                               REFERENCE_PARAM(uint8_t, status),
                                               uint16_t command_type,
                                               CLIENT_INTERFACE(avb_interface, i_avb));

unsafe void process_aem_cmd_getset_stream_format(avb_1722_1_aecp_packet_t *unsafe pkt,
                                                 REFERENCE_PARAM(uint8_t, status),
                                                 uint16_t command_type,
                                                 CLIENT_INTERFACE(avb_interface, i_avb));

unsafe void process_aem_cmd_getset_sampling_rate(avb_1722_1_aecp_packet_t *unsafe pkt,
                                                 REFERENCE_PARAM(uint8_t, status),
                                                 uint16_t command_type,
                                                 CLIENT_INTERFACE(avb_interface, i_avb));

unsafe void process_aem_cmd_getset_clock_source(avb_1722_1_aecp_packet_t *unsafe pkt,
                                                REFERENCE_PARAM(uint8_t, status),
                                                uint16_t command_type,
                                                CLIENT_INTERFACE(avb_interface, i_avb));

unsafe void process_aem_cmd_startstop_streaming(avb_1722_1_aecp_packet_t *unsafe pkt,
                                                REFERENCE_PARAM(uint8_t, status),
                                                uint16_t command_type,
                                                CLIENT_INTERFACE(avb_interface, i_avb));

unsafe void process_aem_cmd_get_counters(avb_1722_1_aecp_packet_t *unsafe pkt,
                                         REFERENCE_PARAM(uint8_t, status),
                                         CLIENT_INTERFACE(avb_interface, i_avb));

unsafe void process_aem_cmd_get_avb_info(avb_1722_1_aecp_packet_t *unsafe pkt,
                                         REFERENCE_PARAM(uint8_t, status),
                                         CLIENT_INTERFACE(avb_interface, i_avb),
                                         chanend c_ptp);
 
unsafe void process_aem_cmd_get_as_path(avb_1722_1_aecp_packet_t *unsafe pkt,
                                        REFERENCE_PARAM(uint8_t, status),
                                        chanend c_ptp);
