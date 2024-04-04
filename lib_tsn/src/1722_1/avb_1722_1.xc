// Copyright (c) 2013-2017, XMOS Ltd, All rights reserved
// Portions Copyright (c) 2024, PADL Software Pty Ltd, All rights reserved
#include <print.h>
#include <string.h>
#include "xassert.h"
#include "avb.h"
#include "avb_internal.h"
#include "avb_1722_common.h"
#include "avb_1722_1.h"
#include "avb_1722_1_common.h"
#include "avb_1722_1_adp.h"
#include "avb_1722_1_acmp.h"
#include "avb_1722_1_aecp.h"
#include "avb_1722_maap.h"
#include "ethernet.h"
#include "avb_1722_1_protocol.h"
#include "avb_mrp.h"
#include "avb_srp.h"
#include "avb_mvrp.h"
#include "otp_board_info.h"
#include "cobs.h"

#define PERIODIC_POLL_TIME 5000

uint8_t my_mac_addr[6];
extern uint8_t maap_dest_addr[6];
extern uint8_t avb_1722_1_adp_dest_addr[6];

// Buffer for constructing 1722.1 transmit packets
unsigned int avb_1722_1_buf[AVB_1722_1_PACKET_SIZE_WORDS];

// The GUID of this device
guid_t my_guid;

void avb_1722_1_init(uint8_t macaddr[MACADDR_NUM_BYTES], unsigned serial_num) {
    memcpy(my_mac_addr, macaddr, MACADDR_NUM_BYTES);

    my_guid.c[0] = macaddr[5];
    my_guid.c[1] = macaddr[4];
    my_guid.c[2] = macaddr[3];
    my_guid.c[3] = 0xfe;
    my_guid.c[4] = 0xff;
    my_guid.c[5] = macaddr[2];
    my_guid.c[6] = macaddr[1];
    my_guid.c[7] = macaddr[0];

    avb_1722_1_adp_init();
#if (AVB_1722_1_AEM_ENABLED)
    avb_1722_1_aecp_aem_init(serial_num);
#endif

#if (AVB_1722_1_CONTROLLER_ENABLED)
    avb_1722_1_acmp_controller_init();
#endif
#if (AVB_1722_1_TALKER_ENABLED)
    // Talker state machine is initialised once MAAP has finished
#endif
#if (AVB_1722_1_LISTENER_ENABLED)
    avb_1722_1_acmp_listener_init();
#endif
}

void avb_1722_1_process_packet(uint8_t buf[len],
                               unsigned len,
                               uint8_t src_addr[6],
                               CLIENT_INTERFACE(ethernet_tx_if, i_eth),
                               CLIENT_INTERFACE(uart_tx_buffered_if ?, i_uart),
                               CLIENT_INTERFACE(avb_interface, i_avb_api),
                               CLIENT_INTERFACE(avb_1722_1_control_callbacks, i_1722_1_entity),
                               chanend c_ptp) {
    avb_1722_1_packet_header_t *pkt = (avb_1722_1_packet_header_t *)&buf[0];
    unsigned subtype = GET_1722_1_SUBTYPE(pkt);
    unsigned datalen = GET_1722_1_DATALENGTH(pkt);

    switch (subtype) {
    case DEFAULT_1722_1_ADP_SUBTYPE:
        if (datalen == AVB_1722_1_ADP_CD_LENGTH) {
            process_avb_1722_1_adp_packet(*(avb_1722_1_adp_packet_t *)pkt, i_eth, i_uart);
        }
        return;
    case DEFAULT_1722_1_AECP_SUBTYPE:
        process_avb_1722_1_aecp_packet(src_addr, (avb_1722_1_aecp_packet_t *)pkt, len, i_eth, i_uart,
                                       i_avb_api, i_1722_1_entity, c_ptp);
        return;
    case DEFAULT_1722_1_ACMP_SUBTYPE:
        if (datalen == AVB_1722_1_ACMP_CD_LENGTH) {
            process_avb_1722_1_acmp_packet((avb_1722_1_acmp_packet_t *)pkt, i_eth, i_uart);
        }
        return;
    default:
        return;
    }
}

void avb_1722_1_periodic(CLIENT_INTERFACE(ethernet_tx_if, i_eth),
                         CLIENT_INTERFACE(uart_tx_buffered_if ?, i_uart),
                         chanend c_ptp,
                         client interface avb_interface i_avb) {
    avb_1722_1_adp_advertising_periodic(i_eth, i_uart, c_ptp, i_avb);
#if (AVB_1722_1_CONTROLLER_ENABLED)
    avb_1722_1_adp_discovery_periodic(i_eth, i_uart, i_avb);
    avb_1722_1_acmp_controller_periodic(i_eth, i_uart, i_avb);
#endif
#if (AVB_1722_1_TALKER_ENABLED)
    avb_1722_1_acmp_talker_periodic(i_eth, i_uart, i_avb);
#endif
#if (AVB_1722_1_LISTENER_ENABLED)
    avb_1722_1_acmp_listener_periodic(i_eth, i_uart, i_avb);
#endif
    avb_1722_1_aecp_aem_periodic(i_eth, i_uart);
}

