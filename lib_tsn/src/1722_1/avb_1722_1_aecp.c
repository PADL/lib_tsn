// Copyright (c) 2011-2017, XMOS Ltd, All rights reserved
// Portions Copyright (c) 2024, PADL Software Pty Ltd, All rights reserved
#include <assert.h>
#include <inttypes.h>
#include "avb.h"
#include "avb_1722_common.h"
#include "avb_1722_1_common.h"
#include "avb_1722_1_aecp.h"
#include "avb_1722_1_adp.h"
#include "misc_timer.h"
#include "avb_srp_pdu.h"
#include <string.h>
#include <print.h>
#include "debug_print.h"
#include "xccompat.h"
#include "avb_1722_1.h"
#include "avb_1722_1_aecp_controls.h"
#include "reboot.h"
#include <xs1.h>
#include <platform.h>
#include "avb_util.h"
#include "avb_internal.h"
#include "ethernet_wrappers.h"
#include "aem_descriptor_types.h"
#if AVB_1722_1_AEM_ENABLED
#include "aem_descriptors.h"
#endif
#include "aem_descriptor_structs.h"

extern unsigned int avb_1722_1_buf[AVB_1722_1_PACKET_SIZE_WORDS];
extern guid_t my_guid;
extern uint8_t my_mac_addr[MACADDR_NUM_BYTES];

static uint8_t aecp_flash_page_buf[FLASH_PAGE_SIZE];
static size_t aecp_aa_bytes_copied;
static size_t aecp_aa_flash_write_addr;
static size_t aecp_aa_next_write_address;
static int operation_id = 0x1000;

static avb_timer aecp_aem_lock_timer;

static enum {
    AEM_ENTITY_NOT_ACQUIRED,
    AEM_ENTITY_ACQUIRED,
    AEM_ENTITY_ACQUIRED_AND_PERSISTENT,
    AEM_ENTITY_ACQUIRED_BUT_PENDING = 0x80000001
} entity_acquired_status = AEM_ENTITY_NOT_ACQUIRED;

static enum {
    AECP_AEM_CONTROLLER_AVAILABLE_IN_A = 0,
    AECP_AEM_CONTROLLER_AVAILABLE_IN_B,
    AECP_AEM_CONTROLLER_AVAILABLE_IN_C,
    AECP_AEM_CONTROLLER_AVAILABLE_IN_D,
    AECP_AEM_CONTROLLER_AVAILABLE_IN_E,
    AECP_AEM_CONTROLLER_AVAILABLE_IN_F,
    AECP_AEM_CONTROLLER_AVAILABLE_IDLE
} aecp_aem_controller_available_state = AECP_AEM_CONTROLLER_AVAILABLE_IDLE;

static avb_timer aecp_aem_controller_available_timer;
static guid_t pending_controller_guid;
static guid_t acquired_controller_guid;
static uint8_t acquired_controller_mac[MACADDR_NUM_BYTES];
static uint8_t pending_controller_mac[MACADDR_NUM_BYTES];
static uint16_t pending_controller_sequence;
static uint8_t pending_persistent;
static uint16_t aecp_controller_available_sequence = -1;

static void process_aem_cmd_register_unsolicited_notification(avb_1722_1_aecp_packet_t *pkt,
                                                              uint8_t src_addr[MACADDR_NUM_BYTES],
                                                              uint8_t *status);
static void process_aem_cmd_deregister_unsolicited_notification(avb_1722_1_aecp_packet_t *pkt,
                                                                uint8_t src_addr[MACADDR_NUM_BYTES],
                                                                uint8_t *status);

static enum {
    AECP_AEM_IDLE,
    AECP_AEM_WAITING,
    AECP_AEM_CONTROLLER_AVAILABLE_TIMEOUT,
    AECP_AEM_LOCK_TIMEOUT
} aecp_aem_state = AECP_AEM_IDLE;

// Unsolicited notification support.
#define AVB_1722_1_MAX_NOTIFICATION_CONTROLLERS 16
static size_t unsolicited_notification_controllers_count = 0;
static guid_t unsolicited_notification_controllers[AVB_1722_1_MAX_NOTIFICATION_CONTROLLERS];
static uint8_t unsolicited_notification_controller_macs[AVB_1722_1_MAX_NOTIFICATION_CONTROLLERS]
                                                       [MACADDR_NUM_BYTES];
static uint16_t unsolicited_sequence_id = -1;

static void send_unsolicited_notifications(CLIENT_INTERFACE(ethernet_tx_if, i_eth),
                                           size_t num_tx_bytes);

// Called on startup to initialise certain static descriptor fields
void avb_1722_1_aem_descriptors_init(unsigned int serial_num) {
#if AVB_1722_1_AEM_ENABLED
    if (AVB_1722_1_FIRMWARE_UPGRADE_ENABLED) {
        fl_BootImageInfo image;

        if (fl_getFactoryImage(&image) == 0) {
            if (fl_getNextBootImage(&image) == 0) {
                unsigned n = byterev(image.size);
                // Update length field of the memory object descriptor
                memcpy(&desc_upgrade_image_memory_object_0[96], &n, 4);
            }
        }
    }
    // entity_guid in Entity Descriptor
    for (int i = 0; i < 8; i++) {
        desc_entity[4 + i] = my_guid.c[7 - i];
    }

    avb_itoa((int)serial_num, (char *)&desc_entity[244], 10, 0);

    for (int i = 0; i < MACADDR_NUM_BYTES; i++) {
        // mac_address in AVB Interface Descriptor
        desc_avb_interface_0[70 + i] = my_mac_addr[i];
        // clock_source_identifier in clock source descriptor
        desc_clock_source_0[74 + i] = my_mac_addr[i];
    }

    // TODO: Should be stored centrally, possibly query PTP for ID per interface
    desc_avb_interface_0[78 + 0] = my_mac_addr[0];
    desc_avb_interface_0[78 + 1] = my_mac_addr[1];
    desc_avb_interface_0[78 + 2] = my_mac_addr[2];
    desc_avb_interface_0[78 + 3] = 0xff;
    desc_avb_interface_0[78 + 4] = 0xfe;
    desc_avb_interface_0[78 + 5] = my_mac_addr[3];
    desc_avb_interface_0[78 + 6] = my_mac_addr[4];
    desc_avb_interface_0[78 + 7] = my_mac_addr[5];
    desc_avb_interface_0[78 + 8] = 0;
    desc_avb_interface_0[78 + 9] = 1;
#endif
}

void avb_1722_1_aecp_aem_init(unsigned int serial_num) {
    avb_1722_1_aem_descriptors_init(serial_num);
    init_avb_timer(&aecp_aem_lock_timer, 100);
    init_avb_timer(&aecp_aem_controller_available_timer, 5);

    aecp_aem_state = AECP_AEM_WAITING;
}

static uint8_t *avb_1722_1_create_aecp_response_header(uint8_t dest_addr[MACADDR_NUM_BYTES],
                                                       uint8_t status,
                                                       uint8_t message_type,
                                                       size_t data_len,
                                                       avb_1722_1_aecp_packet_t *cmd_pkt) {
    struct ethernet_hdr_t *hdr = (ethernet_hdr_t *)&avb_1722_1_buf[0];
    avb_1722_1_aecp_packet_t *pkt =
        (avb_1722_1_aecp_packet_t *)(hdr + AVB_1722_1_PACKET_BODY_POINTER_OFFSET);

    avb_1722_1_create_1722_1_header(dest_addr, DEFAULT_1722_1_AECP_SUBTYPE, message_type + 1,
                                    status, data_len, hdr);

    // Copy the target guid, controller guid and sequence ID into the response
    // header
    memcpy(pkt->target_guid, cmd_pkt->target_guid, (pkt->data.payload - pkt->target_guid));

    return pkt->data.payload;
}

/* data_len: number of bytes of command_specific_data (9.2.1.2 Figure 9.2)
 */
