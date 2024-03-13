// Copyright (c) 2011-2017, XMOS Ltd, All rights reserved
#ifndef __AVB_1722_MAAP_H_
#define __AVB_1722_MAAP_H_
#include <xccompat.h>
#include "xc2compat.h"
#include "avb.h"

#define MAX_AVB_1722_MAAP_PDU_SIZE (64)

/** Request a range of multicast addresses.
 *
 *  This function requests a range of multicast addresses to use as destination
 *  addresses for IEEE 1722 streams. It starts the reservation process
 *  according to the 1722 MAAP protocol. If the reservation is successful it
 *  is reported via the status return value of avb_periodic().
 *
 *  \param num_addresses    number of addresses to try and reserve;
 *                          will be reserved in a contiguous range
 *  \param start_address    an optional six byte array specifying the required
 *                          start address of the range NOTE: must be within the MAAP reserved pool;
 *                           if argument is null then the start address will be picked at
 *                          random from the MAAP reserved pool
 *
 **/
#ifdef __XC__
void avb_1722_maap_request_addresses(int num_addresses, char (&?start_address)[]);
#else
void avb_1722_maap_request_addresses(int num_addresses, char start_address[]);
#endif

void avb_1722_maap_init(uint8_t macaddr[6]);

#ifdef __XC__
void avb_1722_maap_process_packet(uint8_t buf[nbytes], unsigned int nbytes, uint8_t src_addr[6], client interface ethernet_tx_if i_eth);
#else
void avb_1722_maap_process_packet(uint8_t buf[], unsigned int nbytes, uint8_t src_addr[6], CLIENT_INTERFACE(ethernet_tx_if, i_eth));
#endif

/** Relinquish the reserved MAAP address range
 *
 *  This function abandons the claim to the reserved address range
 */
void avb_1722_maap_relinquish_addresses();


/** Re-request a claim on the existing address range
 *
 *  If there is a current address reservation, this will reset the state
 *  machine into the PROBE state, in order to cause the protocol to
 *  re-probe and re-allocate the addresses.
 */
void avb_1722_maap_rerequest_addresses();

#ifdef __XC__
/** Perform MAAP periodic functions
 *
 *  This function performs the various functions needed for the periodic
 *  operation of the MAAP protocol.  For instance, the periodic transmission
 *  of announcement messages.
 *
 *  This function is called internally by the AVB general periodic function.
 *
 *  \param c_tx    Channel for ethernet transmission
 *  \param i_avb   client interface of type avb_interface into avb_manager()
 */
void avb_1722_maap_periodic(client interface ethernet_tx_if i_eth, client interface avb_interface i_avb);

/** MAAP has indicated that a multicast address has been successfully reserved for this Talker stream
 *
 * \param i_avb   client interface of type avb_interface into avb_manager()
 * \param source_num    The local source ID of the Talker
 * \param mac_addr      The destination MAC address reserved for this Talker
 */
void avb_talker_on_source_address_reserved(client interface avb_interface i_avb, int source_num, uint8_t mac_addr[6]);

/** Default implementation of avb_talker_on_source_address_reserved()
 *
 * \param i_avb   client interface of type avb_interface into avb_manager()
 * \param source_num    The local source ID of the Talker
 * \param mac_addr      The destination MAC address reserved for this Talker
 */
void avb_talker_on_source_address_reserved_default(client interface avb_interface i_avb, int source_num, uint8_t mac_addr[6]);


int avb_1722_maap_get_base_address(uint8_t addr[6]);

#endif

#endif