// avb_mrp.c:
extern uint8_t srp_dest_mac[6];
extern uint8_t mvrp_dest_mac[6];

[[combinable]]
void avb_1722_1_maap_srp_task(client interface avb_interface i_avb,
                              client interface avb_1722_1_control_callbacks i_1722_1_entity,
                              fl_QSPIPorts &?qspi_ports,
                              client interface ethernet_rx_if i_eth_rx,
                              client interface ethernet_tx_if i_eth_tx,
                              client interface ethernet_cfg_if i_eth_cfg,
                              chanend c_ptp,
                              otp_ports_t &?otp_ports,
                              client interface uart_rx_if ?i_uart_rx,
                              client interface uart_tx_buffered_if ?i_uart_tx) {
    unsigned periodic_timeout;
    timer tmr;
    unsigned int buf[(ETHERNET_MAX_PACKET_SIZE + 3) / 4];
    uint8_t mac_addr[6];
    unsigned int serial = 0;

    uart_state state = UART_STATE_SYNCHRONIZING;
    uint8_t cobs_encoded_buf[COBS_BUFFER_SIZE];
    size_t cobs_bytes_read = 0;

    if (!isnull(otp_ports)) {
        otp_board_info_get_serial(otp_ports, serial);
    }

#if AVB_1722_1_FIRMWARE_UPGRADE_ENABLED
    if (isnull(qspi_ports)) {
        fail("Firmware upgrade enabled but QSPI ports null");
    } else if (fl_connect(qspi_ports)) {
        fail("Could not connect to flash");
    }
#endif

    srp_store_ethernet_interface(i_eth_tx);
    mrp_store_ethernet_interface(i_eth_tx);

    i_eth_cfg.get_macaddr(0, mac_addr);

    mrp_init(mac_addr);
    srp_domain_init();
    avb_mvrp_init();

    size_t eth_index = i_eth_rx.get_index();
    ethernet_macaddr_filter_t avdecc_maap_filter;
    avdecc_maap_filter.appdata = 0;
    memcpy(avdecc_maap_filter.addr, mac_addr, 6);
    i_eth_cfg.add_macaddr_filter(eth_index, 0, avdecc_maap_filter);
    memcpy(avdecc_maap_filter.addr, maap_dest_addr, 6);
    i_eth_cfg.add_macaddr_filter(eth_index, 0, avdecc_maap_filter);
    memcpy(avdecc_maap_filter.addr, avb_1722_1_adp_dest_addr, 6);
    i_eth_cfg.add_macaddr_filter(eth_index, 0, avdecc_maap_filter);
    i_eth_cfg.add_ethertype_filter(eth_index, AVB_1722_ETHERTYPE);

    ethernet_macaddr_filter_t msrp_mvrp_filter;
    msrp_mvrp_filter.appdata = 0;
    memcpy(msrp_mvrp_filter.addr, srp_dest_mac, 6);
    i_eth_cfg.add_macaddr_filter(eth_index, 0, msrp_mvrp_filter);
    memcpy(msrp_mvrp_filter.addr, mvrp_dest_mac, 6);
    i_eth_cfg.add_macaddr_filter(eth_index, 0, msrp_mvrp_filter);
    i_eth_cfg.add_ethertype_filter(eth_index, AVB_SRP_ETHERTYPE);
    i_eth_cfg.add_ethertype_filter(eth_index, AVB_MVRP_ETHERTYPE);

    avb_1722_1_init(mac_addr, serial);
    avb_1722_maap_init(mac_addr);
#if NUM_ETHERNET_PORTS > 1
    avb_1722_maap_request_addresses(AVB_NUM_SOURCES, null);
#endif

    tmr :> periodic_timeout;

    while (1) {
        select {
            // Receive and process any incoming AVB packets (802.1Qat, 1722_MAAP)
            case i_eth_rx.packet_ready(): {
                ethernet_packet_info_t packet_info;
                i_eth_rx.get_packet(packet_info, (char *)buf, ETHERNET_MAX_PACKET_SIZE);
                avb_process_srp_control_packet(i_avb, buf, packet_info.len, packet_info.type, i_eth_tx,
                                               packet_info.src_ifnum);
                avb_process_1722_control_packet(buf, packet_info.len, packet_info.type,
                                                i_eth_tx, i_uart_tx, i_avb,
                                                i_1722_1_entity, c_ptp);
                break;
            }
            case !isnull(i_uart_rx) => i_uart_rx.data_ready():
                uint8_t byte = i_uart_rx.read();
                size_t payload_len;

                if (uart_rx_byte(state, byte, cobs_encoded_buf, cobs_bytes_read, (uint8_t *)buf, payload_len) == 1) {
                    avb_process_1722_control_packet(buf, payload_len, ETH_RAW_DATA,
                                                    i_eth_tx, i_uart_tx, i_avb,
                                                    i_1722_1_entity, c_ptp);
                }
                break;
            // Periodic processing
            case tmr when timerafter(periodic_timeout) :> unsigned int time_now: {
                avb_1722_1_periodic(i_eth_tx, i_uart_tx, c_ptp, i_avb);
                avb_1722_maap_periodic(i_eth_tx, i_avb);
                mrp_periodic(i_avb);

                periodic_timeout = time_now + PERIODIC_POLL_TIME;
                break;
            }
        }
    }
}