static void avb_1722_1_create_aecp_aem_response(uint8_t src_addr[MACADDR_NUM_BYTES],
                                                uint8_t status,
                                                unsigned int command_data_len,
                                                avb_1722_1_aecp_packet_t *cmd_pkt) {
    /* 9.2.1.1.7: "control_data_length field for AECP is the number of octets
    following the target_guid, but is limited to a maximum of 524"

    control_data_length = payload_specific_data + sequence_id + controller_guid
    payload_specific_data (for AEM) = command_specific_data + command_type + u

    = command_specific_data + 2 + 2 + 8
    */
    avb_1722_1_aecp_aem_msg_t *aem =
        (avb_1722_1_aecp_aem_msg_t *)avb_1722_1_create_aecp_response_header(
            src_addr, status, AECP_CMD_AEM_COMMAND, command_data_len + 12, cmd_pkt);

    /* Copy payload_specific_data into the response */
    memcpy(aem, cmd_pkt->data.payload, command_data_len + 2);
}

#if (AVB_1722_1_AEM_ENABLED == 0)
__attribute__((unused))
#endif
static void
generate_object_name(char *object_name, int base, int n) {
    char num_string[5];
    int count;
    if (n) {
        count = avb_itoa(base * n + 1, num_string, 10, 1);
        num_string[count] = '-';
        count++; // Add a char for the '-'
        count += avb_itoa((base * n) + n, &num_string[count], 10, 0);
    } else {
        count = avb_itoa(base + 1, num_string, 10, 1);
    }
    num_string[count] = '\0';
    strcat(object_name, num_string);
}

static int create_aem_read_descriptor_response(unsigned int read_type,
                                               unsigned int read_id,
                                               uint8_t src_addr[MACADDR_NUM_BYTES],
                                               avb_1722_1_aecp_packet_t *pkt,
                                               CLIENT_INTERFACE(avb_interface, i_avb_api),
                                               CLIENT_INTERFACE(avb_1722_1_control_callbacks,
                                                                i_1722_1_entity)) {
#if AVB_1722_1_AEM_ENABLED
    int desc_size_bytes = 0, i = 0;
    uint8_t *descriptor = NULL;
    int found_descriptor = 0;

#if AEM_GENERATE_DESCRIPTORS_ON_FLY
    switch (read_type) {
    case AEM_AUDIO_CLUSTER_TYPE:
        if (read_id < (AVB_NUM_MEDIA_OUTPUTS + AVB_NUM_MEDIA_INPUTS)) {
            descriptor = &desc_audio_cluster_template[0];
            desc_size_bytes = sizeof(aem_desc_audio_cluster_t);
        }
        break;
#if (AVB_NUM_SOURCES > 0)
    case AEM_STREAM_INPUT_TYPE:
        if (read_id < AVB_NUM_SINKS) {
            descriptor = &desc_stream_input_0[0];
            desc_size_bytes = sizeof(desc_stream_input_0);
        }
        break;
    case AEM_STREAM_PORT_INPUT_TYPE:
        if (read_id < AVB_NUM_SINKS) {
            descriptor = &desc_stream_port_input_0[0];
            desc_size_bytes = sizeof(aem_desc_stream_port_input_output_t);
        }
        break;
#endif
#if (AVB_NUM_SOURCES > 0)
    case AEM_STREAM_OUTPUT_TYPE:
        if (read_id < AVB_NUM_SOURCES) {
            descriptor = &desc_stream_output_0[0];
            desc_size_bytes = sizeof(desc_stream_output_0);
        }
        break;
    case AEM_STREAM_PORT_OUTPUT_TYPE:
        if (read_id < AVB_NUM_SOURCES) {
            descriptor = &desc_stream_port_output_0[0];
            desc_size_bytes = sizeof(aem_desc_stream_port_input_output_t);
        }
        break;
#endif
    }

    if (descriptor != NULL) {
        aem_desc_audio_cluster_t *cluster = (aem_desc_audio_cluster_t *)descriptor;
        char id_num = (char)read_id;

        // The descriptor id is also the channel number
        cluster->descriptor_index[1] = (uint8_t)read_id;

        if ((read_type == AEM_AUDIO_CLUSTER_TYPE) || read_type == AEM_STREAM_OUTPUT_TYPE) {
            int id = (int)read_id;
            ;
            if (read_id >= AVB_NUM_MEDIA_OUTPUTS) {
                id = (int)read_id - AVB_NUM_MEDIA_OUTPUTS;
            }
            memset(cluster->object_name, 0, 64);
            if (read_type == AEM_AUDIO_CLUSTER_TYPE) {
                strcpy((char *)cluster->object_name, "Channel ");
                generate_object_name((char *)cluster->object_name, id, 0);
            }
#if AVB_NUM_SOURCES > 0
            else {
                strcpy((char *)cluster->object_name, "Output ");
                generate_object_name((char *)cluster->object_name, id,
                                     AVB_NUM_MEDIA_INPUTS / AVB_NUM_SOURCES);
            }
#endif
        }
#if AVB_NUM_SINKS > 0
        else if (read_type == AEM_STREAM_INPUT_TYPE) {
            memset(cluster->object_name, 0, 64);
            strcpy((char *)cluster->object_name, "Input ");
            generate_object_name((char *)cluster->object_name, (int)id_num,
                                 AVB_NUM_MEDIA_OUTPUTS / AVB_NUM_SINKS);
        }
#endif
        if (read_type == AEM_STREAM_PORT_OUTPUT_TYPE) {
#if AVB_NUM_SOURCES > 0
            aem_desc_stream_port_input_output_t *stream_port =
                (aem_desc_stream_port_input_output_t *)descriptor;
            hton_16(stream_port->base_cluster,
                    AVB_NUM_MEDIA_OUTPUTS + (read_id * AVB_NUM_MEDIA_INPUTS / AVB_NUM_SOURCES));
            hton_16(stream_port->base_map, AVB_NUM_SOURCES + read_id);
#endif
        } else if (read_type == AEM_STREAM_PORT_INPUT_TYPE) {
#if AVB_NUM_SINKS > 0
            aem_desc_stream_port_input_output_t *stream_port =
                (aem_desc_stream_port_input_output_t *)descriptor;
            hton_16(stream_port->base_cluster, read_id * AVB_NUM_MEDIA_OUTPUTS / AVB_NUM_SINKS);
            hton_16(stream_port->base_map, read_id);
#endif
        }

        found_descriptor = 1;
    } else if (read_type == AEM_AUDIO_MAP_TYPE) {
        if (read_id < (AVB_NUM_SINKS + AVB_NUM_SOURCES)) {
#if (AVB_NUM_SINKS > 0 && AVB_NUM_SOURCES > 0)
            const size_t num_mappings = (read_id < AVB_NUM_SINKS)
                                            ? AVB_NUM_MEDIA_OUTPUTS / AVB_NUM_SINKS
                                            : AVB_NUM_MEDIA_INPUTS / AVB_NUM_SOURCES;
#elif (AVB_NUM_SOURCES > 0)
            const size_t num_mappings =
                (read_id < AVB_NUM_SOURCES) ? AVB_NUM_MEDIA_INPUTS / AVB_NUM_SOURCES : 0;
#else
            const size_t num_mappings =
                (read_id < AVB_NUM_SINKS) ? AVB_NUM_MEDIA_OUTPUTS / AVB_NUM_SINKS : 0;
#endif

            /* Since the map descriptors aren't constant size, unlike the
             * clusters, and dependent on the number of channels, we don't use a
             * template */

            struct ethernet_hdr_t *hdr = (ethernet_hdr_t *)&avb_1722_1_buf[0];
            avb_1722_1_aecp_packet_t *pkt =
                (avb_1722_1_aecp_packet_t *)(hdr + AVB_1722_1_PACKET_BODY_POINTER_OFFSET);
            avb_1722_1_aecp_aem_msg_t *aem = (avb_1722_1_aecp_aem_msg_t *)(pkt->data.payload);
            uint8_t *pktptr = (uint8_t *)&(aem->command.read_descriptor_resp.descriptor);
            aem_desc_audio_map_t *audio_map = (aem_desc_audio_map_t *)pktptr;

            desc_size_bytes = 8 + (num_mappings * 8);

            memset(audio_map, 0, desc_size_bytes);
            hton_16(audio_map->descriptor_type, AEM_AUDIO_MAP_TYPE);
            hton_16(audio_map->descriptor_index, read_id);
            hton_16(audio_map->mappings_offset, 8);
            hton_16(audio_map->number_of_mappings, num_mappings);

            for (int i = 0; i < num_mappings; i++) {
                hton_16(audio_map->mappings[i].mapping_stream_index, read_id % AVB_NUM_SINKS);
                hton_16(audio_map->mappings[i].mapping_stream_channel, i);
                hton_16(audio_map->mappings[i].mapping_cluster_offset, i);
                hton_16(audio_map->mappings[i].mapping_cluster_channel,
                        0); // Single channel audio clusters
            }

            found_descriptor = 2; // 2 signifies do not copy descriptor below
        }
    } else
#endif
    {
        /* Search for the descriptor */
        while (aem_descriptor_list[i] <= read_type) {
            size_t num_descriptors = aem_descriptor_list[i + 1];

            if (aem_descriptor_list[i] == read_type) {
                for (size_t j = 0, k = 2; j < num_descriptors; j++, k += 2) {
                    desc_size_bytes = aem_descriptor_list[i + k];
                    descriptor = (uint8_t *)aem_descriptor_list[i + k + 1];

                    if ((((unsigned)descriptor[2] << 8) | ((unsigned)descriptor[3])) == read_id) {
                        found_descriptor = 1;
                        break;
                    }
                }
            }

            i += ((num_descriptors * 2) + 2);
            if (i >= (sizeof(aem_descriptor_list) >> 2))
                break;
        }
    }

    if (found_descriptor) {
        size_t packet_size =
            sizeof(ethernet_hdr_t) + sizeof(avb_1722_1_packet_header_t) + 24 + desc_size_bytes;

        avb_1722_1_aecp_aem_msg_t *aem =
            (avb_1722_1_aecp_aem_msg_t *)avb_1722_1_create_aecp_response_header(
                src_addr, AECP_AEM_STATUS_SUCCESS, AECP_CMD_AEM_COMMAND, desc_size_bytes + 16, pkt);

        memcpy(aem, pkt->data.payload, 6);
        if (found_descriptor < 2)
            memcpy(&(aem->command.read_descriptor_resp.descriptor), descriptor,
                   desc_size_bytes + 40);
        set_current_fields_in_descriptor(aem->command.read_descriptor_resp.descriptor,
                                         desc_size_bytes, read_type, read_id, i_avb_api,
                                         i_1722_1_entity);
        return packet_size;
    } else // Descriptor not found, send NO_SUCH_DESCRIPTOR reply
    {
        size_t packet_size = sizeof(ethernet_hdr_t) + sizeof(avb_1722_1_packet_header_t) + 20 +
                             sizeof(avb_1722_1_aem_read_descriptor_command_t);

        avb_1722_1_aecp_aem_msg_t *aem =
            (avb_1722_1_aecp_aem_msg_t *)avb_1722_1_create_aecp_response_header(
                src_addr, AECP_AEM_STATUS_NO_SUCH_DESCRIPTOR, AECP_CMD_AEM_COMMAND, 40, pkt);

        memcpy(aem, pkt->data.payload, 20 + sizeof(avb_1722_1_aem_read_descriptor_command_t));

        return packet_size;
    }
#else
    return 0;
#endif
}

