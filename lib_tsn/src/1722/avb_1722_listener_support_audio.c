// Copyright (c) 2011-2017, XMOS Ltd, All rights reserved
// Portions (c) 2024 PADL Software Pty Ltd, All rights reserved
#include "avb_1722_listener.h"
#include "avb_1722_common.h"
#include "avb_1722_1_protocol.h"
#include "gptp.h"
#include "avb_1722_def.h"
#include "audio_output_fifo.h"
#include <string.h>
#include <xs1.h>
#include <xscope.h>
#include "default_avb_conf.h"
#include "debug_print.h"

#if defined(AVB_1722_FORMAT_61883_6) || defined(AVB_1722_FORMAT_AAF)

#ifdef AVB_1722_FORMAT_61883_6
static int avb_1722_listener_process_packet_61883(
    chanend buf_ctl,
    AVB_DataHeader_t *pAVBHdr,
    size_t pktDataLength,
    avb_1722_stream_info_t *stream_info,
    ptp_time_info_mod64 *timeInfo,
    int index,
    int *notified_buf_ctl,
    buffer_handle_t h)
{
    static const size_t sample_length_32 = 4;
    static const uint32_t valid_mask = 0xffffff00;
    AVB_AVB1722_CIP_Header_t *pAVBCIPHeader;
    uint8_t *sample_ptr;
    size_t num_channels = stream_info->num_channels;
    audio_output_fifo_t *map = &stream_info->map[0];
    int dbc_value, dbc_diff;
    size_t num_samples_in_payload, num_channels_in_payload;

    if (pktDataLength < AVB_CIP_HDR_SIZE)
        return 0;

    pAVBCIPHeader = (AVB_AVB1722_CIP_Header_t *)((uint8_t *)pAVBHdr + AVB_TP_HDR_SIZE);

    dbc_value = (int) pAVBCIPHeader->DBC;
    dbc_diff = dbc_value - stream_info->dbc;
    stream_info->dbc = dbc_value;

    if (dbc_diff < 0)
        dbc_diff += 0x100;

    num_samples_in_payload = (pktDataLength - AVB_CIP_HDR_SIZE) / sample_length_32;

    size_t prev_num_samples = stream_info->prev_num_samples;
    stream_info->prev_num_samples = num_samples_in_payload;

    if (stream_info->chan_lock < 16) {
        size_t num_channels;

        if (!prev_num_samples || dbc_diff == 0) {
            return 0;
        }

        num_channels = prev_num_samples / dbc_diff;

        if (!stream_info->num_channels_in_payload ||
            stream_info->num_channels_in_payload != num_channels) {
            stream_info->num_channels_in_payload = num_channels;
            stream_info->chan_lock = 0;
            stream_info->rate = 0;
        }

        stream_info->rate += num_samples_in_payload;
        stream_info->chan_lock++;

        if (stream_info->chan_lock == 16) {
            stream_info->rate = (stream_info->rate / stream_info->num_channels_in_payload / 16);

            switch (stream_info->rate) {
            case 1: stream_info->rate = 8000; break;
            case 2: stream_info->rate = 16000; break;
            case 4: stream_info->rate = 32000; break;
            case 5: stream_info->rate = 44100; break;
            case 6: stream_info->rate = 48000; break;
            case 11: stream_info->rate = 88200; break;
            case 12: stream_info->rate = 96000; break;
            case 24: stream_info->rate = 192000; break;
            default: stream_info->rate = 0; break;
            }
        }

        return 0;
    }

    if (AVBTP_TV(pAVBHdr)) {
        // See 61883-6 section 6.2 which explains that the receiver can calculate
        // which data block (sample) the timestamp refers to using the formula:
        //   index = (SYT_INTERVAL - dbc % SYT_INTERVAL) % SYT_INTERVAL
        unsigned syt_interval = 0, sample_num = 0;

        switch (stream_info->rate) {
            case 8000:   syt_interval = 1; break;
            case 16000:  syt_interval = 2; break;
            case 32000:  syt_interval = 8; break;
            case 44100:  syt_interval = 8; break;
            case 48000:  syt_interval = 8; break;
            case 88200:  syt_interval = 16; break;
            case 96000:  syt_interval = 16; break;
            case 176400: syt_interval = 32; break;
            case 192000: syt_interval = 32; break;
            default: return 0; break;
        }
        sample_num = (syt_interval - (dbc_value & (syt_interval-1))) & (syt_interval-1);
        for (size_t i = 0; i < num_channels; i++) {
            if (map[i] >= 0)
                audio_output_fifo_set_ptp_timestamp(h, map[i], AVBTP_TIMESTAMP(pAVBHdr), sample_num);
        }
    }

    for (size_t i = 0; i < num_channels; i++) {
        if (map[i] >= 0)
            audio_output_fifo_maintain(h, map[i], buf_ctl, notified_buf_ctl);
    }

    // now send the samples
    sample_ptr = (uint8_t *)pAVBCIPHeader + AVB_CIP_HDR_SIZE;
    num_channels_in_payload = stream_info->num_channels_in_payload;

    num_channels = num_channels < num_channels_in_payload ? num_channels : num_channels_in_payload;

    for (size_t i = 0; i < num_channels; i++) {
        if (map[i] >= 0)
            audio_output_fifo_strided_push(h, map[i], sample_ptr, sample_length_32,
                                           num_channels_in_payload, num_samples_in_payload,
                                           IEC_61883_IIDC_SUBTYPE, valid_mask);
        sample_ptr += sample_length_32;
    }

//    assert(sample_ptr == (uint8_t *)pAVBHdr + AVB_TP_HDR_SIZE + AVB_CIP_HDR_SIZE + num_channels * sample_length_32);

    return 1;
}
#endif /* AVB_1722_FORMAT_61883_6 */

