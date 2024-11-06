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
 * @file override_arm.c
 * @brief Override with Arm function implementations.
 * @version 0.1
 * @date Created 10/24/2024
 **/

#include "detect_arm.h"

#if defined(IAMF_ARCH_DETECTED_ARM)

#include <arm_neon.h>

#include "../../arch.h"
#include "arm_multiply_channels.h"
#include "arm_zip_channels.h"

void arch_override(Arch *arch) {
  // Override functions with Arm implementations here

  arch->rendering.multiply_channels_by_matrix =
      &multiply_channels_by_matrix_neon;

  arch->output.float2int16_zip_channels = &float2int16_zip_channels_neon;
  arch->output.float2int24_zip_channels = &float2int24_zip_channels_neon;
  arch->output.float2int32_zip_channels = &float2int32_zip_channels_neon;
}

#endif