static uint16_t avb_1722_1_create_controller_available_packet(void) {
    struct ethernet_hdr_t *hdr = (ethernet_hdr_t *)&avb_1722_1_buf[0];
    avb_1722_1_aecp_packet_t *pkt =
        (avb_1722_1_aecp_packet_t *)(hdr + AVB_1722_1_PACKET_BODY_POINTER_OFFSET);
    avb_1722_1_aecp_aem_msg_t *aem_msg = &(pkt->data.aem);

    avb_1722_1_create_1722_1_header(acquired_controller_mac, DEFAULT_1722_1_AECP_SUBTYPE,
                                    AECP_CMD_AEM_COMMAND, AECP_AEM_STATUS_SUCCESS,
                                    AVB_1722_1_AECP_COMMAND_DATA_OFFSET, hdr);

    set_64(pkt->target_guid, acquired_controller_guid.c);
    set_64(pkt->controller_guid, my_guid.c);
    hton_16(pkt->sequence_id, aecp_controller_available_sequence);

    AEM_MSG_SET_COMMAND_TYPE(aem_msg, AECP_AEM_CMD_CONTROLLER_AVAILABLE);
    AEM_MSG_SET_U_FLAG(aem_msg, 0);

    return AVB_1722_1_AECP_PAYLOAD_OFFSET;
}

static uint16_t avb_1722_1_create_acquire_response_packet(uint8_t status) {
    struct ethernet_hdr_t *hdr = (ethernet_hdr_t *)&avb_1722_1_buf[0];
    avb_1722_1_aecp_packet_t *pkt =
        (avb_1722_1_aecp_packet_t *)(hdr + AVB_1722_1_PACKET_BODY_POINTER_OFFSET);
    avb_1722_1_aecp_aem_msg_t *aem_msg = &(pkt->data.aem);

    aem_msg->command.acquire_entity_cmd.flags[0] = pending_persistent;
    set_64(aem_msg->command.acquire_entity_cmd.owner_guid, acquired_controller_guid.c);
    hton_16(aem_msg->command.acquire_entity_cmd.descriptor_type, 0);
    hton_16(aem_msg->command.acquire_entity_cmd.descriptor_id, 0);

    avb_1722_1_create_1722_1_header(
        pending_controller_mac, DEFAULT_1722_1_AECP_SUBTYPE, AECP_CMD_AEM_RESPONSE, status,
        AVB_1722_1_AECP_COMMAND_DATA_OFFSET + sizeof(avb_1722_1_aem_acquire_entity_command_t), hdr);

    set_64(pkt->target_guid, my_guid.c);
    set_64(pkt->controller_guid, pending_controller_guid.c);
    hton_16(pkt->sequence_id, pending_controller_sequence);

    AEM_MSG_SET_COMMAND_TYPE(aem_msg, AECP_AEM_CMD_ACQUIRE_ENTITY);
    AEM_MSG_SET_U_FLAG(aem_msg, 0);

    return sizeof(avb_1722_1_aem_acquire_entity_command_t) + AVB_1722_1_AECP_PAYLOAD_OFFSET;
}

