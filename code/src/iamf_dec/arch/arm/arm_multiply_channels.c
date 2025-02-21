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
 * @file arm_multiply_channels.c
 * @brief Arm implementation for multiplying channels.
 * @version 0.1
 * @date Created 10/24/2024
 **/

#include "arm_multiply_channels.h"

#if defined(IAMF_ARCH_DETECTED_ARM)

#include <arm_neon.h>

#if defined(__aarch64__)
#define USE_16_ACCUMULATORS (1)
#endif

void multiply_channels_by_matrix_neon(float* mat, int in_dim, int in_next,
                                      int* in_idx_map, int out_dim,
                                      int out_next, float** in, float** out,
                                      int nsamples) {
  int i = 0, out_idx, in_idx;

  if (in_dim <= 0 || out_dim <= 0) return;
#ifdef USE_16_ACCUMULATORS
  const int BLOCK_SIZE = 32;
#else
  const int BLOCK_SIZE = 16;
#endif

  const int blocked_size = nsamples / BLOCK_SIZE * BLOCK_SIZE;

  for (i = 0; i < blocked_size; i += BLOCK_SIZE) {
    for (out_idx = 0; out_idx < (out_dim & ~1); out_idx += 2) {
      float* outPtr_a = out[out_idx + 0] + i;
      float* outPtr_b = out[out_idx + 1] + i;

      float32x4_t sum_a1 = vdupq_n_f32(0.0f);
      float32x4_t sum_a2 = vdupq_n_f32(0.0f);
      float32x4_t sum_a3 = vdupq_n_f32(0.0f);
      float32x4_t sum_a4 = vdupq_n_f32(0.0f);
#ifdef USE_16_ACCUMULATORS
      float32x4_t sum_a5 = vdupq_n_f32(0.0f);
      float32x4_t sum_a6 = vdupq_n_f32(0.0f);
      float32x4_t sum_a7 = vdupq_n_f32(0.0f);
      float32x4_t sum_a8 = vdupq_n_f32(0.0f);
#endif

      float32x4_t sum_b1 = vdupq_n_f32(0.0f);
      float32x4_t sum_b2 = vdupq_n_f32(0.0f);
      float32x4_t sum_b3 = vdupq_n_f32(0.0f);
      float32x4_t sum_b4 = vdupq_n_f32(0.0f);
#ifdef USE_16_ACCUMULATORS
      float32x4_t sum_b5 = vdupq_n_f32(0.0f);
      float32x4_t sum_b6 = vdupq_n_f32(0.0f);
      float32x4_t sum_b7 = vdupq_n_f32(0.0f);
      float32x4_t sum_b8 = vdupq_n_f32(0.0f);
#endif

      for (in_idx = 0; in_idx < in_dim; in_idx++) {
        const int in_mapped_idx = in_idx_map ? in_idx_map[in_idx] : in_idx;
        const int c_a_offset = out_idx * out_next + in_mapped_idx * in_next;
        const int c_b_offset = c_a_offset + out_next;
        const float* inPtr = in[in_idx] + i;

        float32x4_t c_a = vld1q_dup_f32(&mat[c_a_offset]);
        float32x4_t c_b = vld1q_dup_f32(&mat[c_b_offset]);

        float32x4_t in_1 = vld1q_f32(inPtr + 0);
        float32x4_t in_2 = vld1q_f32(inPtr + 4);
        float32x4_t in_3 = vld1q_f32(inPtr + 8);
        float32x4_t in_4 = vld1q_f32(inPtr + 12);
#ifdef USE_16_ACCUMULATORS
        float32x4_t in_5 = vld1q_f32(inPtr + 16);
        float32x4_t in_6 = vld1q_f32(inPtr + 20);
        float32x4_t in_7 = vld1q_f32(inPtr + 24);
        float32x4_t in_8 = vld1q_f32(inPtr + 28);
#endif

        sum_a1 = vmlaq_f32(sum_a1, c_a, in_1);
        sum_a2 = vmlaq_f32(sum_a2, c_a, in_2);
        sum_a3 = vmlaq_f32(sum_a3, c_a, in_3);
        sum_a4 = vmlaq_f32(sum_a4, c_a, in_4);
#ifdef USE_16_ACCUMULATORS
        sum_a5 = vmlaq_f32(sum_a5, c_a, in_5);
        sum_a6 = vmlaq_f32(sum_a6, c_a, in_6);
        sum_a7 = vmlaq_f32(sum_a7, c_a, in_7);
        sum_a8 = vmlaq_f32(sum_a8, c_a, in_8);
#endif

        sum_b1 = vmlaq_f32(sum_b1, c_b, in_1);
        sum_b2 = vmlaq_f32(sum_b2, c_b, in_2);
        sum_b3 = vmlaq_f32(sum_b3, c_b, in_3);
        sum_b4 = vmlaq_f32(sum_b4, c_b, in_4);
#ifdef USE_16_ACCUMULATORS
        sum_b5 = vmlaq_f32(sum_b5, c_b, in_5);
        sum_b6 = vmlaq_f32(sum_b6, c_b, in_6);
        sum_b7 = vmlaq_f32(sum_b7, c_b, in_7);
        sum_b8 = vmlaq_f32(sum_b8, c_b, in_8);
#endif
      }

      vst1q_f32(outPtr_a + 0, sum_a1);
      vst1q_f32(outPtr_a + 4, sum_a2);
      vst1q_f32(outPtr_a + 8, sum_a3);
      vst1q_f32(outPtr_a + 12, sum_a4);
#ifdef USE_16_ACCUMULATORS
      vst1q_f32(outPtr_a + 16, sum_a5);
      vst1q_f32(outPtr_a + 20, sum_a6);
      vst1q_f32(outPtr_a + 24, sum_a7);
      vst1q_f32(outPtr_a + 28, sum_a8);
#endif

      vst1q_f32(outPtr_b + 0, sum_b1);
      vst1q_f32(outPtr_b + 4, sum_b2);
      vst1q_f32(outPtr_b + 8, sum_b3);
      vst1q_f32(outPtr_b + 12, sum_b4);
#ifdef USE_16_ACCUMULATORS
      vst1q_f32(outPtr_b + 16, sum_b5);
      vst1q_f32(outPtr_b + 20, sum_b6);
      vst1q_f32(outPtr_b + 24, sum_b7);
      vst1q_f32(outPtr_b + 28, sum_b8);
#endif
    }

    if (out_dim & 1) {
      out_idx = out_dim - 1;
      float* outPtr = out[out_idx] + i;

      float32x4_t sum_1 = vdupq_n_f32(0.0f);
      float32x4_t sum_2 = vdupq_n_f32(0.0f);
      float32x4_t sum_3 = vdupq_n_f32(0.0f);
      float32x4_t sum_4 = vdupq_n_f32(0.0f);
#ifdef USE_16_ACCUMULATORS
      float32x4_t sum_5 = vdupq_n_f32(0.0f);
      float32x4_t sum_6 = vdupq_n_f32(0.0f);
      float32x4_t sum_7 = vdupq_n_f32(0.0f);
      float32x4_t sum_8 = vdupq_n_f32(0.0f);
#endif

      for (in_idx = 0; in_idx < in_dim; in_idx++) {
        const int in_mapped_idx = in_idx_map ? in_idx_map[in_idx] : in_idx;
        const int c_offset = out_idx * out_next + in_mapped_idx * in_next;
        const float* inPtr = in[in_idx] + i;

        float32x4_t c = vld1q_dup_f32(&mat[c_offset]);

        float32x4_t in_1 = vld1q_f32(inPtr + 0);
        float32x4_t in_2 = vld1q_f32(inPtr + 4);
        float32x4_t in_3 = vld1q_f32(inPtr + 8);
        float32x4_t in_4 = vld1q_f32(inPtr + 12);
#ifdef USE_16_ACCUMULATORS
        float32x4_t in_5 = vld1q_f32(inPtr + 16);
        float32x4_t in_6 = vld1q_f32(inPtr + 20);
        float32x4_t in_7 = vld1q_f32(inPtr + 24);
        float32x4_t in_8 = vld1q_f32(inPtr + 28);
#endif

        sum_1 = vmlaq_f32(sum_1, c, in_1);
        sum_2 = vmlaq_f32(sum_2, c, in_2);
        sum_3 = vmlaq_f32(sum_3, c, in_3);
        sum_4 = vmlaq_f32(sum_4, c, in_4);
#ifdef USE_16_ACCUMULATORS
        sum_5 = vmlaq_f32(sum_5, c, in_5);
        sum_6 = vmlaq_f32(sum_6, c, in_6);
        sum_7 = vmlaq_f32(sum_7, c, in_7);
        sum_8 = vmlaq_f32(sum_8, c, in_8);
#endif
      }
      vst1q_f32(outPtr + 0, sum_1);
      vst1q_f32(outPtr + 4, sum_2);
      vst1q_f32(outPtr + 8, sum_3);
      vst1q_f32(outPtr + 12, sum_4);
#ifdef USE_16_ACCUMULATORS
      vst1q_f32(outPtr + 16, sum_5);
      vst1q_f32(outPtr + 20, sum_6);
      vst1q_f32(outPtr + 24, sum_7);
      vst1q_f32(outPtr + 28, sum_8);
#endif
    }
  }

  for (in_idx = 0; in_idx < in_dim; in_idx++) {
    const int in_mapped_idx = in_idx_map ? in_idx_map[in_idx] : in_idx;

    for (out_idx = 0; out_idx < out_dim; out_idx++) {
      const float c = mat[out_idx * out_next + in_mapped_idx * in_next];

      for (i = blocked_size; i < nsamples; i++) {
        if (in_idx == 0) {
          out[out_idx][i] = 0;
        }
        out[out_idx][i] += c * in[in_idx][i];
      }
    }
  }
}

#endif /* IAMF_ARCH_DETECTED_ARM */
