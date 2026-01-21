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
 * @file IAMF_aac_decoder.c
 * @brief aac codec.
 * @version 0.1
 * @date Created 03/03/2023
 **/

#ifdef CONFIG_AAC_CODEC
#include <stdlib.h>

#include "aac_multistream_decoder.h"
#include "clog.h"
#include "iamf_codec.h"
#include "iamf_private_definitions.h"
#include "iamf_types.h"
#include "iorw.h"

#ifdef def_log_tag
#undef def_log_tag
#endif

#define def_log_tag "IAMF_AAC"

static int iamf_aac_close(iamf_codec_context_t *ths);

typedef struct IamfAacContext {
  aac_ms_decoder_t *dec;
  short *out;
} iamf_aac_context_t;

/**
 *  IAC-AAC-LC Specific
 *
 *  Constraints :
 *  1, objectTypeIndication = 0x40
 *  2, streamType = 0x05 (Audio Stream)
 *  3, upstream = 0
 *  4, decSpecificInfo(): The syntax and values shall conform to
 *     AudioSpecificConfig() of [MP4-Audio] with following constraints:
 *     1> audioObjectType = 2
 *     2> channelConfiguration should be set to 2
 *     3> GASpecificConfig(): The syntax and values shall conform to
 *        GASpecificConfig() of [MP4-Audio] with following constraints:
 *        1) frameLengthFlag = 0 (1024 lines IMDCT)
 *        2) dependsOnCoreCoder = 0
 *        3) extensionFlag = 0
 * */

static int iamf_aac_init(iamf_codec_context_t *ths) {
  iamf_aac_context_t *ctx = (iamf_aac_context_t *)ths->priv;
  uint8_t *config = ths->cspec;
  io_context_t ioc, *r;
  int ret = 0;
  uint32_t val;

  r = &ioc;
  ioc_init(r, ths->cspec, ths->clen);
  if (ior_8(r) != 0x04) return IAMF_ERR_BAD_ARG;
  ior_expandable(r);

  if (ior_8(r) != 0x40) return IAMF_ERR_BAD_ARG;

  val = ior_8(r);
  if (val >> 2 != 5 || val >> 1 & 1) return IAMF_ERR_BAD_ARG;

  ior_skip(r, 11);

  if (ior_8(r) != 0x05) return IAMF_ERR_BAD_ARG;

  ths->clen = ior_expandable(r);
  ths->cspec = config + ioc_tell(r);
  debug("aac codec spec info size %d", ths->clen);

  ctx->dec = aac_multistream_decoder_open(ths->cspec, ths->clen, ths->streams,
                                          ths->coupled_streams,
                                          ck_audio_frame_planar, &ret);
  if (!ctx->dec) return IAMF_ERR_INVALID_STATE;

  ctx->out = (short *)malloc(sizeof(short) * def_max_aac_frame_size *
                             (ths->streams + ths->coupled_streams));
  if (!ctx->out) {
    iamf_aac_close(ths);
    return IAMF_ERR_ALLOC_FAIL;
  }

  return IAMF_OK;
}

static int iamf_aac_decode(iamf_codec_context_t *ths, uint8_t *buffer[],
                           uint32_t size[], uint32_t count, void *pcm,
                           uint32_t frame_size) {
  iamf_aac_context_t *ctx = (iamf_aac_context_t *)ths->priv;
  aac_ms_decoder_t *dec = (aac_ms_decoder_t *)ctx->dec;
  int ret = IAMF_OK;

  if (count != ths->streams) {
    return IAMF_ERR_BAD_ARG;
  }

  ret = aac_multistream_decode(dec, buffer, size, ctx->out, frame_size);
  if (ret > 0) {
    float *out = (float *)pcm;
    uint32_t samples = ret * (ths->streams + ths->coupled_streams);
    for (int i = 0; i < samples; ++i) {
      out[i] = ctx->out[i] / 32768.f;
    }
  }

  return ret;
}

static int iamf_aac_flush(iamf_codec_context_t *ths) {
  iamf_aac_context_t *ctx = (iamf_aac_context_t *)ths->priv;
  return aac_multistream_decoder_flush(ctx->dec);
}

static int iamf_aac_info(iamf_codec_context_t *ths) {
  iamf_aac_context_t *ctx = (iamf_aac_context_t *)ths->priv;
  aac_ms_decoder_t *dec = (aac_ms_decoder_t *)ctx->dec;
  ths->delay = aac_multistream_decoder_get_delay(dec);
  return IAMF_OK;
}

int iamf_aac_close(iamf_codec_context_t *ths) {
  iamf_aac_context_t *ctx = (iamf_aac_context_t *)ths->priv;
  aac_ms_decoder_t *dec = (aac_ms_decoder_t *)ctx->dec;

  if (dec) {
    aac_multistream_decoder_close(dec);
    ctx->dec = 0;
  }
  if (ctx->out) {
    free(ctx->out);
  }

  return IAMF_OK;
}

const iamf_codec_t iamf_aac_decoder = {
    .cid = ck_iamf_codec_id_aac,
    .priv_size = sizeof(iamf_aac_context_t),
    .init = iamf_aac_init,
    .decode = iamf_aac_decode,
    .flush = iamf_aac_flush,
    .info = iamf_aac_info,
    .close = iamf_aac_close,
};
#endif