[[combinable]]
void avb_1722_1_maap_task(otp_ports_t &?otp_ports,
                              client interface avb_interface i_avb,
                              client interface avb_1722_1_control_callbacks i_1722_1_entity,
                              fl_QSPIPorts &?qspi_ports,
                              client interface ethernet_rx_if i_eth_rx,
                              client interface ethernet_tx_if i_eth_tx,
                              client interface ethernet_cfg_if i_eth_cfg,
                              chanend c_ptp,
                              client interface uart_rx_if ?i_uart_rx,
                              client interface uart_tx_buffered_if ?i_uart_tx) {
  unsigned periodic_timeout;
  timer tmr;
  unsigned int buf[(ETHERNET_MAX_PACKET_SIZE + 3) / 4];
  uint8_t mac_addr[6];
  unsigned int serial = 0;

  uart_state state = UART_STATE_SYNCHRONIZING;
  uint8_t cobs_encoded_buf[COBS_BUFFER_SIZE];
  size_t cobs_bytes_read = 0;

  if (!isnull(otp_ports)) {
      otp_board_info_get_serial(otp_ports, serial);
  }
#if AVB_1722_1_FIRMWARE_UPGRADE_ENABLED
  if (isnull(qspi_ports)) {
      fail("Firmware upgrade enabled but QSPI ports null");
  } else if (fl_connect(qspi_ports)) {
      fail("Could not connect to flash");
  }
#endif

  i_eth_cfg.get_macaddr(0, mac_addr);

  size_t eth_index = i_eth_rx.get_index();
  ethernet_macaddr_filter_t avdecc_maap_filter;
  avdecc_maap_filter.appdata = 0;
  memcpy(avdecc_maap_filter.addr, mac_addr, 6);
  i_eth_cfg.add_macaddr_filter(eth_index, 0, avdecc_maap_filter);
  memcpy(avdecc_maap_filter.addr, maap_dest_addr, 6);
  i_eth_cfg.add_macaddr_filter(eth_index, 0, avdecc_maap_filter);
  memcpy(avdecc_maap_filter.addr, avb_1722_1_adp_dest_addr, 6);
  i_eth_cfg.add_macaddr_filter(eth_index, 0, avdecc_maap_filter);

  avb_1722_1_init(mac_addr, serial);
  avb_1722_maap_init(mac_addr);
#if NUM_ETHERNET_PORTS > 1
  avb_1722_maap_request_addresses(AVB_NUM_SOURCES, null);
#endif

  tmr :> periodic_timeout;

  while (1) {
      select {
      // Receive and process any incoming AVB packets (802.1Qat, 1722_MAAP)
      case i_eth_rx.packet_ready(): {
          ethernet_packet_info_t packet_info;
          i_eth_rx.get_packet(packet_info, (char *)buf, AVB_1722_1_PACKET_SIZE_WORDS * 4);

          avb_process_1722_control_packet(buf, packet_info.len, packet_info.type,
                                          i_eth_tx, i_uart_tx, i_avb,
                                          i_1722_1_entity, c_ptp);
          break;
      }
      case !isnull(i_uart_rx) => i_uart_rx.data_ready():
          uint8_t byte = i_uart_rx.read();
          size_t payload_len;

          if (uart_rx_byte(state, byte, cobs_encoded_buf, cobs_bytes_read, (uint8_t *)buf, payload_len) == 1)
              avb_process_1722_control_packet(buf, payload_len, ETH_RAW_DATA,
                                              i_eth_tx, i_uart_tx, i_avb,
                                              i_1722_1_entity, c_ptp);
          break;
      // Periodic processing
      case tmr when timerafter(periodic_timeout) :> unsigned int time_now:
      {
          avb_1722_1_periodic(i_eth_tx, i_uart_tx, c_ptp, i_avb);
          avb_1722_maap_periodic(i_eth_tx, i_avb);

          periodic_timeout += PERIODIC_POLL_TIME;
          break;
      }
      }
  }
}
