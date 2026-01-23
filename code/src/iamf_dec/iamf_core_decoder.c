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
 * @file iamf_core_decoder.c
 * @brief Core decoder.
 * @version 2.0.0
 * @date Created 03/03/2023
 **/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "clog.h"
#include "iamf_core_decoder.h"
#include "iamf_private_definitions.h"
#include "iamf_string.h"
#include "iamf_types.h"
#include "iamf_utils.h"
#include "iorw.h"

#ifdef def_log_tag
#undef def_log_tag
#endif

#define def_log_tag "IAMF_CORE"

#ifdef CONFIG_OPUS_CODEC
extern const iamf_codec_t iamf_opus_decoder;
#endif

#ifdef CONFIG_AAC_CODEC
extern const iamf_codec_t iamf_aac_decoder;
#endif

#ifdef CONFIG_FLAC_CODEC
extern const iamf_codec_t iamf_flac_decoder;
#endif

extern const iamf_codec_t iamf_pcm_decoder;

struct IamfCoreDecoder {
  iamf_codec_id_t cid;
  const iamf_codec_t *cdec;
  iamf_codec_context_t *ctx;

  int ambisonics;
  void *matrix;
  void *buffer;
};

typedef struct FloatMatrix {
  float *matrix;
  int row;
  int column;
} FloatMatrix;

static const iamf_codec_t *_g_codecs[] = {0,
#ifdef CONFIG_OPUS_CODEC
                                          &iamf_opus_decoder,
#else
                                          0,
#endif
#ifdef CONFIG_AAC_CODEC
                                          &iamf_aac_decoder,
#else
                                          0,
#endif
#ifdef CONFIG_FLAC_CODEC
                                          &iamf_flac_decoder,
#else
                                          0,
#endif
                                          &iamf_pcm_decoder

};

static int iamf_core_decoder_convert_mono(iamf_core_decoder_t *ths, float *out,
                                          float *in, uint32_t frame_size) {
  uint8_t *map = ths->matrix;

  memset(out, 0, sizeof(float) * frame_size * ths->ctx->channels);
  for (int i = 0; i < ths->ctx->channels; ++i) {
    if (map[i] != 255)
      memcpy(&out[frame_size * i], &in[frame_size * map[i]],
             frame_size * sizeof(float));
    else
      memset(&out[frame_size * i], 0, frame_size * sizeof(float));
  }
  return IAMF_OK;
}
static int iamf_core_decoder_convert_projection(iamf_core_decoder_t *ths,
                                                float *out, float *in,
                                                uint32_t frame_size) {
  FloatMatrix *matrix = ths->matrix;
  for (int s = 0; s < frame_size; ++s) {
    for (int r = 0; r < matrix->row; ++r) {
      out[r * frame_size + s] = .0f;
      for (int l = 0; l < matrix->column; ++l) {
        out[r * frame_size + s] +=
            in[l * frame_size + s] * matrix->matrix[l * matrix->row + r];
      }
    }
  }
  return IAMF_OK;
}

iamf_core_decoder_t *iamf_core_decoder_open(iamf_codec_id_t cid) {
  iamf_core_decoder_t *ths = 0;
  iamf_codec_context_t *ctx = 0;
  iamf_codec_type_t type = ck_iamf_codec_type_none;
  int ec = IAMF_OK;

  if (!iamf_codec_id_check(cid)) {
    error("Invlid codec id %u", cid);
    return 0;
  }

  type = iamf_codec_type_get(cid);
  if (!_g_codecs[type]) {
    error("Unimplment %s decoder.", iamf_codec_type_string(type));
    return 0;
  }

  ths = def_mallocz(iamf_core_decoder_t, 1);
  if (!ths) {
    error("%s : iamf_core_decoder_t handle.",
          iamf_error_code_string(IAMF_ERR_ALLOC_FAIL));
    return 0;
  }

  ths->cid = cid;
  ths->cdec = _g_codecs[type];

  ctx = def_mallocz(iamf_codec_context_t, 1);
  if (!ctx) {
    ec = IAMF_ERR_ALLOC_FAIL;
    error("%s : iamf_codec_context_t handle.", iamf_error_code_string(ec));
    goto termination;
  }
  ths->ctx = ctx;

  ctx->priv = def_mallocz(char, ths->cdec->priv_size);
  if (!ctx->priv) {
    ec = IAMF_ERR_ALLOC_FAIL;
    error("%s : private data.", iamf_error_code_string(ec));
    goto termination;
  }

termination:
  if (ec != IAMF_OK) {
    iamf_core_decoder_close(ths);
    ths = 0;
  }
  return ths;
}

