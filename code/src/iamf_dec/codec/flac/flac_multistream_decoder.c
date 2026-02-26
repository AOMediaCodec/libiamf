/*
 * Copyright (c) 2024, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */

/**
 * @file flac_multistream_decoder.c
 * @brief FLAC decoder.
 * @version 0.1
 * @date Created 03/03/2023
 **/

#ifdef CONFIG_FLAC_CODEC
#include "flac_multistream_decoder.h"

#include <stdlib.h>
#include <string.h>

#include "FLAC/stream_decoder.h"
#include "IAMF_defines.h"
#include "clog.h"
#include "iamf_private_definitions.h"
#include "iamf_types.h"
#include "iamf_utils.h"
#include "iorw.h"

#ifdef def_log_tag
#undef def_log_tag
#endif

#define def_log_tag "FLACMS"

typedef struct FlacDecoderHandle {
  FLAC__StreamDecoder *dec;
  struct {
    uint32_t depth;
    uint32_t channels;
    uint32_t sample_rate;
  } stream_info;
  uint8_t *packet;
  uint32_t packet_size;
  uint32_t fs;
  int buffer[def_max_flac_frame_size];
} flac_decoder_handle_t;

typedef struct FlacMsDecoder {
  uint32_t flags;
  int streams;
  int coupled_streams;

  flac_decoder_handle_t *handles;
} flac_ms_decoder_t;

static FLAC__StreamDecoderReadStatus flac_stream_read(
    const FLAC__StreamDecoder *decoder, FLAC__byte buffer[], size_t *bytes,
    void *client_data) {
  flac_decoder_handle_t *handle = (flac_decoder_handle_t *)client_data;
  if (handle->packet) {
    trace("read stream %d data", handle->packet_size);
    memcpy(buffer, handle->packet, handle->packet_size);
    *bytes = handle->packet_size;
    handle->packet = 0;
    return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
  } else {
    error("The data is incomplete.");
  }

  return FLAC__STREAM_DECODER_READ_STATUS_ABORT;
}