static uint16_t process_aem_cmd_acquire(avb_1722_1_aecp_packet_t *pkt,
                                        uint8_t *status,
                                        uint8_t src_addr[MACADDR_NUM_BYTES],
                                        CLIENT_INTERFACE(ethernet_tx_if, i_eth)) {
    uint16_t descriptor_index = ntoh_16(pkt->data.aem.command.acquire_entity_cmd.descriptor_id);
    uint16_t descriptor_type = ntoh_16(pkt->data.aem.command.acquire_entity_cmd.descriptor_type);

    if (AEM_ENTITY_TYPE == descriptor_type && 0 == descriptor_index) {
        if (AEM_ACQUIRE_ENTITY_RELEASE_FLAG(&(pkt->data.aem.command.acquire_entity_cmd))) {
            // Release
            if (entity_acquired_status == AEM_ENTITY_NOT_ACQUIRED) {
                *status = AECP_AEM_STATUS_BAD_ARGUMENTS;
            } else if (compare_guid(pkt->controller_guid, &acquired_controller_guid)) {
                *status = AECP_AEM_STATUS_SUCCESS;
                entity_acquired_status = AEM_ENTITY_NOT_ACQUIRED;
                debug_printf("1722.1 Controller %x%x released entity\n",
                             acquired_controller_guid.l << 32, acquired_controller_guid.l);
                zero_guid(&acquired_controller_guid);
                memset(&acquired_controller_mac, 0, 6);
            } else {
                *status = AECP_AEM_STATUS_ENTITY_ACQUIRED;
                hton_guid(pkt->data.aem.command.acquire_entity_cmd.owner_guid,
                          &acquired_controller_guid);
            }
        } else {
            // Acquire

            switch (entity_acquired_status) {
            case AEM_ENTITY_NOT_ACQUIRED:
                *status = AECP_AEM_STATUS_SUCCESS;
                if (AEM_ACQUIRE_ENTITY_PERSISTENT_FLAG(
                        &(pkt->data.aem.command.acquire_entity_cmd))) {
                    entity_acquired_status = AEM_ENTITY_ACQUIRED_AND_PERSISTENT;
                } else {
                    entity_acquired_status = AEM_ENTITY_ACQUIRED;
                }

                ntoh_guid(&acquired_controller_guid, pkt->controller_guid);
                hton_guid(pkt->data.aem.command.acquire_entity_cmd.owner_guid,
                          &acquired_controller_guid);
                debug_printf("1722.1 Controller %x%x acquired entity\n",
                             acquired_controller_guid.l << 32, acquired_controller_guid.l);
                memcpy(&acquired_controller_mac, &src_addr, 6);
                break;

            case AEM_ENTITY_ACQUIRED_BUT_PENDING:
                break;

            case AEM_ENTITY_ACQUIRED:
                if (compare_guid(pkt->controller_guid, &acquired_controller_guid)) {
                    *status = AECP_AEM_STATUS_SUCCESS;
                } else {
                    *status = AECP_AEM_STATUS_IN_PROGRESS;
                    ntoh_guid(&pending_controller_guid, pkt->controller_guid);
                    memcpy(&pending_controller_mac, &src_addr, 6);
                    pending_controller_sequence = ntoh_16(pkt->sequence_id);
                    pending_persistent = AEM_ACQUIRE_ENTITY_PERSISTENT_FLAG(
                        &(pkt->data.aem.command.acquire_entity_cmd));

                    aecp_controller_available_sequence++;

                    avb_1722_1_create_controller_available_packet();
                    eth_send_packet(i_eth, (char *)avb_1722_1_buf, 64, ETHERNET_ALL_INTERFACES);

                    start_avb_timer(&aecp_aem_controller_available_timer, 12);
                    aecp_aem_controller_available_state = AECP_AEM_CONTROLLER_AVAILABLE_IN_A;
                    entity_acquired_status = AEM_ENTITY_ACQUIRED_BUT_PENDING;
                }

                hton_guid(pkt->data.aem.command.acquire_entity_cmd.owner_guid,
                          &acquired_controller_guid);
                break;
            case AEM_ENTITY_ACQUIRED_AND_PERSISTENT:
                if (compare_guid(pkt->controller_guid, &acquired_controller_guid)) {
                    *status = AECP_AEM_STATUS_SUCCESS;
                } else {
                    *status = AECP_AEM_STATUS_ENTITY_ACQUIRED;
                }

                hton_guid(pkt->data.aem.command.acquire_entity_cmd.owner_guid,
                          &acquired_controller_guid);
                break;
            }
        }
    } else {
        *status = AECP_AEM_STATUS_NOT_SUPPORTED;
    }

    return GET_1722_1_DATALENGTH(&pkt->header) - AVB_1722_1_AECP_COMMAND_DATA_OFFSET;
}

static int process_aem_cmd_start_abort_operation(avb_1722_1_aecp_packet_t *pkt,
                                                 uint8_t src_addr[MACADDR_NUM_BYTES],
                                                 uint8_t *status,
                                                 uint16_t command_type,
                                                 CLIENT_INTERFACE(ethernet_tx_if, i_eth),
                                                 uint8_t *reboot) {
    avb_1722_1_aem_start_operation_t *cmd =
        (avb_1722_1_aem_start_operation_t *)(pkt->data.aem.command.payload);
    uint16_t desc_type = ntoh_16(cmd->descriptor_type);
    uint16_t desc_id = ntoh_16(cmd->descriptor_id);
    uint16_t operation_type = ntoh_16(cmd->operation_type);

    if (command_type == AECP_AEM_CMD_START_OPERATION && desc_type == AEM_MEMORY_OBJECT_TYPE &&
        desc_id == 0) // descriptor ID of the AEM_MEMORY_OBJECT_TYPE descriptor
    {
        int num_tx_bytes =
            sizeof(avb_1722_1_aem_start_operation_t) + AVB_1722_1_AECP_PAYLOAD_OFFSET;
        if (num_tx_bytes < 64)
            num_tx_bytes = 64;
        switch (operation_type) {
        case AEM_MEMORY_OBJECT_OPERATION_UPLOAD:
        case AEM_MEMORY_OBJECT_OPERATION_ERASE: {
            fl_BootImageInfo image;
            int flashstatus = fl_getFactoryImage(&image);

            if (flashstatus != 0) {
                debug_printf("No factory image!\n");
                *status = AECP_AEM_STATUS_ENTITY_MISBEHAVING;
                return 0;
            } else {
                flashstatus = fl_getNextBootImage(&image);
                if (flashstatus != 0) {
                    // No upgrade image exists in flash
                    debug_printf("No upgrade\n");
                }

                int result;
                int t = get_local_time();
                const unsigned in_progress_msg_interval_ms = 120 * XS1_TIMER_KHZ;
                do {
                    if (flashstatus != 0) {
                        result = fl_startImageAdd(&image, FLASH_MAX_UPGRADE_IMAGE_SIZE, 0);
                    } else {
                        result = fl_startImageReplace(&image, FLASH_MAX_UPGRADE_IMAGE_SIZE);
                    }
                    if ((result > 0) && (get_local_time() - t >= in_progress_msg_interval_ms)) {
                        t = get_local_time();
                        avb_1722_1_create_aecp_aem_response(src_addr, AECP_AEM_STATUS_IN_PROGRESS,
                                                            GET_1722_1_DATALENGTH(&pkt->header),
                                                            pkt);
                        eth_send_packet(i_eth, (char *)avb_1722_1_buf, num_tx_bytes,
                                        ETHERNET_ALL_INTERFACES);
                    }
                } while (result > 0);

                if (result < 0) {
                    debug_printf("Failed to start image upgrade\n");
                    *status = AECP_AEM_STATUS_ENTITY_MISBEHAVING;
                } else {
                    begin_write_upgrade_image();
                }
            }

            avb_1722_1_create_aecp_aem_response(src_addr, *status,
                                                GET_1722_1_DATALENGTH(&pkt->header), pkt);
            eth_send_packet(i_eth, (char *)avb_1722_1_buf, num_tx_bytes, ETHERNET_ALL_INTERFACES);

            return 0;
            break;
        }
        case AEM_MEMORY_OBJECT_OPERATION_STORE:
        case AEM_MEMORY_OBJECT_OPERATION_STORE_AND_REBOOT: {
            hton_16(cmd->operation_id, operation_id++);

            avb_1722_1_create_aecp_aem_response(src_addr, AECP_AEM_STATUS_SUCCESS,
                                                GET_1722_1_DATALENGTH(&pkt->header), pkt);
            eth_send_packet(i_eth, (char *)avb_1722_1_buf, num_tx_bytes, ETHERNET_ALL_INTERFACES);

            avb_1722_1_aecp_aem_msg_t *aem_msg = &(pkt->data.aem);
            AEM_MSG_SET_U_FLAG(aem_msg, 1);
            AEM_MSG_SET_COMMAND_TYPE(aem_msg, AECP_AEM_CMD_OPERATION_STATUS);
            avb_1722_1_aem_operation_status_t *resp =
                (avb_1722_1_aem_operation_status_t *)(pkt->data.aem.command.payload);

            hton_16(resp->percent_complete, 1000);
            avb_1722_1_create_aecp_aem_response(src_addr, AECP_AEM_STATUS_SUCCESS,
                                                GET_1722_1_DATALENGTH(&pkt->header), pkt);
            eth_send_packet(i_eth, (char *)avb_1722_1_buf, num_tx_bytes, ETHERNET_ALL_INTERFACES);

            if (operation_type == AEM_MEMORY_OBJECT_OPERATION_STORE_AND_REBOOT) {
                *reboot = 1;
            }

            return 0;
        }
        default: {
            *status = AECP_AEM_STATUS_BAD_ARGUMENTS; // Other operation types
                                                     // not supported
            break;
        }
        }
    } else if (command_type == AECP_AEM_CMD_ABORT_OPERATION) {
        aecp_aa_bytes_copied = 0;
        aecp_aa_flash_write_addr = 0;
        aecp_aa_next_write_address = 0;
    } else {
        *status = AECP_AEM_STATUS_NO_SUCH_DESCRIPTOR;
    }
    return GET_1722_1_DATALENGTH(&pkt->header) - AVB_1722_1_AECP_COMMAND_DATA_OFFSET;
}

