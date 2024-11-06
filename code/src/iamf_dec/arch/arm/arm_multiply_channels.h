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
 * @file arm_multiply_channels.h
 * @brief Arm implementation for multiplying channels.
 * @version 0.1
 * @date Created 10/24/2024
 **/

#ifndef ARM_MULTIPLY_CHANNELS_H_
#define ARM_MULTIPLY_CHANNELS_H_

#include "detect_arm.h"

#if defined(IAMF_ARCH_DETECTED_ARM)

void multiply_channels_by_matrix_neon(float *mat, int in_dim, int in_next,
                                      int *in_idx_map, int out_dim,
                                      int out_next, float **in, float **out,
                                      int nsamples);

#endif /* IAMF_ARCH_DETECTED_ARM */
#endif /* ARM_MULTIPLY_CHANNELS_H_ */
