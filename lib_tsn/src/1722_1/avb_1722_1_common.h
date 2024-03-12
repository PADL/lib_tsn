// Copyright (c) 2011-2017, XMOS Ltd, All rights reserved
#ifndef AVB_1722_1_COMMON_H_
#define AVB_1722_1_COMMON_H_

#include <xclib.h>
#include <inttypes.h>

#include "nettypes.h"
#include "ethernet.h"
#include "avb_1722_1_protocol.h"
#include "xccompat.h"
#include "xc2compat.h"

void print_guid_ln(const_guid_ref_t g);
void print_mac_ln(uint8_t c[MACADDR_NUM_BYTES]);
unsigned compare_guid(uint8_t a[8], const_guid_ref_t b);
void zero_guid(guid_ref_t guid);
void hton_guid(uint8_t dst[8], const_guid_ref_t src);
void ntoh_guid(guid_ref_t dst, const uint8_t src[8]);
int qlog2(unsigned n);

#ifdef __XC__
extern "C" {
#endif
void avb_1722_1_create_1722_1_header(const uint8_t *dest_addr, int subtype, int message_type, uint8_t valid_time_status, unsigned data_len, ethernet_hdr_t *hdr);
#ifdef __XC__
}
#endif


#endif /* AVB_1722_1_COMMON_H_ */