static int is_notifiable_command_type_p(uint16_t command_type) {
    switch (command_type) {
    case AECP_AEM_CMD_ACQUIRE_ENTITY:
    case AECP_AEM_CMD_LOCK_ENTITY:
    case AECP_AEM_CMD_WRITE_DESCRIPTOR:
    case AECP_AEM_CMD_SET_CONFIGURATION:
    case AECP_AEM_CMD_SET_STREAM_FORMAT:
    case AECP_AEM_CMD_SET_VIDEO_FORMAT:
    case AECP_AEM_CMD_SET_SENSOR_FORMAT:
    case AECP_AEM_CMD_SET_STREAM_INFO:
    case AECP_AEM_CMD_SET_NAME:
    case AECP_AEM_CMD_SET_ASSOCIATION_ID:
    case AECP_AEM_CMD_SET_SAMPLING_RATE:
    case AECP_AEM_CMD_SET_CLOCK_SOURCE:
    case AECP_AEM_CMD_SET_CONTROL:
    case AECP_AEM_CMD_INCREMENT_CONTROL:
    case AECP_AEM_CMD_DECREMENT_CONTROL:
    case AECP_AEM_CMD_SET_SIGNAL_SELECTOR:
    case AECP_AEM_CMD_SET_MIXER:
    case AECP_AEM_CMD_SET_MATRIX:
    case AECP_AEM_CMD_START_STREAMING:
    case AECP_AEM_CMD_STOP_STREAMING:
    case AECP_AEM_CMD_DEREGISTER_UNSOLICITED_NOTIFICATION:
    case AECP_AEM_CMD_GET_COUNTERS:
    case AECP_AEM_CMD_REBOOT:
    case AECP_AEM_CMD_ADD_AUDIO_MAPPINGS:
    case AECP_AEM_CMD_REMOVE_AUDIO_MAPPINGS:
    case AECP_AEM_CMD_ADD_VIDEO_MAPPINGS:
    case AECP_AEM_CMD_REMOVE_VIDEO_MAPPINGS:
    case AECP_AEM_CMD_ADD_SENSOR_MAPPINGS:
    case AECP_AEM_CMD_REMOVE_SENSOR_MAPPINGS:
        // case AECP_AEM_CMD_SET_MAX_TRANSIT_TIME:
        // case AECP_AEM_CMD_SET_SAMPLING_RATE_RANGE:
        // case AECP_AEM_CMD_SET_PTP_INSTANCE_INFO:
        // case AECP_AEM_CMD_SET_PTP_PORT_INITIAL_INTERVALS:
        // case AECP_AEM_CMD_SET_PTP_PORT_CURRENT_INTERVALS:
        // case AECP_AEM_CMD_SET_PTP_PORT_INFO:
        // case AECP_AEM_CMD_SET_PTP_PORT_OVERRIDES:
        return 1;
    default:
        return 0;
    }
}