#ifdef AVB_1722_FORMAT_AAF
static int avb_1722_listener_process_packet_aaf(
    chanend buf_ctl,
    AVB_DataHeader_t *pAVBHdr,
    size_t pktDataLength,
    avb_1722_stream_info_t *stream_info,
    ptp_time_info_mod64 *timeInfo,
    int index,
    int *notified_buf_ctl,
    buffer_handle_t h)
{
    uint8_t *sample_ptr;
    audio_output_fifo_t *map = &stream_info->map[0];
    size_t sample_length;

    sample_length = (size_t)bit_depth_from_format(pAVBHdr->gateway_info[0]) / 8;
    if (sample_length == 0) {
#if AVB_1722_RECORD_ERRORS
        debug_printf("avb_1722_listener_process_packet_aaf: unsupported format %02x\n", pAVBHdr->gateway_info[0]);
#endif
        return 0;
    }

    uint8_t nsr = (pAVBHdr->gateway_info[1] & 0xF0) >> 4;
    stream_info->rate = sampling_rate_from_nsr(nsr);
    if (stream_info->rate == 0) {
#if AVB_1722_RECORD_ERRORS
        debug_printf("avb_1722_listener_process_packet_aaf: unsupported NSR value %02x\n", nsr);
#endif
        return 0;
    }

    // FIXME: do we want this to be able to change with each packet? or just the first?
    uint16_t cpf = (pAVBHdr->gateway_info[1] & 0x3) << 8 | (pAVBHdr->gateway_info[2]);
    stream_info->num_channels_in_payload = cpf;

    size_t num_samples_in_payload = pktDataLength / sample_length;

    if (AVBTP_TV(pAVBHdr)) {
        if (AVBTP_PROTOCOL_SPECIFIC(pAVBHdr) & AAF_PROTOCOL_SPECIFIC_SPARSE) {
            // TODO: handle sparse timestamps, MILAN doesn't require it so we ignore for now
#if AVB_1722_RECORD_ERRORS
        debug_printf("avb_1722_listener_process_packet_aaf: sparse timestamps not supported\n");
#endif
            return 0;
        }
        for (size_t i = 0; i < stream_info->num_channels; i++) {
            if (map[i] >= 0)
                audio_output_fifo_set_ptp_timestamp(h, map[i], AVBTP_TIMESTAMP(pAVBHdr), 0);
        }
    }

    for (size_t i = 0; i < stream_info->num_channels; i++) {
        if (map[i] >= 0)
            audio_output_fifo_maintain(h, map[i], buf_ctl, notified_buf_ctl);
    }

    sample_ptr = (uint8_t *)pAVBHdr + AVB_TP_HDR_SIZE;

    uint8_t bit_depth = pAVBHdr->gateway_info[3];
    uint32_t valid_mask = (bit_depth >= 32) ? 0xffffffff : (1 << bit_depth) - 1;

    for (size_t i = 0; i < stream_info->num_channels; i++) {
        if (map[i] >= 0)
            audio_output_fifo_strided_push(h, map[i], sample_ptr, sample_length,
                                           cpf, num_samples_in_payload,
                                           AVTP_AUDIO_SUBTYPE, valid_mask);
        sample_ptr += sample_length;
    }

    return 1;
}
#endif /* AVB_1722_FORMAT_AAF */

