// Copyright (c) 2011-2017, XMOS Ltd, All rights reserved

#pragma once

#include <inttypes.h>
#include <xccompat.h>
#include "xc2compat.h"
#include "avb_1722_1.h"
#include "avb_1722_1_aecp_pdu.h"
#include "avb_1722_1_aecp_aem.h"
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
                                    CLIENT_INTERFACE(avb_interface, i_avb_api),
                                    CLIENT_INTERFACE(avb_1722_1_control_callbacks, i_1722_1_entity),
                                    chanend c_ptp);
#ifdef __XC__
}
#endif
void avb_1722_1_aecp_aem_periodic(CLIENT_INTERFACE(ethernet_tx_if, i_eth));

void begin_write_upgrade_image(void);

void abort_write_upgrade_image(void);

int avb_write_upgrade_image_page(int address,
                                 uint8_t data[FLASH_PAGE_SIZE],
                                 REFERENCE_PARAM(uint16_t, status));
