// Copyright (c) 2011-2017, XMOS Ltd, All rights reserved
// Portions Copyright (c) 2024 PADL Software Pty Ltd, All rights reserved

#include <print.h>
#include <xccompat.h>
#include <xscope.h>
#include <xassert.h>
#include "audio_output_fifo.h"
#include "avb_1722_def.h"
#include "media_clock_client.h"
#include "avb_1722_1_protocol.h"

#define OUTPUT_DURING_LOCK 0
#define NOTIFICATION_PERIOD 250

#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

void
audio_output_fifo_init(buffer_handle_t s0, unsigned index)
{
    ofifo_t *s = (ofifo_t *)((struct output_finfo *)s0)->p_buffer[index];

    s->state = DISABLED;
    s->dptr = START_OF_FIFO(s);
    s->wrptr = START_OF_FIFO(s);
    s->media_clock = -1;
    s->pending_init_notification = 0;
    s->last_notification_time = 0;
}

void
disable_audio_output_fifo(buffer_handle_t s0, unsigned index)
{
    ofifo_t *s = (ofifo_t *)((struct output_finfo *)s0)->p_buffer[index];

    s->state = DISABLED;
    s->zero_flag = 1;
}

void
enable_audio_output_fifo(buffer_handle_t s0, unsigned index, int media_clock)
{
  ofifo_t *s = (ofifo_t *)((struct output_finfo *)s0)->p_buffer[index];

  s->state = ZEROING;
  s->dptr = START_OF_FIFO(s);
  s->wrptr = START_OF_FIFO(s);
  s->marker = (unsigned int *) 0;
  s->local_ts = 0;
  s->ptp_ts = 0;
  s->zero_marker = END_OF_FIFO(s)-1;
  s->zero_flag = 1;
  *s->zero_marker = 1;
  s->sample_count = 0;
  s->media_clock = media_clock;
  s->pending_init_notification = 1;
}


// 1722 thread
void audio_output_fifo_set_ptp_timestamp(buffer_handle_t s0,
                                         unsigned int index,
                                         unsigned int ptp_ts,
                                         unsigned sample_number)
{
  ofifo_t *s = (ofifo_t *)((struct output_finfo *)s0)->p_buffer[index];

  if (s->marker == 0) {
	unsigned int* new_marker = s->wrptr + sample_number;
	if (new_marker >= END_OF_FIFO(s)) new_marker -= AUDIO_OUTPUT_FIFO_WORD_SIZE;

	if (ptp_ts==0) ptp_ts = 1;
    s->ptp_ts = ptp_ts;
    s->local_ts = 0;
    s->marker = new_marker;
  }
}

// 1722 thread
void
audio_output_fifo_maintain(buffer_handle_t s0,
                           unsigned index,
                           chanend buf_ctl,
                           int *notified_buf_ctl)
{
  ofifo_t *s = (ofifo_t *)((struct output_finfo *)s0)->p_buffer[index];
  unsigned time_since_last_notification;

  if (s->pending_init_notification && !(*notified_buf_ctl)) {
    notify_buf_ctl_of_new_stream(buf_ctl, (int)s); // TODO: This can pass the index
    *notified_buf_ctl = 1;
    s->pending_init_notification = 0;
  }

  switch (s->state)
    {
    case DISABLED:
      break;
    case ZEROING:
      if (*s->zero_marker == 0) {
        // we have zero-ed the entire fifo
        // set the wrptr so that the fifo size is 1/2 of the buffer size
        int buf_len = (END_OF_FIFO(s) - START_OF_FIFO(s));
        unsigned int *new_wrptr;

        new_wrptr = s->dptr + ((buf_len>>1));
        while (new_wrptr >= END_OF_FIFO(s))
          new_wrptr -= buf_len;

        s->wrptr = new_wrptr;
        s->state = LOCKING;
        s->local_ts = 0;
        s->ptp_ts = 0;
        s->marker = 0;
#if (OUTPUT_DURING_LOCK == 0)
        s->zero_flag = 1;
#endif
      }
      break;
    case LOCKING:
    case LOCKED:
      time_since_last_notification =
        (signed) s->sample_count - (signed) s->last_notification_time;
      if (s->ptp_ts != 0 &&
          s->local_ts != 0 &&
          !(*notified_buf_ctl)
          &&
          (s->last_notification_time == 0 ||
           time_since_last_notification > NOTIFICATION_PERIOD)
          )
        {
          notify_buf_ctl_of_info(buf_ctl, (int)s); // TODO: FIXME
          *notified_buf_ctl = 1;
          s->last_notification_time = s->sample_count;
        }
      break;
    }
}

