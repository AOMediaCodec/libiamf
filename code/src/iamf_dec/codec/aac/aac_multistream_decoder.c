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
 * @file aac_multistream_decoder.c
 * @brief AAC decoder.
 * @version 2.0.0
 * @date Created 03/03/2023
 **/

#ifdef CONFIG_AAC_CODEC
#include "aac_multistream_decoder.h"

#include <stdlib.h>
#include <string.h>

#include "IAMF_defines.h"
#include "clog.h"
#include "fdk-aac/aacdecoder_lib.h"
#include "iamf_private_definitions.h"
#include "iamf_types.h"
#include "iamf_utils.h"

#ifdef def_log_tag
#undef def_log_tag
#endif

#define def_log_tag "AACMS"

#define def_max_aac_buffer_size def_max_aac_frame_size * 2

struct AacMsDecoder {
  uint32_t flags;
  int streams;
  int coupled_streams;
  int delay;

  HANDLE_AACDECODER *handles;
  INT_PCM buffer[def_max_aac_buffer_size];
};

typedef void (*func_aac_copy_channel_out_t)(void *dst, const void *src,
                                            int frame_size, int channes);

void aac_copy_channel_out_short_plane(void *dst, const void *src,
                                      int frame_size, int channels) {
  if (channels == 1) {
    memcpy(dst, src, sizeof(INT_PCM) * frame_size);
  } else if (channels == 2) {
    INT_PCM *in, *out;
    in = (INT_PCM *)src;
    out = (INT_PCM *)dst;
    for (int s = 0; s < frame_size; ++s) {
      out[s] = in[channels * s];
      out[s + frame_size] = in[channels * s + 1];
    }
  }
}

static int aac_config_set_channels(uint8_t *conf, uint32_t size, int channels) {
  int nbytes = 0;
  uint8_t byte = 0;

  // audioObjectType = 2 (5bits)
  // samplingFrequencyIndex (4 or 28 bits)
  // channelConfiguration (4 bits)
  byte = conf[nbytes] & 0x7;
  byte = byte << 1 | conf[++nbytes] >> 7;
  if (byte == 0xf) nbytes += 3;
  byte = ~(0xf << 3) & conf[nbytes];
  byte = (channels & 0xf) << 3 | byte;
  debug("%d byte 1 bit: 0x%x->0x%x", nbytes, conf[nbytes], byte);
  if (nbytes < size)
    conf[nbytes] = byte;
  else
    return IAMF_ERR_BAD_ARG;

  return IAMF_OK;
}

static int aac_multistream_decode_native(
    aac_ms_decoder_t *st, uint8_t *buffer[], uint32_t size[], void *pcm,
    int frame_size, func_aac_copy_channel_out_t copy_channel_out) {
  UINT valid;
  AAC_DECODER_ERROR err;
  INT_PCM *out = (INT_PCM *)pcm;
  int fs = 0;
  UINT flags = 0;
  CStreamInfo *info = 0;

  for (int i = 0; i < st->streams; ++i) {
    trace("stream %d", i);
    valid = size[i];
    flags = buffer[i] ? 0 : AACDEC_FLUSH;
    if (!flags) {
      err = aacDecoder_Fill(st->handles[i], &buffer[i], &size[i], &valid);
      if (err != AAC_DEC_OK) {
        return IAMF_ERR_INVALID_PACKET;
      }
    }
    err = aacDecoder_DecodeFrame(st->handles[i], st->buffer,
                                 def_max_aac_buffer_size, flags);
    if (err == AAC_DEC_NOT_ENOUGH_BITS) {
      return IAMF_ERR_BUFFER_TOO_SMALL;
    }
    if (err != AAC_DEC_OK) {
      error("stream %d : fail to decode", i);
      return IAMF_ERR_INTERNAL;
    }

    info = aacDecoder_GetStreamInfo(st->handles[i]);
    if (info) {
      fs = info->frameSize;
      if (fs > def_max_aac_frame_size) {
        warning("frame size %d , is larger than %u", fs,
                def_max_aac_frame_size);
        fs = def_max_aac_frame_size;
      }

      trace("aac decoder %d:", i);
      trace(" > sampleRate : %d.", info->sampleRate);
      trace(" > frameSize  : %d.", info->frameSize);
      trace(" > numChannels: %d.", info->numChannels);
      trace(" > outputDelay: %u.", info->outputDelay);

    } else {
      fs = frame_size;
    }

    if (info) {
      (*copy_channel_out)(out, st->buffer, fs, info->numChannels);
      out += (fs * info->numChannels);
    } else {
      warning("Can not get stream info.");
    }
  }

  if (info && st->delay < 0) st->delay = info->outputDelay;

  return fs;
}

