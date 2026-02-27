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
 * @file animated_parameter.c
 * @brief Animated parameter implementation.
 * @version 2.0.0
 * @date Created 12/11/2025
 **/

#include "animated_parameter.h"

#include <math.h>

#include "iamf_private_definitions.h"
#include "iamf_utils.h"

#undef def_log_tag
#define def_log_tag "AMT_PMT"

int animated_parameter_data_read_bits(bits_io_context_t *bits_r,
                                      uint32_t n_bits,
                                      iamf_animation_type_t type,
                                      apd_u32_t *param) {
  int ret = IAMF_OK;
  if (!bits_r || !param) return IAMF_ERR_BAD_ARG;

  switch (type) {
    case ck_iamf_animation_type_step:
      param->start = bits_ior_le32(bits_r, n_bits);
      break;
    case ck_iamf_animation_type_linear:
      param->start = bits_ior_le32(bits_r, n_bits);
    case ck_iamf_animation_type_inter_linear:
      param->end = bits_ior_le32(bits_r, n_bits);
      break;
    case ck_iamf_animation_type_bezier:
      param->start = bits_ior_le32(bits_r, n_bits);
    case ck_iamf_animation_type_inter_bezier:
      param->end = bits_ior_le32(bits_r, n_bits);
      param->control = bits_ior_le32(bits_r, n_bits);
      param->control_relative_time = bits_ior_le32(bits_r, 8);
      break;
    default:
      error("Unknown animation type: %u", type);
      ret = IAMF_ERR_BAD_ARG;
      break;
  }

  return ret;
}