// 1722 thread
void
audio_output_fifo_strided_push(buffer_handle_t s0,
                               size_t index,
                               uint8_t *sample_ptr,
                               size_t sample_length, // size of sample in bytes
                               size_t stride, // skip * sample_length bytes to get to next sample
                               size_t num_samples, // how many samples in the entire payload
                               uint8_t subtype,
                               uint32_t valid_mask)
{
  ofifo_t *s = (ofifo_t *)((struct output_finfo *)s0)->p_buffer[index];
  unsigned int *wrptr = s->wrptr;
  unsigned int *new_wrptr;
  uint32_t sample;
  size_t sample_count = 0;

  if (s->state == DISABLED)
    return;

  for (size_t i = 0; i < num_samples; i += stride) {
    if (subtype == IEC_61883_IIDC_SUBTYPE) {
        sample = __builtin_bswap32(*((uint32_t *)sample_ptr));
        sample <<= 8; // IEC 61883 samples are right justified
    } else {
        switch (sample_length) {
        case 4:
            sample = __builtin_bswap32(*((uint32_t *)sample_ptr));
            break;
        case 3:
            sample = (uint32_t)sample_ptr[0] << 16;
            sample |= (uint32_t)sample_ptr[1] << 8;
            sample |= (uint32_t)sample_ptr[2] << 0;
            break;
        case 2:
            sample = (uint32_t)(__builtin_bswap16(*((uint16_t *)sample_ptr)) << 16);
            break;
        default:
            fail("unknown sample bit depth");
        }
    }

    sample_ptr += sample_length * stride;
    sample_count++;

    if (unlikely(s->state == ZEROING))
        sample = 0;

    new_wrptr = wrptr + 1;

    if (new_wrptr == END_OF_FIFO(s))
        new_wrptr = START_OF_FIFO(s);

    if (new_wrptr != s->dptr) {
        *wrptr = sample & valid_mask;
        wrptr = new_wrptr;
    } else {
        // debug_printf("audio_output_fifo_strided_push: overflow index %u\n", index);
    }
  }

  s->wrptr = wrptr;
  s->sample_count += sample_count;
}

// 1722 thread
void
audio_output_fifo_handle_buf_ctl(chanend buf_ctl,
                                 buffer_handle_t s0,
                                 unsigned index,
                                 int *buf_ctl_notified,
                                 timer tmr)
{
  int cmd;
  ofifo_t *s = (ofifo_t *)((struct output_finfo *)s0)->p_buffer[index];
  cmd = get_buf_ctl_cmd(buf_ctl);
  switch (cmd)
    {
    case BUF_CTL_REQUEST_INFO: {
      send_buf_ctl_info(buf_ctl,
                        s->state == LOCKED,
                        s->ptp_ts,
                        s->local_ts,
                        s->dptr - START_OF_FIFO(s),
                        s->wrptr - START_OF_FIFO(s),
                        tmr);
      s->ptp_ts = 0;
      s->local_ts = 0;
      s->marker = (unsigned int *) 0;
      break;
    }
    case BUF_CTL_REQUEST_NEW_STREAM_INFO: {
      send_buf_ctl_new_stream_info(buf_ctl,
                                   s->media_clock);
      buf_ctl_ack(buf_ctl);
      *buf_ctl_notified = 0;
      break;
    }
    case BUF_CTL_ADJUST_FILL:
      {
        int adjust;
        unsigned int *new_wrptr;
        adjust = get_buf_ctl_adjust(buf_ctl);

        new_wrptr = s->wrptr - adjust;
        while (new_wrptr < START_OF_FIFO(s))
          new_wrptr += (END_OF_FIFO(s) - START_OF_FIFO(s));

        while (new_wrptr >= END_OF_FIFO(s))
          new_wrptr -= (END_OF_FIFO(s) - START_OF_FIFO(s));

        s->wrptr = new_wrptr;
      }
      s->state = LOCKED;
      s->zero_flag = 0;
      s->ptp_ts = 0;
      s->local_ts = 0;
      s->marker = (unsigned int *) 0;
      buf_ctl_ack(buf_ctl);
      *buf_ctl_notified = 0;
      break;
    case BUF_CTL_RESET:
      s->state = ZEROING;
      if (s->wrptr == START_OF_FIFO(s))
        s->zero_marker = END_OF_FIFO(s) - 1;
      else
        s->zero_marker = s->wrptr - 1;
      s->zero_flag = 1;
      *s->zero_marker = 1;
      buf_ctl_ack(buf_ctl);
      *buf_ctl_notified = 0;
      break;
    case BUF_CTL_ACK:
      buf_ctl_ack(buf_ctl);
      *buf_ctl_notified = 0;
      break;
    default:
      break;
    }
}

ofifo_state_t
audio_output_fifo_get_state(buffer_handle_t s0, unsigned index)
{
    ofifo_t *s = (ofifo_t *)((struct output_finfo *)s0)->p_buffer[index];
    return s->state;
}