aac_ms_decoder_t *aac_multistream_decoder_open(uint8_t *config, uint32_t size,
                                               int streams, int coupled_streams,
                                               uint32_t flags, int *error) {
  aac_ms_decoder_t *st = 0;
  HANDLE_AACDECODER *handles = 0;
  HANDLE_AACDECODER handle;

  UCHAR *conf[2] = {0, 0};
  UINT clen;

  int i, ret = 0;

  if (!config || !size || streams < 1) {
    ret = IAMF_ERR_BAD_ARG;
    goto end;
  }

  conf[0] = config;
  clen = size;
  conf[1] = def_malloc(UCHAR, clen);
  if (!conf[1]) {
    ret = IAMF_ERR_ALLOC_FAIL;
    goto end;
  }

  st = def_mallocz(aac_ms_decoder_t, 1);
  handles = def_mallocz(HANDLE_AACDECODER, streams);

  if (!st || !handles) {
    ret = IAMF_ERR_ALLOC_FAIL;
    def_free(handles);
    goto end;
  }

  st->flags = flags;
  st->streams = streams;
  st->coupled_streams = coupled_streams;
  st->delay = -1;
  st->handles = handles;
  memcpy(conf[1], conf[0], clen);
  aac_config_set_channels(conf[1], clen, 1);

  for (i = 0; i < st->streams; ++i) {
    handle = aacDecoder_Open(TT_MP4_RAW, 1);
    if (!handle) {
      ret = IAMF_ERR_INVALID_STATE;
      break;
    }

    st->handles[i] = handle;

    if (i < coupled_streams)
      ret = aacDecoder_ConfigRaw(handle, conf, &clen);
    else
      ret = aacDecoder_ConfigRaw(handle, &conf[1], &clen);
    if (ret != AAC_DEC_OK) {
      error("aac config raw error code %d", ret);
      ret = IAMF_ERR_INTERNAL;
      break;
    }
    ret = aacDecoder_SetParam(handle, AAC_CONCEAL_METHOD, 1);
    if (ret != AAC_DEC_OK) {
      error("aac set parameter error code %d", ret);
      ret = IAMF_ERR_INTERNAL;
      break;
    }
  }

end:
  if (ret < 0) {
    if (error) *error = ret;
    if (st) aac_multistream_decoder_close(st);
    st = 0;
  }

  def_free(conf[1]);

  return st;
}

int aac_multistream_decode(aac_ms_decoder_t *st, uint8_t *buffer[],
                           uint32_t size[], void *pcm, uint32_t frame_size) {
  if (st->flags & ck_audio_frame_planar)
    return aac_multistream_decode_native(st, buffer, size, pcm, frame_size,
                                         aac_copy_channel_out_short_plane);
  error("flags is 0x%x, is not implmeneted.", st->flags);
  return IAMF_ERR_UNIMPLEMENTED;
}

void aac_multistream_decoder_close(aac_ms_decoder_t *st) {
  if (st) {
    if (st->handles) {
      for (int i = 0; i < st->streams; ++i) {
        if (st->handles[i]) {
          aacDecoder_Close(st->handles[i]);
        }
      }
      free(st->handles);
    }
    free(st);
  }
}

int aac_multistream_decoder_get_delay(aac_ms_decoder_t *st) {
  return st->delay < 0 ? 0 : st->delay;
}
#endif