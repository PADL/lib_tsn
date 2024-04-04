// Copyright (c) 2011-2017, XMOS Ltd, All rights reserved

#pragma once

#include <inttypes.h>
#include <xccompat.h>
#include "xc2compat.h"
#include "avb_1722_1.h"
#include "avb_1722_1_aecp_pdu.h"
#include "avb_1722_1_aecp_aem.h"
#include "aem_descriptor_types.h"
#include "avb.h"
#include "avb_1722_1_callbacks.h"
#include "ethernet.h"

void avb_1722_1_aecp_aem_init(unsigned int serial_num);
void avb_1722_1_aem_set_grandmaster_id(REFERENCE_PARAM(uint8_t, as_grandmaster_id));
#ifdef __XC__
extern "C" {
#endif
void process_avb_1722_1_aecp_packet(uint8_t src_addr[MACADDR_NUM_BYTES],
                                    avb_1722_1_aecp_packet_t *pkt,
                                    int num_packet_bytes,
                                    CLIENT_INTERFACE(ethernet_tx_if, i_eth),
                                    CLIENT_INTERFACE(uart_tx_buffered_if ?, i_uart),
                                    CLIENT_INTERFACE(avb_interface, i_avb_api),
                                    CLIENT_INTERFACE(avb_1722_1_control_callbacks, i_1722_1_entity),
                                    chanend c_ptp);
#ifdef __XC__
}
#endif
void avb_1722_1_aecp_aem_periodic(CLIENT_INTERFACE(ethernet_tx_if, i_eth),
                                  CLIENT_INTERFACE(uart_tx_buffered_if ?, i_uart));

void begin_write_upgrade_image(void);

void abort_write_upgrade_image(void);

int avb_write_upgrade_image_page(int address,
                                 uint8_t data[FLASH_PAGE_SIZE],
                                 REFERENCE_PARAM(uint16_t, status));

void send_unsolicited_notifications_state_changed(uint16_t command_type,
                                                  uint16_t stream_desc_type, // for AECP_AEM_CMD_GET_STREAM_INFO
                                                  uint16_t stream_desc_id, // for AECP_AEM_CMD_GET_STREAM_INFO
                                                  CLIENT_INTERFACE(ethernet_tx_if, i_eth),
                                                  CLIENT_INTERFACE(uart_tx_buffered_if ?, i_uart),
                                                  CLIENT_INTERFACE(avb_interface, i_avb_api),
                                                  NULLABLE_RESOURCE(chanend, c_ptp));

#ifdef __XC__
static inline void notify_listener_stream_changed(uint16_t stream_desc_id,
                                                  CLIENT_INTERFACE(ethernet_tx_if, i_eth),
                                                  CLIENT_INTERFACE(uart_tx_buffered_if ?, i_uart),
                                                  CLIENT_INTERFACE(avb_interface, i_avb_api)) {
    send_unsolicited_notifications_state_changed(AECP_AEM_CMD_GET_STREAM_INFO, AEM_STREAM_INPUT_TYPE,
                                                 stream_desc_id, i_eth, i_uart, i_avb_api, null);
}

static inline void notify_talker_stream_changed(uint16_t stream_desc_id,
                                                CLIENT_INTERFACE(ethernet_tx_if, i_eth),
                                                CLIENT_INTERFACE(uart_tx_buffered_if ?, i_uart),
                                                CLIENT_INTERFACE(avb_interface, i_avb_api)) {
    send_unsolicited_notifications_state_changed(AECP_AEM_CMD_GET_STREAM_INFO, AEM_STREAM_OUTPUT_TYPE,
                                                 stream_desc_id, i_eth, i_uart, i_avb_api, null);
}

static inline void notify_avb_info_changed(CLIENT_INTERFACE(ethernet_tx_if, i_eth),
                                           CLIENT_INTERFACE(uart_tx_buffered_if ?, i_uart),
                                           chanend c_ptp,
                                           CLIENT_INTERFACE(avb_interface, i_avb_api)) {
    send_unsolicited_notifications_state_changed(AECP_AEM_CMD_GET_AVB_INFO, 0, 0, i_eth, i_uart, i_avb_api, c_ptp);
}

static inline void notify_counters_changed(CLIENT_INTERFACE(ethernet_tx_if, i_eth),
                                           CLIENT_INTERFACE(uart_tx_buffered_if ?, i_uart),
                                           CLIENT_INTERFACE(avb_interface, i_avb_api)) {
    send_unsolicited_notifications_state_changed(AECP_AEM_CMD_GET_COUNTERS, 0, 0, i_eth, i_uart, i_avb_api, null);
}
#endif
