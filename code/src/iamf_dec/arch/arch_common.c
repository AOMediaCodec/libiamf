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

#include <math.h>

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

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

static int16_t FLOAT2INT16(float x) {
  x = x * (float)(1 << 15);
  x = MAX(x, INT16_MIN);
  x = MIN(x, INT16_MAX);
  return (int16_t)lrintf(x);
}

static int32_t FLOAT2INT24(float x) {
  #define INT24_MAX (8388607)
  #define INT24_MIN (-8388608)

  x = x * (float)(1 << 23);
  x = MAX(x, (float)INT24_MIN);
  x = MIN(x, (float)INT24_MAX);
  return (int32_t)lrintf(x);
}

static int32_t FLOAT2INT32(float x) {
  // unary minus applied to maintain correct signedness
  x = x * -(float)(1 << 31);
  if (x > (float)INT32_MIN && x < (float)INT32_MAX)
    return (int32_t)lrintf(x);
  else
    return (x > 0.0f ? INT32_MAX : INT32_MIN);
}

void float2int16_zip_channels_c(const float *src, int next_channel,
                                int channels, int16_t *int16_dst,
                                int nsamples) {
  int i, c;

  for (c = 0; c < channels; ++c) {
    for (i = 0; i < nsamples; i++) {
      int16_dst[i * channels + c] = FLOAT2INT16(src[next_channel * c + i]);
    }
  }
}

void float2int24_zip_channels_c(const float *src, int next_channel,
                                int channels, uint8_t *int24_dst,
                                int nsamples) {
  int i, c;

  for (c = 0; c < channels; ++c) {
    for (i = 0; i < nsamples; i++) {
      int32_t tmp = FLOAT2INT24(src[next_channel * c + i]);
      int24_dst[(i * channels + c) * 3 + 0] = tmp & 0xff;
      int24_dst[(i * channels + c) * 3 + 1] = (tmp >> 8) & 0xff;
      int24_dst[(i * channels + c) * 3 + 2] = (tmp >> 16) & 0xff;
    }
  }
}

void float2int32_zip_channels_c(const float *src, int next_channel,
                                int channels, int32_t *int32_dst,
                                int nsamples) {
  int i, c;

  for (c = 0; c < channels; ++c) {
    for (i = 0; i < nsamples; i++) {
      int32_dst[i * channels + c] = FLOAT2INT32(src[next_channel * c + i]);
    }
  }
}
