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
 * @file IAMF_opus_decoder.c
 * @brief opus codec.
 * @version 0.1
 * @date Created 03/03/2023
 **/

#include <stdlib.h>

#include "clog.h"
#include "iamf_codec.h"
#include "iamf_private_definitions.h"
#include "opus_multistream2_decoder.h"

#ifdef def_log_tag
#undef def_log_tag
#endif

#define def_log_tag "IAMF_OPUS"

#ifdef CONFIG_OPUS_CODEC
typedef struct IamfOpusContext {
  void *dec;
  short *out;
} iamf_opus_context_t;

static int iamf_opus_close(iamf_codec_context_t *ths);

/**
 *  IAC-OPUS Specific
 *
 *   0                   1                   2                   3
 *   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |  Version = 1  | Channel Count |           Pre-skip            |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |                     Input Sample Rate (Hz)                    |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |   Output Gain (Q7.8 in dB)    | Mapping Family|
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 *  The fields in the ID Header:
 *
 *  unsigned int(8) Version;
 *  unsigned int(8) OutputChannelCount;
 *  unsigned int(16) PreSkip;
 *  unsigned int(32) InputSampleRate;
 *  signed int(16) OutputGain;
 *  unsigned int(8) ChannelMappingFamily;
 *
 *
 *  Constraints:
 *
 *  1, ChannelMappingFamily = 0
 *  2, Channel Count should be set to 2
 *  3, Output Gain shall not be used. In other words, it shall be set to 0dB
 *  4, The byte order of each field in ID Header shall be converted to big
 * endian
 *
 * */
static int iamf_opus_init(iamf_codec_context_t *ths) {
  iamf_opus_context_t *ctx = (iamf_opus_context_t *)ths->priv;
  int ec = IAMF_OK;
  int ret = 0;

  ths->sample_rate = 48000;

  ctx->dec = opus_multistream2_decoder_create(ths->sample_rate, ths->streams,
                                              ths->coupled_streams,
                                              ck_audio_frame_planar, &ret);
  if (!ctx->dec) {
    error("fail to open opus decoder.");
    ec = IAMF_ERR_INVALID_STATE;
  }

  ctx->out = (short *)malloc(sizeof(short) * def_max_opus_frame_size *
                             (ths->streams + ths->coupled_streams));
  if (!ctx->out) {
    iamf_opus_close(ths);
    return IAMF_ERR_ALLOC_FAIL;
  }

  return ec;
}

static int iamf_opus_decode(iamf_codec_context_t *ths, uint8_t *buf[],
                            uint32_t len[], uint32_t count, void *pcm,
                            const uint32_t frame_size) {
  iamf_opus_context_t *ctx = (iamf_opus_context_t *)ths->priv;
  Opus_Ms2_Decoder_t *dec = (Opus_Ms2_Decoder_t *)ctx->dec;
  int ret = IAMF_OK;

  if (count != ths->streams) {
    return IAMF_ERR_BAD_ARG;
  }
  ret = opus_multistream2_decode(dec, buf, len, ctx->out, frame_size);
  if (ret > 0) {
    float *out = (float *)pcm;
    uint32_t samples = ret * (ths->streams + ths->coupled_streams);
    for (int i = 0; i < samples; ++i) {
      out[i] = ctx->out[i] / 32768.f;
    }
  }
  return ret;
}

static int iamf_opus_flush(iamf_codec_context_t *ths) {
  iamf_opus_context_t *ctx = (iamf_opus_context_t *)ths->priv;
  return opus_multistream2_decoder_flush(ctx->dec);
}

int iamf_opus_close(iamf_codec_context_t *ths) {
  iamf_opus_context_t *ctx = (iamf_opus_context_t *)ths->priv;
  Opus_Ms2_Decoder_t *dec = (Opus_Ms2_Decoder_t *)ctx->dec;

  if (dec) {
    opus_multistream2_decoder_destroy(dec);
    ctx->dec = 0;
  }

  if (ctx->out) {
    free(ctx->out);
  }
  return IAMF_OK;
}

const iamf_codec_t iamf_opus_decoder = {
    .cid = ck_iamf_codec_id_opus,
    .priv_size = sizeof(iamf_opus_context_t),
    .init = iamf_opus_init,
    .decode = iamf_opus_decode,
    .flush = iamf_opus_flush,
    .close = iamf_opus_close,
};
#endif