static void process_avb_1722_1_aecp_aem_msg(avb_1722_1_aecp_packet_t *pkt,
                                            uint8_t src_addr[MACADDR_NUM_BYTES],
                                            int message_type,
                                            int num_pkt_bytes,
                                            CLIENT_INTERFACE(ethernet_tx_if, i_eth),
                                            CLIENT_INTERFACE(avb_interface, i_avb_api),
                                            CLIENT_INTERFACE(avb_1722_1_control_callbacks,
                                                             i_1722_1_entity),
                                            chanend c_ptp) {
    avb_1722_1_aecp_aem_msg_t *aem_msg = &(pkt->data.aem);
    uint16_t command_type = AEM_MSG_GET_COMMAND_TYPE(aem_msg);
    uint8_t status = AECP_AEM_STATUS_SUCCESS;
    size_t cd_len = 0;
    uint8_t reboot = 0;
    uint8_t force_send = 0;

    if (message_type == AECP_CMD_AEM_COMMAND) {
        if (compare_guid(pkt->target_guid, &my_guid) == 0)
            return; // not for us

        switch (command_type) {
        case AECP_AEM_CMD_ACQUIRE_ENTITY: // Long term exclusive control of the
                                          // entity
        {
            cd_len = process_aem_cmd_acquire(pkt, &status, src_addr, i_eth);
            break;
        }
        case AECP_AEM_CMD_LOCK_ENTITY: // Atomic operation on the entity
        {
            break;
        }
        case AECP_AEM_CMD_ENTITY_AVAILABLE: {
            cd_len = AVB_1722_1_AECP_PAYLOAD_OFFSET;
            break;
        }
        case AECP_AEM_CMD_REBOOT: {
            // Reply before reboot, do the reboot after sending the packet
            cd_len = AVB_1722_1_AECP_PAYLOAD_OFFSET;
            reboot = 1;
            break;
        }
        case AECP_AEM_CMD_READ_DESCRIPTOR: {
            unsigned int desc_read_type, desc_read_id;
            int num_tx_bytes;

            desc_read_type = ntoh_16(aem_msg->command.read_descriptor_cmd.descriptor_type);
            desc_read_id = ntoh_16(aem_msg->command.read_descriptor_cmd.descriptor_id);

            num_tx_bytes = create_aem_read_descriptor_response(
                desc_read_type, desc_read_id, src_addr, pkt, i_avb_api, i_1722_1_entity);

            if (num_tx_bytes < 64)
                num_tx_bytes = 64;

            eth_send_packet(i_eth, (char *)avb_1722_1_buf, num_tx_bytes, ETHERNET_ALL_INTERFACES);

            break;
        }
        case AECP_AEM_CMD_GET_AVB_INFO: {
            process_aem_cmd_get_avb_info(pkt, &status, i_avb_api, c_ptp);
            cd_len = sizeof(avb_1722_1_aem_get_avb_info_response_t);
            break;
        }
        case AECP_AEM_CMD_GET_STREAM_INFO:
        case AECP_AEM_CMD_SET_STREAM_INFO: // Fallthrough intentional
        {
            process_aem_cmd_getset_stream_info(pkt, &status, command_type, i_avb_api);
            cd_len = sizeof(avb_1722_1_aem_getset_stream_info_t);
            break;
        }
        case AECP_AEM_CMD_GET_STREAM_FORMAT:
        case AECP_AEM_CMD_SET_STREAM_FORMAT: // Fallthrough intentional
        {
            process_aem_cmd_getset_stream_format(pkt, &status, command_type, i_avb_api);
            if (command_type == AECP_AEM_CMD_GET_STREAM_FORMAT)
                cd_len = sizeof(avb_1722_1_aem_get_stream_info_ex_t);
            else
                cd_len = sizeof(avb_1722_1_aem_getset_stream_format_t);
            break;
        }
        case AECP_AEM_CMD_GET_SAMPLING_RATE:
        case AECP_AEM_CMD_SET_SAMPLING_RATE: {
            process_aem_cmd_getset_sampling_rate(pkt, &status, command_type, i_avb_api);
            cd_len = sizeof(avb_1722_1_aem_getset_sampling_rate_t);
            break;
        }
        case AECP_AEM_CMD_GET_CLOCK_SOURCE:
        case AECP_AEM_CMD_SET_CLOCK_SOURCE: {
            process_aem_cmd_getset_clock_source(pkt, &status, command_type, i_avb_api);
            cd_len = sizeof(avb_1722_1_aem_getset_clock_source_t);
            break;
        }
        case AECP_AEM_CMD_START_STREAMING:
        case AECP_AEM_CMD_STOP_STREAMING: {
            process_aem_cmd_startstop_streaming(pkt, &status, command_type, i_avb_api);
            cd_len = sizeof(avb_1722_1_aem_startstop_streaming_t);
            break;
        }
        case AECP_AEM_CMD_REGISTER_UNSOLICITED_NOTIFICATION:
            process_aem_cmd_register_unsolicited_notification(pkt, src_addr, &status);
            cd_len = 0; // FIXME: return flags
            force_send = 1;
            break;
        case AECP_AEM_CMD_DEREGISTER_UNSOLICITED_NOTIFICATION:
            process_aem_cmd_deregister_unsolicited_notification(pkt, src_addr, &status);
            cd_len = 0;
            force_send = 1;
            break;
        case AECP_AEM_CMD_GET_CONTROL:
        case AECP_AEM_CMD_SET_CONTROL: {
            cd_len = process_aem_cmd_getset_control(pkt, &status, command_type, i_1722_1_entity) +
                     sizeof(avb_1722_1_aem_getset_control_t) + AVB_1722_1_AECP_COMMAND_DATA_OFFSET;
            break;
        }
        case AECP_AEM_CMD_GET_SIGNAL_SELECTOR:
        case AECP_AEM_CMD_SET_SIGNAL_SELECTOR: {
            process_aem_cmd_getset_signal_selector(pkt, &status, command_type, i_1722_1_entity);
            cd_len = sizeof(avb_1722_1_aem_getset_signal_selector_t);
            break;
        }
        case AECP_AEM_CMD_GET_COUNTERS: {
            process_aem_cmd_get_counters(pkt, &status, i_avb_api);
            cd_len = sizeof(avb_1722_1_aem_get_counters_t);
            break;
        }
        case AECP_AEM_CMD_START_OPERATION:
        case AECP_AEM_CMD_ABORT_OPERATION: {
            if (AVB_1722_1_FIRMWARE_UPGRADE_ENABLED) {
                cd_len = process_aem_cmd_start_abort_operation(pkt, src_addr, &status, command_type,
                                                               i_eth, &reboot);
            }
            break;
        }
        default: {
            unsigned num_tx_bytes = num_pkt_bytes + sizeof(ethernet_hdr_t);
            if (num_tx_bytes < 64)
                num_tx_bytes = 64;
            status = AECP_AEM_STATUS_NOT_IMPLEMENTED;
            avb_1722_1_aecp_aem_msg_t *aem =
                (avb_1722_1_aecp_aem_msg_t *)avb_1722_1_create_aecp_response_header(
                    src_addr, status, AECP_CMD_AEM_COMMAND, GET_1722_1_DATALENGTH(&pkt->header),
                    pkt);
            memcpy(aem, pkt->data.payload, num_pkt_bytes - AVB_1722_1_AECP_PAYLOAD_OFFSET);
            eth_send_packet(i_eth, (char *)avb_1722_1_buf, num_tx_bytes, ETHERNET_ALL_INTERFACES);
            return;
        }
        }

        // Send a response if required
        if (cd_len > 0 || force_send) {
            avb_1722_1_create_aecp_aem_response(src_addr, status, cd_len, pkt);
        }
    } else // AECP_CMD_AEM_RESPONSE
    {
        switch (command_type) {
        case AECP_AEM_CMD_CONTROLLER_AVAILABLE:
            if (AEM_ENTITY_ACQUIRED_BUT_PENDING == entity_acquired_status &&
                compare_guid(pkt->controller_guid, &my_guid)) {
                if (compare_guid(pkt->target_guid, &pending_controller_guid)) {
                    entity_acquired_status = AEM_ENTITY_ACQUIRED;
                    aecp_aem_controller_available_state = AECP_AEM_CONTROLLER_AVAILABLE_IDLE;

                    cd_len = avb_1722_1_create_acquire_response_packet(AECP_AEM_STATUS_SUCCESS);
                } else if (compare_guid(pkt->target_guid, &acquired_controller_guid)) {
                    if (AEM_ACQUIRE_ENTITY_PERSISTENT_FLAG(
                            &(pkt->data.aem.command.acquire_entity_cmd))) {
                        entity_acquired_status = AEM_ENTITY_ACQUIRED_AND_PERSISTENT;
                    } else {
                        entity_acquired_status = AEM_ENTITY_ACQUIRED;
                    }

                    cd_len =
                        avb_1722_1_create_acquire_response_packet(AECP_AEM_STATUS_ENTITY_ACQUIRED);
                    stop_avb_timer(&aecp_aem_controller_available_timer);
                }
            }
            break;
        default:
            break;
        }
    }

    if (cd_len > 0 || force_send) {
        int num_tx_bytes = cd_len + 2 + // U Flag + command type
                           AVB_1722_1_AECP_PAYLOAD_OFFSET + sizeof(ethernet_hdr_t);

        if (num_tx_bytes < 64)
            num_tx_bytes = 64;

        eth_send_packet(i_eth, (char *)avb_1722_1_buf, num_tx_bytes, ETHERNET_ALL_INTERFACES);

        // then send to all notifiers
        if (status == AECP_AEM_STATUS_SUCCESS && is_notifiable_command_type_p(command_type)) {
            send_unsolicited_notifications(i_eth, num_tx_bytes);
        }
    }
    if (reboot) {
        avb_1722_1_adp_depart_immediately(i_eth);
        waitfor(10000); // Wait for the response packet to egress
        device_reboot();
    }
}

