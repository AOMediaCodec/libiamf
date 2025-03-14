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
 * @file arm_zip_channels.h
 * @brief Arm implementation for zipping channels.
 * @version 0.1
 * @date Created 10/24/2024
 **/

#ifndef ARM_ZIP_CHANNELS_H_
#define ARM_ZIP_CHANNELS_H_

#include "detect_arm.h"

#if defined(IAMF_ARCH_DETECTED_ARM)

#include <stdint.h>

void float2int16_zip_channels_neon(const float *src, int next_channel,
                                   int channels, int16_t *int16_dst,
                                   int nsamples);

void float2int24_zip_channels_neon(const float *src, int next_channel,
                                   int channels, uint8_t *int24_dst,
                                   int nsamples);

void float2int32_zip_channels_neon(const float *src, int next_channel,
                                   int channels, int32_t *int32_dst,
                                   int nsamples);

#endif /* IAMF_ARCH_DETECTED_ARM */
#endif /* ARM_ZIP_CHANNELS_H_ */
