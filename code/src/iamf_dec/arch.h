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
 * @file arch.h
 * @brief Collection of CPU-specific functions.
 * @version 0.1
 * @date Created 10/24/2024
 **/

#ifndef ARCH_H_
#define ARCH_H_

#include <stdint.h>

typedef struct ArchCallbacks {
  // Functions with possible architecture-specific optimizations
  struct {
    void (*multiply_channels_by_matrix)(float *mat, int in_dim, int in_next,
                                        int *in_idx_map, int out_dim,
                                        int out_next, float **in, float **out,
                                        int nsamples);
  } rendering;
  struct {
    void (*float2int16_zip_channels)(const float *src, int next_channel,
                                     int channels, int16_t *int16_dst,
                                     int nsamples);
    void (*float2int24_zip_channels)(const float *src, int next_channel,
                                     int channels, uint8_t *int24_dst,
                                     int nsamples);
    void (*float2int32_zip_channels)(const float *src, int next_channel,
                                     int channels, int32_t *int32_dst,
                                     int nsamples);
  } output;
} Arch;

Arch *arch_create();
void arch_destroy(Arch *arch);

#endif /* ARCH_H_ */
