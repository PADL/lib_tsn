// Copyright (c) 2011-2017, XMOS Ltd, All rights reserved

#pragma once

#include <inttypes.h>

#include "avb_1722_1_protocol.h"
#include "avb_1722_1_default_conf.h"

#define AVB_1722_1_ADP_CD_LENGTH 56

#define AVB_1722_1_ADP_ENTITY_CAPABILITIES_EFU_MODE                         (0x00000001)
#define AVB_1722_1_ADP_ENTITY_CAPABILITIES_ADDRESS_ACCESS_SUPPORTED         (0x00000002)
#define AVB_1722_1_ADP_ENTITY_CAPABILITIES_GATEWAY_ENTITY                   (0x00000004)
#define AVB_1722_1_ADP_ENTITY_CAPABILITIES_AEM_SUPPORTED                    (0x00000008)
#define AVB_1722_1_ADP_ENTITY_CAPABILITIES_LEGACY_AVC                       (0x00000010)
#define AVB_1722_1_ADP_ENTITY_CAPIBILITIES_ASSOCIATION_ID_SUPPORTED         (0x00000020)
#define AVB_1722_1_ADP_ENTITY_CAPIBILITIES_ASSOCIATION_ID_VALID             (0x00000040)
#define AVB_1722_1_ADP_ENTITY_CAPIBILITIES_VENDOR_UNIQUE_SUPPORTED          (0x00000080)
#define AVB_1722_1_ADP_ENTITY_CAPABILITIES_CLASS_A_SUPPORTED                (0x00000100)
#define AVB_1722_1_ADP_ENTITY_CAPABILITIES_CLASS_B_SUPPORTED                (0x00000200)
#define AVB_1722_1_ADP_ENTITY_CAPABILITIES_GPTP_SUPPORTED                   (0x00000400)
#define AVB_1722_1_ADP_ENTITY_CAPABILITIES_AEM_AUTHENTICATION_SUPPORTED     (0x00000800)
#define AVB_1722_1_ADP_ENTITY_CAPABILITIES_AEM_AUTHENTICATION_REQUIRED      (0x00001000)
#define AVB_1722_1_ADP_ENTITY_CAPABILITIES_AEM_PERSISTENT_ACQUIRE_SUPPORTED (0x00002000)
#define AVB_1722_1_ADP_ENTITY_CAPABILITIES_AEM_IDENTIFY_CONTROL_INDEX_VALID (0x00004000)
#define AVB_1722_1_ADP_ENTITY_CAPABILITIES_AEM_INTERFACE_INDEX_VALID        (0x00008000)
#define AVB_1722_1_ADP_ENTITY_CAPABILITIES_GENERAL_CONTROLLER_IGNORE        (0x00010000)
#define AVB_1722_1_ADP_ENTITY_CAPABILITIES_ENTITY_NOT_READY                 (0x00020000)

#define AVB_1722_1_ADP_TALKER_CAPABILITIES_IMPLEMENTED        (0x0001)
#define AVB_1722_1_ADP_TALKER_CAPABILITIES_OTHER_SOURCE       (0x0200)
#define AVB_1722_1_ADP_TALKER_CAPABILITIES_CONTROL_SOURCE     (0x0400)
#define AVB_1722_1_ADP_TALKER_CAPABILITIES_MEDIA_CLOCK_SOURCE (0x0800)
#define AVB_1722_1_ADP_TALKER_CAPABILITIES_SMPTE_SOURCE       (0x1000)
#define AVB_1722_1_ADP_TALKER_CAPABILITIES_MIDI_SOURCE        (0x2000)
#define AVB_1722_1_ADP_TALKER_CAPABILITIES_AUDIO_SOURCE       (0x4000)
#define AVB_1722_1_ADP_TALKER_CAPABILITIES_VIDEO_SOURCE       (0x8000)

#define AVB_1722_1_ADP_LISTENER_CAPABILITIES_IMPLEMENTED      (0x0001)
#define AVB_1722_1_ADP_LISTENER_CAPABILITIES_OTHER_SINK       (0x0200)
#define AVB_1722_1_ADP_LISTENER_CAPABILITIES_CONTROL_SINK     (0x0400)
#define AVB_1722_1_ADP_LISTENER_CAPABILITIES_MEDIA_CLOCK_SINK (0x0800)
#define AVB_1722_1_ADP_LISTENER_CAPABILITIES_SMPTE_SINK       (0x1000)
#define AVB_1722_1_ADP_LISTENER_CAPABILITIES_MIDI_SINK        (0x2000)
#define AVB_1722_1_ADP_LISTENER_CAPABILITIES_AUDIO_SINK       (0x4000)
#define AVB_1722_1_ADP_LISTENER_CAPABILITIES_VIDEO_SINK       (0x8000)

#define AVB_1722_1_ADP_CONTROLLER_CAPABILITIES_IMPLEMENTED  (0x0001)
#define AVB_1722_1_ADP_CONTROLLER_CAPABILITIES_LAYER3_PROXY (0x0002)

/**
 *  A 1722.1 AVDECC Discovery Protocol packet
 *
 *  \note all elements 16 bit aligned
 */
typedef struct {
    avb_1722_1_packet_header_t header;
    uint8_t entity_guid[GUID_NUM_BYTES];
    uint8_t vendor_id[4];
    uint8_t entity_model_id[4];
    uint8_t entity_capabilities[4];
    uint8_t talker_stream_sources[2];
    uint8_t talker_capabilities[2];
    uint8_t listener_stream_sinks[2];
    uint8_t listener_capabilities[2];
    uint8_t controller_capabilities[4];
    uint8_t available_index[4];
    uint8_t gptp_grandmaster_id[GUID_NUM_BYTES];
    uint8_t gptp_domain_number;
    uint8_t reserved0[3];
    uint8_t identify_control_index[2];
    uint8_t interface_index[2];
    uint8_t association_id[GUID_NUM_BYTES];
    uint8_t reserved1[4];
} avb_1722_1_adp_packet_t;

#define AVB_1722_1_ADP_PACKET_SIZE (sizeof(ethernet_hdr_t) + sizeof(avb_1722_1_adp_packet_t))

typedef struct {
    guid_t guid;
    uint32_t vendor_id;
    uint32_t entity_model_id;
    uint32_t capabilities;
    uint16_t talker_stream_sources;
    uint16_t talker_capabilities;
    uint16_t listener_stream_sinks;
    uint16_t listener_capabilities;
    uint32_t controller_capabilities;
    uint32_t available_index;
    gmid_t gptp_grandmaster_id;
    uint8_t gptp_domain_number;
    uint16_t identify_control_index;
    uint32_t association_id;
    unsigned timeout;
} avb_1722_1_entity_record;

typedef enum {
    ENTITY_AVAILABLE = 0,
    ENTITY_DEPARTING = 1,
    ENTITY_DISCOVER = 2
} avb_1722_1_adp_message_type;
