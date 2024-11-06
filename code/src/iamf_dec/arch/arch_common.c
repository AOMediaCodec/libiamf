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
 * @file arch_common.c
 * @brief C implementation for CPU-specific functions.
 * @version 0.1
 * @date Created 10/24/2024
 **/

#include "arch_common.h"

void multiply_channels_by_matrix_c(float *mat, int in_dim, int in_next,
                                   int *in_idx_map, int out_dim, int out_next,
                                   float **in, float **out, int nsamples) {
  int i, in_idx, out_idx;

  if (in_dim <= 0 || out_dim <= 0) return;

  for (in_idx = 0; in_idx < in_dim; in_idx++) {
    const int in_mapped_idx = in_idx_map ? in_idx_map[in_idx] : in_idx;

    for (out_idx = 0; out_idx < out_dim; out_idx++) {
      const float c = mat[out_idx * out_next + in_mapped_idx * in_next];

      for (i = 0; i < nsamples; i++) {
        if (in_idx == 0) {
          out[out_idx][i] = 0;
        }
        out[out_idx][i] += c * in[in_idx][i];
      }
    }
  }
}
