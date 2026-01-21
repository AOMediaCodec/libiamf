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
 * @file iamfdec_private.h
 * @brief IAMF decoder header file.
 * @version 2.0.0
 * @date Created 12/11/2025
 **/

#ifndef __IAMFDEC_PRIVATE_H__
#define __IAMFDEC_PRIVATE_H__

#include "IAMF_defines.h"

#if SUPPORT_VERIFIER
#define def_flag_vlog 0x2
#endif

#define def_flag_disable_limiter 0x4


#define def_default_sampling_rate 48000
#define def_default_profile IA_PROFILE_ADVANCED_2
#define def_default_mix_presentation_id -1LL
#define def_block_size 960 * 6 * 2 * 16


typedef struct Layout {
  int type;
  union {
    IAMF_SoundSystem ss;
  };
} layout_t;

typedef struct DecoderArgs {
  const char *path;
  const char *output_path;
  layout_t *layout;
  float peak;
  float loudness;
  uint32_t flags;
  uint32_t st;
  uint32_t rate;
  uint32_t bit_depth;
  int64_t mix_presentation_id;
  IA_Profile profile;
} decoder_args_t;

#endif  // __IAMFDEC_PRIVATE_H__
