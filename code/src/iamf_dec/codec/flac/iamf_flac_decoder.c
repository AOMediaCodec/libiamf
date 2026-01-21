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
 * @file IAMF_flac_decoder.c
 * @brief flac codec.
 * @version 0.1
 * @date Created 03/03/2023
 **/

#ifdef CONFIG_FLAC_CODEC
#include <stdlib.h>

#include "clog.h"
#include "flac_multistream_decoder.h"
#include "iamf_codec.h"
#include "iamf_private_definitions.h"
#include "iamf_types.h"
#include "iamf_utils.h"

#ifdef def_log_tag
#undef def_log_tag
#endif

#define def_log_tag "IAMF_FLAC"

static int iamf_flac_close(iamf_codec_context_t *ths);

typedef struct IamfFlacContext {
  flac_ms_decoder_t *dec;
  int *out;
  float scale_i2f;
} iamf_flac_context_t;

static int iamf_flac_init(iamf_codec_context_t *ths) {
  iamf_flac_context_t *ctx = (iamf_flac_context_t *)ths->priv;
  int ret = IAMF_OK;
  int bits = 0;

  if (!ths->cspec || ths->clen <= 0) {
    return IAMF_ERR_BAD_ARG;
  }

  ctx->dec = flac_multistream_decoder_open(ths->cspec, ths->clen, ths->streams,
                                           ths->coupled_streams,
                                           ck_audio_frame_planar, &ret);
  if (!ctx->dec) {
    return ret;
  }

  bits = flac_multistream_decoder_get_sample_bits(ctx->dec);
  if (bits < 1) {
    error("metadata : Invalid sample bits.");
    iamf_flac_close(ths);
    return IAMF_ERR_INTERNAL;
  }

  ctx->scale_i2f = 1 << (bits - 1);
  debug("the scale of i%d to float : %f", bits, ctx->scale_i2f);

  ctx->out = def_malloc(
      int, def_max_flac_frame_size *(ths->streams + ths->coupled_streams));
  if (!ctx->out) {
    iamf_flac_close(ths);
    return IAMF_ERR_ALLOC_FAIL;
  }

  return IAMF_OK;
}

static int iamf_flac_decode(iamf_codec_context_t *ths, uint8_t *buffer[],
                            uint32_t size[], uint32_t count, void *pcm,
                            uint32_t frame_size) {
  iamf_flac_context_t *ctx = (iamf_flac_context_t *)ths->priv;
  flac_ms_decoder_t *dec = (flac_ms_decoder_t *)ctx->dec;
  int ret = IAMF_OK;

  if (count != ths->streams) {
    return IAMF_ERR_BAD_ARG;
  }

  ret = flac_multistream_decode(dec, buffer, size, ctx->out, frame_size);
  if (ret > 0) {
    float *out = (float *)pcm;
    uint32_t samples = ret * (ths->streams + ths->coupled_streams);
    for (int i = 0; i < samples; ++i) {
      out[i] = ctx->out[i] / ctx->scale_i2f;
    }
  }

  return ret;
}

int iamf_flac_close(iamf_codec_context_t *ths) {
  iamf_flac_context_t *ctx = (iamf_flac_context_t *)ths->priv;
  flac_ms_decoder_t *dec = (flac_ms_decoder_t *)ctx->dec;

  if (dec) {
    flac_multistream_decoder_close(dec);
    ctx->dec = 0;
  }
  if (ctx->out) {
    free(ctx->out);
  }

  return IAMF_OK;
}

const iamf_codec_t iamf_flac_decoder = {
    .cid = ck_iamf_codec_id_flac,
    .priv_size = sizeof(iamf_flac_context_t),
    .init = iamf_flac_init,
    .decode = iamf_flac_decode,
    .close = iamf_flac_close,
};
#endif