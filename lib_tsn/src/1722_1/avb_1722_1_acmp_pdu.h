// Copyright (c) 2011-2017, XMOS Ltd, All rights reserved

#pragma once

#include <inttypes.h>

#include "avb_1722_1_protocol.h"
#include "avb_1722_1_default_conf.h"

#define AVB_1722_1_ACMP_CD_LENGTH 44

#define AVB_1722_1_ACMP_FLAGS_CLASS_B        (0x0001)
#define AVB_1722_1_ACMP_FLAGS_FAST_CONNECT   (0x0002)
#define AVB_1722_1_ACMP_FLAGS_SAVED_STATE    (0x0004)
#define AVB_1722_1_ACMP_FLAGS_STREAMING_WAIT (0x0008)
#define AVB_1722_1_ACMP_FLAGS_ENCRYPTED_PDU  (0x0010)

#define ACMP_TIMEOUT_CONNECT_TX_COMMAND        2000
#define ACMP_TIMEOUT_DISCONNECT_TX_COMMAND     200
#define ACMP_TIMEOUT_GET_TX_STATE_COMMAND      200
#define ACMP_TIMEOUT_CONNECT_RX_COMMAND        4500
#define ACMP_TIMEOUT_DISCONNECT_RX_COMMAND     500
#define ACMP_TIMEOUT_GET_RX_STATE_COMMAND      200
#define ACMP_TIMEOUT_GET_TX_CONNECTION_COMMAND 200

/**
 *  A 1722.1 AVDECC Connection Management packet
 *
 * \note all elements 16 bit aligned
 */
typedef struct {
    avb_1722_1_packet_header_t header;
    uint8_t stream_id[8];
    uint8_t controller_guid[8];
    uint8_t talker_guid[8];
    uint8_t listener_guid[8];
    uint8_t talker_unique_id[2];
    uint8_t listener_unique_id[2];
    uint8_t dest_mac[MACADDR_NUM_BYTES];
    uint8_t connection_count[2];
    uint8_t sequence_id[2];
    uint8_t flags[2];
    uint8_t vlan_id[2];
    uint8_t reserved[2];
} avb_1722_1_acmp_packet_t;

#define AVB_1722_1_ACMP_PACKET_SIZE (sizeof(ethernet_hdr_t) + sizeof(avb_1722_1_acmp_packet_t))

typedef struct {
    uint8_t message_type;
    uint8_t status;
    stream_t stream_id;
    guid_t controller_guid;
    guid_t talker_guid;
    guid_t listener_guid;
    uint16_t talker_unique_id;
    uint16_t listener_unique_id;
    uint8_t stream_dest_mac[MACADDR_NUM_BYTES];
    uint16_t connection_count;
    uint16_t sequence_id;
    uint16_t flags;
    uint16_t vlan_id;
} avb_1722_1_acmp_cmd_resp;

typedef struct {
    guid_t guid;
    uint16_t unique_id;
    short padding;
} avb_1722_1_acmp_listener_pair;

typedef struct {
    guid_t controller_guid;
    guid_t talker_guid;
    uint16_t talker_unique_id;
    uint16_t padding;
} avb_1722_1_acmp_fast_connect_talker_info;

typedef struct {
    unsigned int info_present_bitfield;
    avb_1722_1_acmp_fast_connect_talker_info talkers[AVB_1722_1_MAX_LISTENERS];
} avb_1722_1_acmp_fast_connect_persist_state;

typedef struct {
    stream_t stream_id;
    int connection_count;
    uint8_t destination_mac[MACADDR_NUM_BYTES];
    avb_1722_1_acmp_listener_pair connected_listeners[AVB_1722_1_MAX_LISTENERS_PER_TALKER];
} avb_1722_1_acmp_talker_stream_info;

typedef struct {
    guid_t talker_guid;
    uint16_t talker_unique_id;
    int connected;
    stream_t stream_id;
    uint8_t destination_mac[MACADDR_NUM_BYTES];
} avb_1722_1_acmp_listener_stream_info;

typedef struct {
    int in_use;
    unsigned int timeout;
    unsigned int retried;
    avb_1722_1_acmp_cmd_resp command;
    uint16_t original_sequence_id;
} avb_1722_1_acmp_inflight_command;

typedef enum {
    ACMP_CMD_CONNECT_TX_COMMAND = 0,
    ACMP_CMD_CONNECT_TX_RESPONSE = 1,
    ACMP_CMD_DISCONNECT_TX_COMMAND = 2,
    ACMP_CMD_DISCONNECT_TX_RESPONSE = 3,
    ACMP_CMD_GET_TX_STATE_COMMAND = 4,
    ACMP_CMD_GET_TX_STATE_RESPONSE = 5,
    ACMP_CMD_CONNECT_RX_COMMAND = 6,
    ACMP_CMD_CONNECT_RX_RESPONSE = 7,
    ACMP_CMD_DISCONNECT_RX_COMMAND = 8,
    ACMP_CMD_DISCONNECT_RX_RESPONSE = 9,
    ACMP_CMD_GET_RX_STATE_COMMAND = 10,
    ACMP_CMD_GET_RX_STATE_RESPONSE = 11,
    ACMP_CMD_GET_TX_CONNECTION_COMMAND = 12,
    ACMP_CMD_GET_TX_CONNECTION_RESPONSE = 13
} avb_1722_1_acmp_message_t;

typedef enum {
    ACMP_STATUS_SUCCESS = 0,
    ACMP_STATUS_LISTENER_UNKNOWN_ID = 1,
    ACMP_STATUS_TALKER_UNKNOWN_ID = 2,
    ACMP_STATUS_TALKER_DEST_MAC_FAIL = 3,
    ACMP_STATUS_TALKER_NO_STREAM_INDEX = 4,
    ACMP_STATUS_TALKER_NO_BANDWIDTH = 5,
    ACMP_STATUS_TALKER_EXCLUSIVE = 6,
    ACMP_STATUS_LISTENER_TALKER_TIMEOUT = 7,
    ACMP_STATUS_LISTENER_EXCLUSIVE = 8,
    ACMP_STATUS_STATE_UNAVAILABLE = 9,
    ACMP_STATUS_NOT_CONNECTED = 10,
    ACMP_STATUS_NO_SUCH_CONNECTION = 11,
    ACMP_STATUS_COULD_NOT_SEND_MESSAGE = 12,
    ACMP_STATUS_TALKER_MISBEHAVING = 13,
    ACMP_STATUS_LISTENER_MISBEHAVING = 14,
    ACMP_STATUS_SRP_FACE = 15,
    ACMP_STATUS_CONTROLLER_NOT_AUTHORIZED = 16,
    ACMP_STATUS_INCOMPATIBLE_REQUEST = 17,
    ACMP_STATUS_NOT_SUPPORTED = 31
} avb_1722_1_acmp_status_t;