static void process_avb_1722_1_aecp_address_access_cmd(avb_1722_1_aecp_packet_t *pkt,
                                                       uint8_t src_addr[MACADDR_NUM_BYTES],
                                                       int message_type,
                                                       int num_pkt_bytes,
                                                       CLIENT_INTERFACE(ethernet_tx_if, i_eth)) {
    avb_1722_1_aecp_address_access_t *aa_cmd = &(pkt->data.address);
    int tlv_count = ntoh_16(aa_cmd->tlv_count);
    unsigned int address = ntoh_32(&aa_cmd->address[4]);
    uint16_t status = AECP_AA_STATUS_SUCCESS;
    int mode = ADDRESS_MSG_GET_MODE(aa_cmd);
    int length = ADDRESS_MSG_GET_LENGTH(aa_cmd);
    int cd_len = 0;

    if (compare_guid(pkt->target_guid, &my_guid) == 0)
        return;

    if (tlv_count != 1 || mode != AECP_AA_MODE_WRITE) {
        status = AECP_AA_STATUS_TLV_INVALID;
    } else if (aecp_aa_next_write_address != address) {
        // We currently only process address writes in order and do not allow an
        // address to be written to twice
        status = AECP_AA_STATUS_ADDRESS_INVALID;
    } else {
        int bytes_available = length + aecp_aa_bytes_copied;
        unsigned int packet_index = 0;

        do {
            if (aecp_aa_bytes_copied == 0) {
                if (packet_index == 0) {
                    avb_write_upgrade_image_page(aecp_aa_flash_write_addr, aa_cmd->data, &status);
                    packet_index += FLASH_PAGE_SIZE;
                    aecp_aa_flash_write_addr += FLASH_PAGE_SIZE;
                    bytes_available -= FLASH_PAGE_SIZE;
                } else if (packet_index + FLASH_PAGE_SIZE > length) {
                    memcpy(aecp_flash_page_buf, &aa_cmd->data[packet_index], bytes_available);
                    aecp_aa_bytes_copied += bytes_available;
                    bytes_available = 0;
                    packet_index = 0;
                } else if (packet_index + FLASH_PAGE_SIZE <= length) {
                    avb_write_upgrade_image_page(aecp_aa_flash_write_addr,
                                                 (uint8_t *)&aa_cmd->data[packet_index], &status);
                    packet_index += FLASH_PAGE_SIZE;
                    aecp_aa_flash_write_addr += FLASH_PAGE_SIZE;
                    bytes_available -= FLASH_PAGE_SIZE;
                }
            } else if (aecp_aa_bytes_copied == FLASH_PAGE_SIZE) {
                avb_write_upgrade_image_page(aecp_aa_flash_write_addr,
                                             (uint8_t *)aecp_flash_page_buf, &status);
                aecp_aa_flash_write_addr += FLASH_PAGE_SIZE;
                bytes_available -= FLASH_PAGE_SIZE;
                aecp_aa_bytes_copied = 0;
            } else {
                int bytes_to_copy = FLASH_PAGE_SIZE - aecp_aa_bytes_copied;
                memcpy(&aecp_flash_page_buf[aecp_aa_bytes_copied], &aa_cmd->data[packet_index],
                       bytes_to_copy);
                aecp_aa_bytes_copied += bytes_to_copy;
                packet_index += bytes_to_copy;
            }
        } while (bytes_available > 0);

        aecp_aa_next_write_address += length;
    }

    cd_len = GET_1722_1_DATALENGTH(&pkt->header);

    avb_1722_1_aecp_address_access_t *aa_pkt =
        (avb_1722_1_aecp_address_access_t *)avb_1722_1_create_aecp_response_header(
            src_addr, status, AECP_CMD_ADDRESS_ACCESS_COMMAND, cd_len, pkt);
    memcpy(aa_pkt, pkt->data.payload, sizeof(avb_1722_1_aecp_address_access_t));

    unsigned num_tx_bytes = num_pkt_bytes + sizeof(ethernet_hdr_t);

    if (num_tx_bytes < 64)
        num_tx_bytes = 64;
    eth_send_packet(i_eth, (char *)avb_1722_1_buf, num_tx_bytes, ETHERNET_ALL_INTERFACES);
}

void process_avb_1722_1_aecp_packet(uint8_t src_addr[MACADDR_NUM_BYTES],
                                    avb_1722_1_aecp_packet_t *pkt,
                                    int num_pkt_bytes,
                                    CLIENT_INTERFACE(ethernet_tx_if, i_eth),
                                    CLIENT_INTERFACE(avb_interface, i_avb),
                                    CLIENT_INTERFACE(avb_1722_1_control_callbacks, i_1722_1_entity),
                                    chanend c_ptp) {
    int message_type = GET_1722_1_MSG_TYPE(((avb_1722_1_packet_header_t *)pkt));

    switch (message_type) {
    case AECP_CMD_AEM_COMMAND:
    case AECP_CMD_AEM_RESPONSE: {
        if (AVB_1722_1_AEM_ENABLED) {
            process_avb_1722_1_aecp_aem_msg(pkt, src_addr, message_type, num_pkt_bytes, i_eth,
                                            i_avb, i_1722_1_entity, c_ptp);
        }
        break;
    }
    case AECP_CMD_ADDRESS_ACCESS_COMMAND: {
        if (AVB_1722_1_FIRMWARE_UPGRADE_ENABLED) {
            process_avb_1722_1_aecp_address_access_cmd(pkt, src_addr, message_type, num_pkt_bytes,
                                                       i_eth);
        }
        break;
    }
    case AECP_CMD_AVC_COMMAND: {
        break;
    }
    case AECP_CMD_VENDOR_UNIQUE_COMMAND: {
        break;
    }
    case AECP_CMD_EXTENDED_COMMAND: {
        break;
    }
    default:
        // This node is not expecting a response
        break;
    }
}

void avb_1722_1_aecp_aem_periodic(CLIENT_INTERFACE(ethernet_tx_if, i_eth)) {
    char available_timeouts[5] = {12, 1, 11, 12, 2};
    if (avb_timer_expired(&aecp_aem_controller_available_timer)) {
        int cd_len = 0;

        // Timeline

        // TX Controller Available
        //   IN_A state for 120ms
        // TX IN_PROGRESS response
        //   IN_B state for 120ms
        // TX IN_PROGRESS
        //   IN_C state for 10ms
        // TX Controller Available
        //   IN_D state for 110ms
        // TX IN_PROGRESS response
        //   IN_E state for 120ms
        // TX IN_PROGRESS response
        //   IN_F state for 20ms
        // Timed out so it's an acquire

        switch (aecp_aem_controller_available_state) {
        case AECP_AEM_CONTROLLER_AVAILABLE_IDLE:
            // Nothing to do
            break;
        case AECP_AEM_CONTROLLER_AVAILABLE_IN_A:
        case AECP_AEM_CONTROLLER_AVAILABLE_IN_B:
        case AECP_AEM_CONTROLLER_AVAILABLE_IN_C:
        case AECP_AEM_CONTROLLER_AVAILABLE_IN_D:
        case AECP_AEM_CONTROLLER_AVAILABLE_IN_E:
            cd_len = avb_1722_1_create_acquire_response_packet(AECP_AEM_STATUS_IN_PROGRESS);
            start_avb_timer(&aecp_aem_controller_available_timer,
                            available_timeouts[aecp_aem_controller_available_state]);
            aecp_aem_controller_available_state++;
            break;
        case AECP_AEM_CONTROLLER_AVAILABLE_IN_F:
            if (pending_persistent) {
                entity_acquired_status = AEM_ENTITY_ACQUIRED_AND_PERSISTENT;
            } else {
                entity_acquired_status = AEM_ENTITY_ACQUIRED;
            }
            acquired_controller_guid = pending_controller_guid;
            debug_printf("1722.1 Controller %x%x acquired entity after timeout\n",
                         acquired_controller_guid.l << 32, acquired_controller_guid.l);

            memcpy(&acquired_controller_mac, &pending_controller_mac, 6);

            // TODO: Construct and send response to pending controller
            cd_len = avb_1722_1_create_acquire_response_packet(AECP_AEM_STATUS_SUCCESS);

            aecp_aem_controller_available_state = AECP_AEM_CONTROLLER_AVAILABLE_IDLE;
            break;
        }

        if (cd_len) {
            int num_tx_bytes = cd_len;

            if (num_tx_bytes < 64) {
                num_tx_bytes = 64;
            }

            eth_send_packet(i_eth, (char *)avb_1722_1_buf, num_tx_bytes, ETHERNET_ALL_INTERFACES);
        }
    }
}

static void process_aem_cmd_register_unsolicited_notification(avb_1722_1_aecp_packet_t *pkt,
                                                              uint8_t src_addr[MACADDR_NUM_BYTES],
                                                              uint8_t *status) {
    assert(unsolicited_notification_controllers_count < AVB_1722_1_MAX_NOTIFICATION_CONTROLLERS);

    // TODO: support flags for timed registrations
    for (size_t i = 0; i < unsolicited_notification_controllers_count; i++) {
        if (compare_guid(pkt->controller_guid, &unsolicited_notification_controllers[i])) {
            debug_printf("process_aem_cmd_register_unsolicited_notification: "
                         "controller %08x%08x already registered\n",
                         unsolicited_notification_controllers[i].l << 32,
                         unsolicited_notification_controllers[i].l);
            *status = AECP_AEM_STATUS_SUCCESS;
            return;
        }
    }

    if (unsolicited_notification_controllers_count == AVB_1722_1_MAX_NOTIFICATION_CONTROLLERS) {
        debug_printf("process_aem_cmd_register_unsolicited_notification: "
                     "notification controller limit reached\n");
        *status = AECP_AEM_STATUS_NO_RESOURCES;
        return;
    }

    ntoh_guid(&unsolicited_notification_controllers[unsolicited_notification_controllers_count],
              pkt->controller_guid);
    memcpy(unsolicited_notification_controller_macs[unsolicited_notification_controllers_count],
           src_addr, MACADDR_NUM_BYTES);

    debug_printf("process_aem_cmd_register_unsolicited_notification: "
                 "registered controller %08x%08x in slot %d\n",
                 unsolicited_notification_controllers[unsolicited_notification_controllers_count].l
                     << 32,
                 unsolicited_notification_controllers[unsolicited_notification_controllers_count].l,
                 unsolicited_notification_controllers_count);

    unsolicited_notification_controllers_count++;
}

