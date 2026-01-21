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
 * @file iamf_codec.h
 * @brief Common codec APIs.
 * @version 0.1
 * @date Created 03/03/2023
 **/

#ifndef __IAMF_CODEC_H__
#define __IAMF_CODEC_H__

#include <stdint.h>

#include "iamf_types.h"

typedef struct IamfCodecContext {
  void *priv;

  uint8_t *cspec;
  uint32_t clen;

  uint32_t delay;
  uint32_t flags;
  uint32_t sample_rate;
  uint32_t sample_size;

  uint8_t streams;
  uint8_t coupled_streams;
  uint8_t channels;
} iamf_codec_context_t;

typedef struct IamfCodec {
  iamf_codec_id_t cid;
  uint32_t flags;
  uint32_t priv_size;
  int (*init)(iamf_codec_context_t *ths);
  int (*decode)(iamf_codec_context_t *ths, uint8_t *buf[], uint32_t len[],
                uint32_t count, void *pcm, const uint32_t frame_size);
  int (*flush)(iamf_codec_context_t *ths);
  int (*info)(iamf_codec_context_t *ths);
  int (*close)(iamf_codec_context_t *ths);
} iamf_codec_t;

#endif /* __IAMF_CODEC_H__ */