int avb_1722_listener_process_packet(
    chanend buf_ctl,
    uint8_t Buf[],
    int numBytes,
    avb_1722_stream_info_t *stream_info,
    ptp_time_info_mod64* timeInfo,
    int index,
    int *notified_buf_ctl,
    buffer_handle_t h)
{
    AVB_DataHeader_t *pAVBHdr;
    size_t avb_ethernet_hdr_size = (Buf[12] == 0x81) ? 18 : 14;

    // sanity check on number bytes in payload
    if (numBytes < avb_ethernet_hdr_size + AVB_TP_HDR_SIZE) {
#if AVB_1722_RECORD_ERRORS
        debug_printf("avb_1722_listener_process_packet: got %u bytes, expected at least %u\n",
                     numBytes, avb_ethernet_hdr_size + AVB_TP_HDR_SIZE);
#endif
        return 0;
    }

    pAVBHdr = (AVB_DataHeader_t *) &Buf[avb_ethernet_hdr_size];

    if (AVBTP_VERSION(pAVBHdr) != 0) {
#if AVB_1722_RECORD_ERRORS
        debug_printf("avb_1722_listener_process_packet: unknown version %u\n", AVBTP_VERSION(pAVBHdr));
#endif
        return 0;
    }

    if (AVBTP_CD(pAVBHdr) != AVBTP_CD_DATA) {
#if AVB_1722_RECORD_ERRORS
        debug_printf("avb_1722_listener_process_packet: received control instead of data packet\n");
#endif
        return 0;
    }

    if (AVBTP_SV(pAVBHdr) == 0) {
#if AVB_1722_RECORD_ERRORS
        debug_printf("avb_1722_listener_process_packet: stream valid bit not set\n");
#endif
        return 0;
    }

    size_t pktDataLength = NTOH_U16(pAVBHdr->packet_data_length);
    if (numBytes < avb_ethernet_hdr_size + AVB_TP_HDR_SIZE + pktDataLength) {
#if AVB_1722_RECORD_ERRORS
        debug_printf("avb_1722_listener_process_packet: packet truncated, expected at least %u bytes, got %u\n",
                     avb_ethernet_hdr_size + AVB_TP_HDR_SIZE + pktDataLength, numBytes);
#endif
        return 0;
  }

#if AVB_1722_RECORD_ERRORS == 2 // record out of sequence packets
    uint8_t seq_num = AVBTP_SEQUENCE_NUMBER(pAVBHdr);
    if (stream_info->last_sequence && seq_num - stream_info->last_sequence != 1) {
        debug_printf("avb_1722_listener_process_packet: dropping packet %d last %d ts %x\n",
                     seq_num, stream_info->last_sequence, AVBTP_TIMESTAMP(pAVBHdr));
    }
    stream_info->last_sequence = seq_num;
#endif

    switch (pAVBHdr->subtype) {
#ifdef AVB_1722_FORMAT_61883_6
    case IEC_61883_IIDC_SUBTYPE:
        return avb_1722_listener_process_packet_61883(buf_ctl, pAVBHdr, pktDataLength,
                                                      stream_info, timeInfo, index, notified_buf_ctl, h);
#endif
#ifdef AVB_1722_FORMAT_AAF
    case AVTP_AUDIO_SUBTYPE:
        return avb_1722_listener_process_packet_aaf(buf_ctl, pAVBHdr, pktDataLength,
                                                    stream_info, timeInfo, index, notified_buf_ctl, h);
#endif
    default:
#if AVB_1722_RECORD_ERRORS
        debug_printf("avb_1722_listener_process_packet: unsupported subtype %02x\n", pAVBHdr->subtype);
#endif
        return 0;
    }
}

#endif