static void process_aem_cmd_deregister_unsolicited_notification(avb_1722_1_aecp_packet_t *pkt,
                                                                uint8_t src_addr[MACADDR_NUM_BYTES],
                                                                uint8_t *status) {
    size_t i;

    *status = AECP_AEM_STATUS_BAD_ARGUMENTS;

    assert(unsolicited_notification_controllers_count < AVB_1722_1_MAX_NOTIFICATION_CONTROLLERS);

    // TODO: do we need to check MAC address matches?
    for (i = 0; i < unsolicited_notification_controllers_count; i++) {
        if (compare_guid(pkt->controller_guid, &unsolicited_notification_controllers[i]) &&
            memcmp(src_addr, unsolicited_notification_controller_macs[i], MACADDR_NUM_BYTES) == 0) {
            *status = AECP_AEM_STATUS_SUCCESS;
            break;
        }
    }

    if (*status != AECP_AEM_STATUS_SUCCESS) {
        guid_t guid;
        ntoh_guid(&guid, pkt->controller_guid);
        debug_printf("process_aem_cmd_deregister_unsolicited_notification: "
                     "controller %08x%08x not registered\n",
                     guid.l << 32, guid.l);
        return;
    }

    debug_printf("process_aem_cmd_deregister_unsolicited_notification: "
                 "deregistered controller %08x%08x from slot %d\n",
                 unsolicited_notification_controllers[i].l << 32,
                 unsolicited_notification_controllers[i].l, i);

    memmove(&unsolicited_notification_controllers[i], &unsolicited_notification_controllers[i + 1],
            sizeof(guid_t) * (unsolicited_notification_controllers_count - i - 1));
    memmove(&unsolicited_notification_controller_macs[i],
            unsolicited_notification_controller_macs[i + 1],
            MACADDR_NUM_BYTES * (unsolicited_notification_controllers_count - i - 1));

    unsolicited_notification_controllers_count--;
}

static void send_unsolicited_notifications(CLIENT_INTERFACE(ethernet_tx_if, i_eth),
                                           size_t num_tx_bytes) {
    struct ethernet_hdr_t *hdr = (ethernet_hdr_t *)&avb_1722_1_buf[0];
    avb_1722_1_aecp_packet_t *pkt =
        (avb_1722_1_aecp_packet_t *)(hdr + AVB_1722_1_PACKET_BODY_POINTER_OFFSET);
    avb_1722_1_aecp_aem_msg_t *aem_msg = &pkt->data.aem;
    guid_t requesting_controller_guid;

    ntoh_guid(&requesting_controller_guid, pkt->controller_guid);

    AEM_MSG_SET_U_FLAG(aem_msg, 1);
    hton_16(pkt->sequence_id, ++unsolicited_sequence_id);

    // if a controller has acquired the entity, and it's not the requesting
    // controller, then notify it
    if (entity_acquired_status == AECP_AEM_STATUS_ENTITY_ACQUIRED &&
        memcmp(&acquired_controller_guid, &requesting_controller_guid,
               sizeof(requesting_controller_guid)) != 0) {
        hton_guid(pkt->controller_guid, &acquired_controller_guid);
        memcpy(hdr->dest_addr, acquired_controller_mac, MACADDR_NUM_BYTES);
        eth_send_packet(i_eth, (char *)avb_1722_1_buf, num_tx_bytes, ETHERNET_ALL_INTERFACES);
        debug_printf("send_unsolicited_notifications: notified 1722.1 "
                     "acquiring controller %08x%08x for command %u\n",
                     acquired_controller_guid.l << 32, acquired_controller_guid.l,
                     AEM_MSG_GET_COMMAND_TYPE(aem_msg));
    }

    for (size_t i = 0; i < unsolicited_notification_controllers_count; i++) {
        if (memcmp(&unsolicited_notification_controllers[i], &requesting_controller_guid,
                   sizeof(requesting_controller_guid)) == 0 ||
            (entity_acquired_status == AECP_AEM_STATUS_ENTITY_ACQUIRED &&
             memcmp(&unsolicited_notification_controllers[i], &acquired_controller_guid,
                    sizeof(acquired_controller_guid)) == 0))
            continue; // don't send notification in addition to a response, or
                      // if we sent to the acquiring controller previously

        hton_guid(pkt->controller_guid, &unsolicited_notification_controllers[i]);
        memcpy(hdr->dest_addr, unsolicited_notification_controller_macs[i], MACADDR_NUM_BYTES);
        eth_send_packet(i_eth, (char *)avb_1722_1_buf, num_tx_bytes, ETHERNET_ALL_INTERFACES);
        debug_printf("send_unsolicited_notifications[%d]: notified 1722.1 "
                     "controller %08x%08x for command %u\n",
                     i, unsolicited_notification_controllers[i].l << 32,
                     unsolicited_notification_controllers[i].l, AEM_MSG_GET_COMMAND_TYPE(aem_msg));
    }
}

void send_unsolicited_notifications_state_changed(uint16_t command_type,
                                                  uint16_t stream_desc_type,
                                                  uint16_t stream_desc_id,
                                                  CLIENT_INTERFACE(ethernet_tx_if, i_eth),
                                                  CLIENT_INTERFACE(avb_interface, i_avb_api),
                                                  CLIENT_INTERFACE(avb_1722_1_control_callbacks, i_1722_1_entity),
                                                  chanend c_ptp) {
    avb_1722_1_aecp_packet_t pkt;
    size_t cd_len;
    uint8_t status = AECP_AEM_STATUS_NOT_IMPLEMENTED;

    memset(&pkt, 0, sizeof(pkt));
    memcpy(pkt.target_guid, &my_guid, sizeof(my_guid));
    memset(pkt.controller_guid, 0xff, sizeof(pkt.controller_guid));

    switch (command_type) {
    case AECP_AEM_CMD_GET_STREAM_INFO: {
        avb_1722_1_aem_getset_stream_info_t *cmd =
            (avb_1722_1_aem_getset_stream_info_t *)(pkt.data.aem.command.payload);
        hton_16(cmd->descriptor_id, stream_desc_id);
        hton_16(cmd->descriptor_type, stream_desc_type);
        process_aem_cmd_getset_stream_info(&pkt, &status, AECP_AEM_CMD_GET_STREAM_INFO, i_avb_api);
        cd_len = sizeof(avb_1722_1_aem_getset_stream_info_t);
        break;
    }
    case AECP_AEM_CMD_GET_AVB_INFO:
        process_aem_cmd_get_avb_info(&pkt, &status, i_avb_api, c_ptp);
        cd_len = sizeof(avb_1722_1_aem_get_avb_info_response_t);
        break;
    case AECP_AEM_CMD_GET_AS_PATH:
        break;
    case AECP_AEM_CMD_GET_COUNTERS:
        break;
    case AECP_AEM_CMD_LOCK_ENTITY:
        break;
    default:
        break;
    }

    if (status != AECP_AEM_STATUS_SUCCESS)
        return;

    size_t num_tx_bytes = cd_len + 2 + // U Flag + command type
                       AVB_1722_1_AECP_PAYLOAD_OFFSET + sizeof(ethernet_hdr_t);

    if (num_tx_bytes < 64)
        num_tx_bytes = 64;

    send_unsolicited_notifications(i_eth, num_tx_bytes);
}
