// Copyright (c) 2024, PADL Software Pty Ltd, All rights reserved

#pragma once

#include <inttypes.h>
#include <xccompat.h>
#include "ethernet.h"
#include "uart.h"
#include "cobs.h"

unsafe void eth_send_packet(CLIENT_INTERFACE(ethernet_tx_if, i_eth),
                            const uint8_t *packet,
                            size_t n,
                            unsigned dst_port);

unsafe void _eth_uart_send_packet(CLIENT_INTERFACE(ethernet_tx_if, i_eth),
                                  CLIENT_INTERFACE(uart_tx_buffered_if ?, i_uart),
                                  const uint8_t *packet,
                                  size_t n,
                                  unsigned dst_port);

static inline void eth_uart_send_packet(CLIENT_INTERFACE(ethernet_tx_if, i_eth),
                                        CLIENT_INTERFACE(uart_tx_buffered_if ?, i_uart),
                                        const uint8_t *packet,
                                        size_t n,
                                        unsigned dst_port) {
    unsafe { _eth_uart_send_packet(i_eth, i_uart, packet, n, dst_port); }
}

#define ETH_RAW_DATA (ETH_NO_DATA + 1) // raw packet without Ethernet header

extern uint8_t uart_proxy_address[MACADDR_NUM_BYTES]; // denotes UART source/destination address

typedef enum uart_state {
    UART_STATE_SYNCHRONIZING,
    UART_STATE_READING,
    UART_STATE_FINISHED
} uart_state;

// we don't count the leading and trailing zeros here because they are consumed by uart_rx_byte()
#define COBS_BUFFER_SIZE (ETHERNET_MAX_PACKET_SIZE + _COBS_BUFFER_PAD(ETHERNET_MAX_PACKET_SIZE))

#ifdef __XC__
// return values: -1 (error), 0 (continue), 1 (process packet)
static inline int uart_rx_byte(REFERENCE_PARAM(uart_state, state),
                               uint8_t byte,
                               ARRAY_OF_SIZE(uint8_t, cobs_encoded_buf, COBS_BUFFER_SIZE),
                               REFERENCE_PARAM(size_t, cobs_bytes_read),
                               ARRAY_OF_SIZE(uint8_t, payload_buf, ETHERNET_MAX_PACKET_SIZE),
                               REFERENCE_PARAM(size_t, payload_len)) {
    int ret;

    if (cobs_bytes_read >= COBS_BUFFER_SIZE) {
        state = UART_STATE_SYNCHRONIZING;
        cobs_bytes_read = 0;
        return -1;
    }

    switch (state) {
    case UART_STATE_SYNCHRONIZING:
        if (byte == COBS_DELIM_BYTE)
            state = UART_STATE_READING;
        ret = 0;
        break;
    case UART_STATE_READING:
        if (byte != COBS_DELIM_BYTE) {
            cobs_encoded_buf[cobs_bytes_read++] = byte;
            ret = 0;
            break;
        }
        state = UART_STATE_FINISHED;
        [[fallthrough]];
    case UART_STATE_FINISHED:
        payload_len =
            cobs_decode(cobs_encoded_buf, cobs_bytes_read, payload_buf, ETHERNET_MAX_PACKET_SIZE);
        ret = (payload_len > 0);
        state = UART_STATE_SYNCHRONIZING;
        cobs_bytes_read = 0;
        break;
    }

    return ret;
}
#endif