void iamf_core_decoder_close(iamf_core_decoder_t *ths) {
  if (ths) {
    if (ths->ctx) {
      if (ths->cdec) ths->cdec->close(ths->ctx);
      if (ths->ctx->priv) {
        free(ths->ctx->priv);
      }
      free(ths->ctx);
    }

    if (ths->matrix) {
      if (ths->ambisonics == ck_stream_mode_ambisonics_projection) {
        FloatMatrix *fm = ths->matrix;
        if (fm->matrix) free(fm->matrix);
      }
      free(ths->matrix);
    }

    if (ths->buffer) free(ths->buffer);

    free(ths);
  }
}

int iamf_core_decoder_init(iamf_core_decoder_t *ths) {
  iamf_codec_context_t *ctx = ths->ctx;
  return ths->cdec->init(ctx);
}

int iamf_core_decoder_set_codec_conf(iamf_core_decoder_t *ths, uint8_t *spec,
                                     uint32_t len) {
  iamf_codec_context_t *ctx = ths->ctx;
  ctx->cspec = spec;
  ctx->clen = len;
  return IAMF_OK;
}

int iamf_core_decoder_set_streams_info(iamf_core_decoder_t *ths, uint32_t mode,
                                       uint8_t channels, uint8_t streams,
                                       uint8_t coupled_streams,
                                       uint8_t mapping[],
                                       uint32_t mapping_size) {
  iamf_codec_context_t *ctx = ths->ctx;
  ths->ambisonics = mode;
  ctx->channels = channels;
  ctx->streams = streams;
  ctx->coupled_streams = coupled_streams;
  if (mapping && mapping_size > 0) {
    if (ths->ambisonics == ck_stream_mode_ambisonics_projection) {
      FloatMatrix *matrix = def_mallocz(FloatMatrix, 1);
      int count;
      io_context_t ioc, *r;
      float *factors;

      if (!matrix) return IAMF_ERR_ALLOC_FAIL;

      r = &ioc;
      ioc_init(r, mapping, mapping_size);

      matrix->row = ctx->channels;
      matrix->column = ctx->streams + ctx->coupled_streams;
      count = matrix->row * matrix->column;
      factors = def_mallocz(float, count);
      if (!factors) {
        free(matrix);
        return IAMF_ERR_ALLOC_FAIL;
      }
      matrix->matrix = factors;

      for (int i = 0; i < count; ++i) {
        factors[i] = iamf_q15_to_float(def_lsb_16bits(ior_b16(r)));
        info("factor %d : %f", i, factors[i]);
      }
      ths->matrix = matrix;
    } else if (ths->ambisonics == ck_stream_mode_ambisonics_mono) {
      uint8_t *matrix = def_mallocz(uint8_t, mapping_size);
      if (!matrix) return IAMF_ERR_ALLOC_FAIL;

      if (channels != mapping_size) {
        free(matrix);
        error("Invalid ambisonics mono info.");
        return IAMF_ERR_BAD_ARG;
      }
      memcpy(matrix, mapping, mapping_size);
      ths->matrix = matrix;
    }
  }
  return IAMF_OK;
}

int iamf_core_decoder_decode(iamf_core_decoder_t *ths, uint8_t *buffer[],
                             uint32_t size[], uint32_t count, float *out,
                             uint32_t frame_size) {
  int ret = IAMF_OK;
  iamf_codec_context_t *ctx = ths->ctx;
  if (ctx->streams != count) return IAMF_ERR_BUFFER_TOO_SMALL;
  if (ths->ambisonics == ck_stream_mode_ambisonics_none)
    return ths->cdec->decode(ctx, buffer, size, count, out, frame_size);

  if (!ths->buffer) {
    float *block = 0;

    block = def_mallocz(float, (ctx->channels * frame_size));
    if (!block) return IAMF_ERR_ALLOC_FAIL;
    ths->buffer = block;
  }
  ret = ths->cdec->decode(ctx, buffer, size, count, ths->buffer, frame_size);
  if (ret > 0) {
    if (ths->ambisonics == ck_stream_mode_ambisonics_projection)
      iamf_core_decoder_convert_projection(ths, out, ths->buffer, ret);
    else
      iamf_core_decoder_convert_mono(ths, out, ths->buffer, ret);
  }
  return ret;
}

uint32_t iamf_core_decoder_get_delay(iamf_core_decoder_t *ths) {
  if (ths) {
    if (ths->cdec && ths->cdec->info) (ths->cdec->info)(ths->ctx);
    if (ths->ctx) return ths->ctx->delay;
  }
  return 0;
}
