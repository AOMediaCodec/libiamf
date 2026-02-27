/*
 * Copyright (c) 2025, Alliance for Open Media. All rights reserved
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
 * @file codec_config_obu.h
 * @brief Codec config OBU APIs.
 * @version 2.0.0
 * @date Created 12/11/2025
 **/

#ifndef __CODEC_CONFIG_OBU_H__
#define __CODEC_CONFIG_OBU_H__

#include "iamf_obu.h"
#include "iamf_types.h"

#define def_codec_config_obu_ptr(a) ((iamf_codec_config_obu_t *)a)

/**
 * Codec Config OBU.
 * */

typedef struct IamfCodecConfigObu {
  iamf_obu_t obu;

  uint32_t codec_config_id;

  // codec config
  uint32_t codec_id;
  uint32_t num_samples_per_frame;
  int audio_roll_distance;

  buffer_wrap_t *decoder_config;
} iamf_codec_config_obu_t;

iamf_codec_config_obu_t *iamf_codec_config_obu_new(io_context_t *ior);
void iamf_codec_config_obu_free(iamf_codec_config_obu_t *obu);
int iamf_codec_config_obu_get_parameter(iamf_codec_config_obu_t *obu,
                                        audio_codec_parameter_t *param);
#endif  // __CODEC_CONFIG_OBU_H__
