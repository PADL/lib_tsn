// Copyright (c) 2024 PADL Software Pty Ltd, All rights reserved
#include <string.h>
#include <assert.h>
#include "ethernet_wrappers.h"
#include "cobs.h"

#define ETH_HDR_LEN 14

// we need a valid non-zero MAC address to represent the serial port
// use a locally assigned address assigned by the author (PADL CID)
uint8_t uart_proxy_address[MACADDR_NUM_BYTES] = {0x0A, 0xE9, 0x1B, 0x00, 0x00, 0x00};

static inline unsafe uint8_t is_uart_proxy_address_p(const uint8_t *unsafe address) {
    return memcmp(address, uart_proxy_address, MACADDR_NUM_BYTES) == 0;
}

static inline unsafe uint8_t is_multicast_address_p(const uint8_t *unsafe address) {
    return (address[0] & 0x1) != 0;
}

unsafe void eth_send_packet(client interface ethernet_tx_if i_eth,
                            const uint8_t *packet,
                            size_t n,
                            unsigned dst_port) {
    i_eth.send_packet((uint8_t *restrict)packet, (unsigned)n, dst_port);
}

static unsafe void uart_send_packet(client interface uart_tx_buffered_if ?i_uart,
                                    const uint8_t *packet,
                                    size_t n) {
    assert(n > ETH_HDR_LEN);

    // strip off Ethernet header and add COBS framing
    uint8_t cobs_encoded_buf[1 + ETHERNET_MAX_PACKET_SIZE +
                             _COBS_BUFFER_PAD(ETHERNET_MAX_PACKET_SIZE) + 1] = {COBS_DELIM_BYTE};
    size_t cobs_bytes_encoded = 1 + cobs_encode(&packet[ETH_HDR_LEN], n - ETH_HDR_LEN,
                                                &cobs_encoded_buf[1], sizeof(cobs_encoded_buf) - 2);
    cobs_encoded_buf[cobs_bytes_encoded++] = COBS_DELIM_BYTE;

    for (size_t i = 0; i < cobs_bytes_encoded; i++)
        i_uart.write(cobs_encoded_buf[i]);
}

unsafe void _eth_uart_send_packet(client interface ethernet_tx_if i_eth,
                                  client interface uart_tx_buffered_if ?i_uart,
                                  const uint8_t *packet,
                                  size_t n,
                                  unsigned dst_port) {
    uint8_t is_uart_proxy_address = is_uart_proxy_address_p(packet);
    uint8_t eth_tx = !is_uart_proxy_address;
    uint8_t uart_tx = is_uart_proxy_address || is_multicast_address_p(packet);

    if (eth_tx)
        i_eth.send_packet((uint8_t *restrict)packet, (unsigned)n, dst_port);

    if (!isnull(i_uart) && uart_tx)
        uart_send_packet(i_uart, packet, n);
}
