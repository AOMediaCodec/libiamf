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
 * @file iamf_decoder_private.h
 * @brief IAMF decoder internal APIs.
 * @version 2.0.0
 * @date Created 03/03/2023
 **/

#ifndef IAMF_DECODER_PRIVATE_H
#define IAMF_DECODER_PRIVATE_H

#include <stdint.h>

#include "iamf_database.h"
#include "iamf_element_reconstructor.h"
#include "iamf_post_processor.h"
#include "iamf_presentation.h"

typedef enum EIamfDecoderStatus {
  ck_iamf_decoder_status_init,
  ck_iamf_decoder_status_parse_1,
  ck_iamf_decoder_status_configure,
  ck_iamf_decoder_status_reconfigure,
  ck_iamf_decoder_status_parse_2,
  ck_iamf_decoder_status_process,
} iamf_decoder_status_t;

struct IAMF_DecoderContext {
  uint32_t flags;
  iamf_decoder_status_t status;
  uint32_t configure_flags;

  uint32_t bit_depth;
  uint32_t sampling_rate;

  fraction_t frame_duration;

  iamf_layout_t layout;
  int64_t mix_presentation_id;

  hash_map_t *element_gains;

  float loudness_lkfs;
  float normalized_loudness_lkfs;
  float limiter_threshold_db;
  int enable_limiter;

  quaternion_t head_rotation;
  uint32_t head_tracking_enabled;

  int key_frame;
  int delimiter;

  struct {
    uint32_t decoder;  // delay for decoder
    uint32_t padding;  // number samples of trimming at end
  } cache;

  iamf_database_t database;
};

typedef struct IAMF_DecoderContext iamf_decoder_context_t;

struct IAMF_Decoder {
  iamf_decoder_context_t ctx;

  iamf_obu_parser_t *parser;
  iamf_element_reconstructor_t *reconstructor;
  iamf_presentation_t *presentation;
  iamf_post_processor_t *post_processor;
};

typedef struct IAMF_Decoder iamf_decoder_t;

#endif /* IAMF_DECODER_PRIVATE_H */