static FLAC__StreamDecoderWriteStatus flac_stream_write(
    const FLAC__StreamDecoder *decoder, const FLAC__Frame *frame,
    const FLAC__int32 *const buffer[], void *client_data) {
  flac_decoder_handle_t *handle = (flac_decoder_handle_t *)client_data;
  char *out = (char *)handle->buffer;
  int fss = 0;

  handle->fs = frame->header.blocksize;
  fss = handle->fs * sizeof(FLAC__int32);

  for (int i = 0; i < handle->stream_info.channels; ++i) {
    memcpy(out, (void *)buffer[i], fss);
    out += fss;
  }

  return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

static void flac_stream_metadata(const FLAC__StreamDecoder *decoder,
                                 const FLAC__StreamMetadata *metadata,
                                 void *client_data) {
  if (metadata->type == FLAC__METADATA_TYPE_STREAMINFO) {
    flac_decoder_handle_t *handle = (flac_decoder_handle_t *)client_data;
    handle->stream_info.depth = metadata->data.stream_info.bits_per_sample;
    handle->stream_info.channels = metadata->data.stream_info.channels;
    handle->stream_info.sample_rate = metadata->data.stream_info.sample_rate;
    info("depth %d, channels %d, sample_rate %d.", handle->stream_info.depth,
         handle->stream_info.channels, handle->stream_info.sample_rate);
  }
}

static void flac_stream_error(const FLAC__StreamDecoder *decoder,
                              FLAC__StreamDecoderErrorStatus status,
                              void *client_data) {}

static int flac_header_set_channels(uint8_t *h, uint32_t size, int n) {
  uint32_t last, type, s;
  io_context_t ioc, *r, *w;
  uint32_t byte, val;

  r = w = &ioc;

  ioc_init(r, h, size);
  ior_skip(r, 4);
  while (1) {
    val = ior_8(r);
    last = val >> 7 & 0x01;
    type = val & 0x7f;
    s = ior_b24(r);

    if (!type) {
      ior_skip(r, 12);
      val = ior_8(r);
      byte = ~(0x7 << 1) & val;
      byte = ((n - 1) & 0x7) << 1 | byte;
      w->idx--;
      debug("%d byte 1 bit: 0x%x->0x%x", ioc_tell(w), val, byte);
      iow_8(w, byte);
    } else
      ior_skip(r, s);

    if (last || !ioc_remain(r)) break;
  }

  if (!ioc_remain(r)) return IAMF_ERR_BAD_ARG;
  return IAMF_OK;
}

static int flac_multistream_decode_native(flac_ms_decoder_t *st,
                                          uint8_t *buffer[], uint32_t size[],
                                          void *pcm, int frame_size) {
  flac_decoder_handle_t *handle = NULL;
  char *out = (char *)pcm;
  int ss = 0;

  for (int i = 0; i < st->streams; ++i) {
    trace("stream %d", i);
    handle = &st->handles[i];
    handle->packet = buffer[i];
    handle->packet_size = size[i];

    if (!FLAC__stream_decoder_process_single(handle->dec)) {
      return IAMF_ERR_INTERNAL;
    }

    if (handle->fs != frame_size)
      warning("Different frame size %d vs %d", frame_size, handle->fs);

    ss = handle->fs * handle->stream_info.channels * 4;
    memcpy(out, handle->buffer, ss);
    out += ss;
  }
  if (!handle) {
    return IAMF_ERR_BAD_ARG;
  } else {
    return handle->fs;
  }
}

flac_ms_decoder_t *flac_multistream_decoder_open(uint8_t *config, uint32_t size,
                                                 int streams,
                                                 int coupled_streams,
                                                 uint32_t flags, int *error) {
  flac_ms_decoder_t *st = 0;
  flac_decoder_handle_t *handles = 0;
  flac_decoder_handle_t *handle;
  FLAC__StreamDecoderInitStatus status;
  uint8_t *header[2];
  int i, ret = 0;
  char magic[] = {'f', 'L', 'a', 'C'};

  if (!config || !size) {
    if (error) *error = IAMF_ERR_BAD_ARG;
    return 0;
  }

  st = def_mallocz(flac_ms_decoder_t, 1);
  handles = def_mallocz(flac_decoder_handle_t, streams);
  for (int i = 0; i < 2; ++i) header[i] = def_mallocz(uint8_t, size + 4);

  if (!st || !handles || !header[0] || !header[1]) {
    if (error) {
      *error = IAMF_ERR_ALLOC_FAIL;
    }
    if (st) {
      flac_multistream_decoder_close(st);
    }
    def_free(handles);
    for (int i = 0; i < 2; ++i) def_free(header[i]);

    return 0;
  }

  for (int i = 0; i < 2; ++i) {
    memcpy(header[i], magic, 4);
    memcpy(header[i] + 4, config, size);
  }

  flac_header_set_channels(header[1], size + 4, 1);
  st->flags = flags;
  st->streams = streams;
  st->coupled_streams = coupled_streams;
  st->handles = handles;

  for (i = 0; i < st->streams; ++i) {
    handle = &st->handles[i];
    if (i < coupled_streams)
      handle->packet = header[0];
    else
      handle->packet = header[1];
    handle->packet_size = size + 4;

    handle->dec = FLAC__stream_decoder_new();
    if (!handle->dec) {
      ret = IAMF_ERR_INVALID_STATE;
      break;
    }

    FLAC__stream_decoder_set_md5_checking(handle->dec, false);

    status = FLAC__stream_decoder_init_stream(
        handle->dec, flac_stream_read, NULL, NULL, NULL, NULL,
        flac_stream_write, flac_stream_metadata, flac_stream_error, handle);

    if (status != FLAC__STREAM_DECODER_INIT_STATUS_OK) {
      error("flac decoder init errno %d (%s)", -status,
            FLAC__StreamDecoderInitStatusString[status]);
      ret = IAMF_ERR_INTERNAL;
      break;
    }

    if (!FLAC__stream_decoder_process_until_end_of_metadata(handle->dec)) {
      error("Failed to read meta.");
      ret = IAMF_ERR_INTERNAL;
      break;
    }
  }

  if (ret < 0) {
    if (error) {
      *error = ret;
    }
    flac_multistream_decoder_close(st);
    st = 0;
  }

  for (int i = 0; i < 2; ++i) def_free(header[i]);

  return st;
}

int flac_multistream_decode(flac_ms_decoder_t *st, uint8_t *buffer[],
                            uint32_t size[], void *pcm, uint32_t frame_size) {
  if (st->flags & ck_audio_frame_planar)
    return flac_multistream_decode_native(st, buffer, size, pcm, frame_size);
  else {
    return IAMF_ERR_UNIMPLEMENTED;
  }
}

void flac_multistream_decoder_close(flac_ms_decoder_t *st) {
  if (st) {
    if (st->handles) {
      for (int i = 0; i < st->streams; ++i) {
        if (st->handles[i].dec) {
          FLAC__stream_decoder_flush(st->handles[i].dec);
          FLAC__stream_decoder_delete(st->handles[i].dec);
        }
      }
      free(st->handles);
    }
    free(st);
  }
}

int flac_multistream_decoder_get_sample_bits(flac_ms_decoder_t *st) {
  if (st && st->handles) return st->handles[0].stream_info.depth;
  return IAMF_ERR_INTERNAL;
}
#endif