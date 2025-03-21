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
 * @file arm_zip_channels.c
 * @brief Arm implementation for zipping channels.
 * @version 0.1
 * @date Created 10/24/2024
 **/

#include "arm_zip_channels.h"

#if defined(IAMF_ARCH_DETECTED_ARM)

#include <arm_neon.h>
#include <math.h>

#include "../arch_common.h"

#define MUL_16BIT 32768.0f

#define MUL_24BIT 8388608.f
#define RANGE_MIN_24BIT -8388608
#define RANGE_MAX_24BIT 8388607

#define MUL_32BIT 2147483648.f

static inline int32x4_t vroundf(float32x4_t x)
{
#if defined(__ARM_ARCH) && __ARM_ARCH >= 8
  return vcvtaq_s32_f32(x);
#else
  uint32x4_t sign = vandq_u32(vreinterpretq_u32_f32(x), vdupq_n_u32(0x80000000));
  uint32x4_t bias = vdupq_n_u32(0x3F000000);
  return vcvtq_s32_f32(vaddq_f32(x, vreinterpretq_f32_u32(vorrq_u32(bias, sign))));
#endif
}

static inline int16x4_t cvt_for_int16(float32x4_t vals) {
  return vqmovn_s32(vroundf(vmulq_n_f32(vals, MUL_16BIT)));
}

static inline int16x8x2_t cvt_for_int16_x2(float32x4x2_t vals0,
                                           float32x4x2_t vals1) {
  int16x8x2_t ret;
  ret.val[0] =
      vcombine_s16(cvt_for_int16(vals0.val[0]), cvt_for_int16(vals0.val[1]));
  ret.val[1] =
      vcombine_s16(cvt_for_int16(vals1.val[0]), cvt_for_int16(vals1.val[1]));
  return ret;
}

static inline int16x8x3_t cvt_for_int16_x3(float32x4x2_t vals0,
                                           float32x4x2_t vals1,
                                           float32x4x2_t vals2) {
  int16x8x3_t ret;
  ret.val[0] =
      vcombine_s16(cvt_for_int16(vals0.val[0]), cvt_for_int16(vals0.val[1]));
  ret.val[1] =
      vcombine_s16(cvt_for_int16(vals1.val[0]), cvt_for_int16(vals1.val[1]));
  ret.val[2] =
      vcombine_s16(cvt_for_int16(vals2.val[0]), cvt_for_int16(vals2.val[1]));
  return ret;
}

static inline int32x4_t cvt_clamp_for_int24_s32(int32x4_t lower,
                                                float32x4_t vals,
                                                int32x4_t upper) {
  return vmaxq_s32(
      vminq_s32(vroundf(vmulq_n_f32(vals, MUL_24BIT)), upper), lower);
}

static inline uint8x16_t cvt_clamp_for_int24_u8(int32x4_t lower,
                                                float32x4_t vals,
                                                int32x4_t upper) {
  return vreinterpretq_u8_s32(vmaxq_s32(
      vminq_s32(vroundf(vmulq_n_f32(vals, MUL_24BIT)), upper), lower));
}

static inline void write_consecutive_int24(uint8_t *ptr, uint8x8_t firstPart,
                                           uint8_t secondPart) {
  vst1_u8(ptr, firstPart);
  ptr[8] = secondPart;
}

static inline void write_pair_int24(uint8_t *ptr, int step, uint64_t val) {
  ptr[0] = (uint8_t)((val >> 0) & 0xff);
  ptr[1] = (uint8_t)((val >> 8) & 0xff);
  ptr[2] = (uint8_t)((val >> 16) & 0xff);

  ptr[step + 0] = (uint8_t)((val >> 32) & 0xff);
  ptr[step + 1] = (uint8_t)((val >> 40) & 0xff);
  ptr[step + 2] = (uint8_t)((val >> 48) & 0xff);
}

static inline int32x4_t cvt_for_int32(float32x4_t vals) {
  return vroundf(vmulq_n_f32(vals, MUL_32BIT));
}

static inline int32x4x2_t cvt_for_int32_x2(float32x4_t vals0,
                                           float32x4_t vals1) {
  int32x4x2_t ret;
  ret.val[0] = cvt_for_int32(vals0);
  ret.val[1] = cvt_for_int32(vals1);
  return ret;
}

static inline int32x4x3_t cvt_for_int32_x3(float32x4_t vals0, float32x4_t vals1,
                                           float32x4_t vals2) {
  int32x4x3_t ret;
  ret.val[0] = cvt_for_int32(vals0);
  ret.val[1] = cvt_for_int32(vals1);
  ret.val[2] = cvt_for_int32(vals2);
  return ret;
}

static inline int16x4x4_t transpose_s16_4x4(const int16x4_t a, const int16x4_t b, const int16x4_t c,
                                            const int16x4_t d) {
  int16x8_t aq = vcombine_s16(a, vdup_n_s16(0));
  int16x8_t bq = vcombine_s16(b, vdup_n_s16(0));
  int16x8_t cq = vcombine_s16(c, vdup_n_s16(0));
  int16x8_t dq = vcombine_s16(d, vdup_n_s16(0));

  int16x8_t ac = vzipq_s16(aq, cq).val[0];
  int16x8_t bd = vzipq_s16(bq, dq).val[0];

  int16x8x2_t abcd = vzipq_s16(ac, bd);

  int16x4x4_t ret = {{
      vget_low_s16(abcd.val[0]),
      vget_high_s16(abcd.val[0]),
      vget_low_s16(abcd.val[1]),
      vget_high_s16(abcd.val[1])
  }};
  return ret;
}

static inline int32x4x2_t vtrnq_s64_to_s32(int32x4_t a0, int32x4_t a1) {
  int32x4x2_t b0;
#if defined(__aarch64__)
  b0.val[0] = vreinterpretq_s32_s64(
      vtrn1q_s64(vreinterpretq_s64_s32(a0), vreinterpretq_s64_s32(a1)));
  b0.val[1] = vreinterpretq_s32_s64(
      vtrn2q_s64(vreinterpretq_s64_s32(a0), vreinterpretq_s64_s32(a1)));
#else
  b0.val[0] = vcombine_s32(vget_low_s32(a0), vget_low_s32(a1));
  b0.val[1] = vcombine_s32(vget_high_s32(a0), vget_high_s32(a1));
#endif
  return b0;
}

static inline int32x4x4_t transpose_s32_4x4(int32x4_t a, int32x4_t b, int32x4_t c,
                                            int32x4_t d) {
  const int32x4x2_t trn_ab = vtrnq_s32(a, b);
  const int32x4x2_t trn_cd = vtrnq_s32(c, d);

  const int32x4x2_t r0 = vtrnq_s64_to_s32(trn_ab.val[0], trn_cd.val[0]);
  const int32x4x2_t r1 = vtrnq_s64_to_s32(trn_ab.val[1], trn_cd.val[1]);

  int32x4x4_t ret;
  ret.val[0] = r0.val[0];
  ret.val[1] = r1.val[0];
  ret.val[2] = r0.val[1];
  ret.val[3] = r1.val[1];
  return ret;
}

static int float2int16_zip_1channels(const float *src, int16_t *int16_dst,
                                     int nsamples) {
  const int BLOCK_SIZE = 32;
  const int blocked_size = nsamples / BLOCK_SIZE * BLOCK_SIZE;
  int i;

  for (i = 0; i < blocked_size; i += BLOCK_SIZE) {
    float32x4x2_t in_a_01 = { { vld1q_f32(src + i +  0), vld1q_f32(src + i +  4) } };
    float32x4x2_t in_a_23 = { { vld1q_f32(src + i +  8), vld1q_f32(src + i + 12) } };
    float32x4x2_t in_a_45 = { { vld1q_f32(src + i + 16), vld1q_f32(src + i + 20) } };
    float32x4x2_t in_a_67 = { { vld1q_f32(src + i + 24), vld1q_f32(src + i + 28) } };

    int16x8x2_t out_01 = cvt_for_int16_x2(in_a_01, in_a_23);
    int16x8x2_t out_23 = cvt_for_int16_x2(in_a_45, in_a_67);

    vst1q_s16(int16_dst + i +  0, out_01.val[0]);
    vst1q_s16(int16_dst + i +  8, out_01.val[1]);
    vst1q_s16(int16_dst + i + 16, out_23.val[0]);
    vst1q_s16(int16_dst + i + 24, out_23.val[1]);
  }

  return blocked_size;
}

static int float2int16_zip_2channels(const float *src, int next_channel,
                                     int16_t *int16_dst, int nsamples) {
  const int BLOCK_SIZE = 16;
  const int blocked_size = nsamples / BLOCK_SIZE * BLOCK_SIZE;
  int i;

  for (i = 0; i < blocked_size; i += BLOCK_SIZE) {
    float32x4x2_t in_a_01 = { { vld1q_f32(src + i + 0), vld1q_f32(src + i +  4) } };
    float32x4x2_t in_a_23 = { { vld1q_f32(src + i + 8), vld1q_f32(src + i + 12) } };
    float32x4x2_t in_b_01 = { { vld1q_f32(src + i + next_channel + 0), vld1q_f32(src + i + next_channel +  4) } };
    float32x4x2_t in_b_23 = { { vld1q_f32(src + i + next_channel + 8), vld1q_f32(src + i + next_channel + 12) } };

    int16x8x2_t out_ab_01 = cvt_for_int16_x2(in_a_01, in_b_01);
    int16x8x2_t out_ab_23 = cvt_for_int16_x2(in_a_23, in_b_23);

    vst2q_s16(int16_dst + i * 2 + 0, out_ab_01);
    vst2q_s16(int16_dst + i * 2 + 16, out_ab_23);
  }

  return blocked_size;
}

static int float2int16_zip_3channels(const float *src, int next_channel,
                                     int16_t *int16_dst, int nsamples) {
  const int BLOCK_SIZE = 16;
  const int blocked_size = nsamples / BLOCK_SIZE * BLOCK_SIZE;
  int i;

  for (i = 0; i < blocked_size; i += BLOCK_SIZE) {
    float32x4x2_t in_a_01 = { { vld1q_f32(src + i + 0 * next_channel + 0), vld1q_f32(src + i + 0 * next_channel +  4) } };
    float32x4x2_t in_a_23 = { { vld1q_f32(src + i + 0 * next_channel + 8), vld1q_f32(src + i + 0 * next_channel + 12) } };
    float32x4x2_t in_b_01 = { { vld1q_f32(src + i + 1 * next_channel + 0), vld1q_f32(src + i + 1 * next_channel +  4) } };
    float32x4x2_t in_b_23 = { { vld1q_f32(src + i + 1 * next_channel + 8), vld1q_f32(src + i + 1 * next_channel + 12) } };
    float32x4x2_t in_c_01 = { { vld1q_f32(src + i + 2 * next_channel + 0), vld1q_f32(src + i + 2 * next_channel +  4) } };
    float32x4x2_t in_c_23 = { { vld1q_f32(src + i + 2 * next_channel + 8), vld1q_f32(src + i + 2 * next_channel + 12) } };

    int16x8x3_t out_abc_01 = cvt_for_int16_x3(in_a_01, in_b_01, in_c_01);
    int16x8x3_t out_abc_23 = cvt_for_int16_x3(in_a_23, in_b_23, in_c_23);

    vst3q_s16(int16_dst + i * 3 + 0, out_abc_01);
    vst3q_s16(int16_dst + i * 3 + 24, out_abc_23);
  }

  return blocked_size;
}

static int float2int16_zip_nchannels(const float *src, int next_channel,
                                     int channels, int16_t *int16_dst,
                                     int nsamples) {
  const int BATCH = 4;
  const int BATCHED_BLOCK_SIZE = 8;
  const int SINGLE_BLOCK_SIZE = 16;
  const int bathed_channels = channels / BATCH * BATCH;
  const int blocked_size = nsamples / BATCHED_BLOCK_SIZE * BATCHED_BLOCK_SIZE /
                           SINGLE_BLOCK_SIZE * SINGLE_BLOCK_SIZE;

  int i, c;

  for (c = 0; c < bathed_channels; c += BATCH) {
    for (i = 0; i < blocked_size; i += BATCHED_BLOCK_SIZE) {
      float32x4x2_t in_a_01 = { { vld1q_f32(src + next_channel * (c + 0) + i), vld1q_f32(src + next_channel * (c + 0) + i + 4) } };
      float32x4x2_t in_b_01 = { { vld1q_f32(src + next_channel * (c + 1) + i), vld1q_f32(src + next_channel * (c + 1) + i + 4) } };
      float32x4x2_t in_c_01 = { { vld1q_f32(src + next_channel * (c + 2) + i), vld1q_f32(src + next_channel * (c + 2) + i + 4) } };
      float32x4x2_t in_d_01 = { { vld1q_f32(src + next_channel * (c + 3) + i), vld1q_f32(src + next_channel * (c + 3) + i + 4) } };

      int16x4_t s32_a_0 = cvt_for_int16(in_a_01.val[0]);
      int16x4_t s32_b_0 = cvt_for_int16(in_b_01.val[0]);
      int16x4_t s32_c_0 = cvt_for_int16(in_c_01.val[0]);
      int16x4_t s32_d_0 = cvt_for_int16(in_d_01.val[0]);
      int16x4_t s32_a_1 = cvt_for_int16(in_a_01.val[1]);
      int16x4_t s32_b_1 = cvt_for_int16(in_b_01.val[1]);
      int16x4_t s32_c_1 = cvt_for_int16(in_c_01.val[1]);
      int16x4_t s32_d_1 = cvt_for_int16(in_d_01.val[1]);

      int16x4x4_t transposed_abcd_0 =
          transpose_s16_4x4(s32_a_0, s32_b_0, s32_c_0, s32_d_0);
      int16x4x4_t transposed_abcd_1 =
          transpose_s16_4x4(s32_a_1, s32_b_1, s32_c_1, s32_d_1);

      int16_t *ptr = int16_dst + i * channels + c;
      const int step = channels;
      vst1_s16(ptr + step * 0, transposed_abcd_0.val[0]);
      vst1_s16(ptr + step * 1, transposed_abcd_0.val[1]);
      vst1_s16(ptr + step * 2, transposed_abcd_0.val[2]);
      vst1_s16(ptr + step * 3, transposed_abcd_0.val[3]);
      vst1_s16(ptr + step * 4, transposed_abcd_1.val[0]);
      vst1_s16(ptr + step * 5, transposed_abcd_1.val[1]);
      vst1_s16(ptr + step * 6, transposed_abcd_1.val[2]);
      vst1_s16(ptr + step * 7, transposed_abcd_1.val[3]);
    }
  }

  for (c = bathed_channels; c < channels; ++c) {
    for (i = 0; i < blocked_size; i += SINGLE_BLOCK_SIZE) {
      float32x4x2_t in_a_01 = { { vld1q_f32(src + next_channel * c + i + 0), vld1q_f32(src + next_channel * c + i +  4) } };
      float32x4x2_t in_a_23 = { { vld1q_f32(src + next_channel * c + i + 8), vld1q_f32(src + next_channel * c + i + 12) } };

      uint64_t out_a_0 =
          vget_lane_u64(vreinterpret_u64_s16(cvt_for_int16(in_a_01.val[0])), 0);
      int64_t out_a_1 =
          vget_lane_u64(vreinterpret_u64_s16(cvt_for_int16(in_a_01.val[1])), 0);
      int64_t out_a_2 =
          vget_lane_u64(vreinterpret_u64_s16(cvt_for_int16(in_a_23.val[0])), 0);
      int64_t out_a_3 =
          vget_lane_u64(vreinterpret_u64_s16(cvt_for_int16(in_a_23.val[1])), 0);

      int16_t *ptr = int16_dst + i * channels + c;
      const int step = channels;
      ptr[step * 0] = (int16_t)((out_a_0 >> 0) & 0xffff);
      ptr[step * 1] = (int16_t)((out_a_0 >> 16) & 0xffff);
      ptr[step * 2] = (int16_t)((out_a_0 >> 32) & 0xffff);
      ptr[step * 3] = (int16_t)((out_a_0 >> 48) & 0xffff);
      ptr[step * 4] = (int16_t)((out_a_1 >> 0) & 0xffff);
      ptr[step * 5] = (int16_t)((out_a_1 >> 16) & 0xffff);
      ptr[step * 6] = (int16_t)((out_a_1 >> 32) & 0xffff);
      ptr[step * 7] = (int16_t)((out_a_1 >> 48) & 0xffff);
      ptr[step * 8] = (int16_t)((out_a_2 >> 0) & 0xffff);
      ptr[step * 9] = (int16_t)((out_a_2 >> 16) & 0xffff);
      ptr[step * 10] = (int16_t)((out_a_2 >> 32) & 0xffff);
      ptr[step * 11] = (int16_t)((out_a_2 >> 48) & 0xffff);
      ptr[step * 12] = (int16_t)((out_a_3 >> 0) & 0xffff);
      ptr[step * 13] = (int16_t)((out_a_3 >> 16) & 0xffff);
      ptr[step * 14] = (int16_t)((out_a_3 >> 32) & 0xffff);
      ptr[step * 15] = (int16_t)((out_a_3 >> 48) & 0xffff);
    }
  }

  return blocked_size;
}

static inline uint8x8_t tbl2(uint8x16_t a, uint8x16_t b, uint8x8_t idx) {
#if defined(__aarch64__)
  uint8x16x2_t table = { { a, b } };
  return vqtbl2_u8(table, idx);
#else
  uint8x8x4_t table = { { vget_low_u8(a), vget_high_u8(a), vget_low_u8(b),
                          vget_high_u8(b) } };
  return vtbl4_u8(table, idx);
#endif
}

static inline uint8x16_t tbl2q(uint8x16_t a, uint8x16_t b, uint8x16_t idx) {
#if defined(__aarch64__)
  uint8x16x2_t table = { { a, b } };
  return vqtbl2q_u8(table, idx);
#else
  uint8x8x4_t table = { { vget_low_u8(a), vget_high_u8(a), vget_low_u8(b),
                          vget_high_u8(b) } };
  return vcombine_u8(vtbl4_u8(table, vget_low_u8(idx)),
                     vtbl4_u8(table, vget_high_u8(idx)));
#endif
}

static int float2int24_zip_1channels(const float *src, uint8_t *int24_dst,
                                     int nsamples) {
  const int BLOCK_SIZE = 8;
  const int blocked_size = nsamples / BLOCK_SIZE * BLOCK_SIZE;

  int i;

  static uint8_t MAP01[] = {0,  1,  2,  4,  5,  6,  8,  9,
                            10, 12, 13, 14, 16, 17, 18, 20};
  static uint8_t MAP2[] = {21, 22, 24, 25, 26, 28, 29, 30,
                           0,  0,  0,  0,  0,  0,  0,  0};
  uint8x16_t map01 = vld1q_u8(MAP01);
  uint8x16_t map2 = vld1q_u8(MAP2);
  int32x4_t min24 = vdupq_n_s32(RANGE_MIN_24BIT);
  int32x4_t max24 = vdupq_n_s32(RANGE_MAX_24BIT);

  for (i = 0; i < blocked_size; i += BLOCK_SIZE) {
    float32x4x2_t in_a_01 = { { vld1q_f32(src + i), vld1q_f32(src + i + 4) } };

    uint8x16x2_t u8_a_01;
    u8_a_01.val[0] = cvt_clamp_for_int24_u8(min24, in_a_01.val[0], max24);
    u8_a_01.val[1] = cvt_clamp_for_int24_u8(min24, in_a_01.val[1], max24);

    uint8x16_t out_01 = tbl2q(u8_a_01.val[0], u8_a_01.val[1], map01);
    uint8x8_t out_2 = vget_low_u8(tbl2q(u8_a_01.val[0], u8_a_01.val[1], map2));

    vst1q_u8(int24_dst + i * 3 + 0, out_01);
    vst1_u8(int24_dst + i * 3 + 16, out_2);
  }

  return blocked_size;
}

static int float2int24_zip_2channels(const float *src, int next_channel,
                                     uint8_t *int24_dst, int nsamples) {
  const int BLOCK_SIZE = 4;
  const int blocked_size = nsamples / BLOCK_SIZE * BLOCK_SIZE;

  int i;

  static uint8_t MAP01[] = {0, 1,  2,  16, 17, 18, 4,  5,
                            6, 20, 21, 22, 8,  9,  10, 24};
  static uint8_t MAP2[] = {25, 26, 12, 13, 14, 28, 29, 30,
                           0,  0,  0,  0,  0,  0,  0,  0};
  uint8x16_t map01 = vld1q_u8(MAP01);
  uint8x16_t map2 = vld1q_u8(MAP2);
  int32x4_t min24 = vdupq_n_s32(RANGE_MIN_24BIT);
  int32x4_t max24 = vdupq_n_s32(RANGE_MAX_24BIT);

  for (i = 0; i < blocked_size; i += BLOCK_SIZE) {
    float32x4_t in_a_0 = vld1q_f32(src + i);
    float32x4_t in_b_0 = vld1q_f32(src + next_channel + i);

    uint8x16x2_t u8_ab_0;
    u8_ab_0.val[0] = cvt_clamp_for_int24_u8(min24, in_a_0, max24);
    u8_ab_0.val[1] = cvt_clamp_for_int24_u8(min24, in_b_0, max24);

    uint8x16_t out_firstpart_ab_0 = tbl2q(u8_ab_0.val[0], u8_ab_0.val[1], map01);
    uint8x8_t out_secondpart_ab_0 = vget_low_u8(tbl2q(u8_ab_0.val[0], u8_ab_0.val[1], map2));

    vst1q_u8(int24_dst + (i * 2 + 0) * 3 + 0 + 0, out_firstpart_ab_0);
    vst1_u8(int24_dst + (i * 2 + 0) * 3 + 15 + 1, out_secondpart_ab_0);
  }

  return blocked_size;
}

static inline uint8x16_t tbl1q(uint8x16_t a, uint8x16_t idx) {
#if defined(__aarch64__)
  return vqtbl1q_u8(a, idx);
#else
  uint8x8x2_t table = { { vget_low_u8(a), vget_high_u8(a) } };
  uint8x8_t lo = vtbl2_u8(table, vget_low_u8(idx));
  uint8x8_t hi = vtbl2_u8(table, vget_high_u8(idx));
  return vcombine_u8(lo, hi);
#endif
}

static int float2int24_zip_nchannels(const float *src, int next_channel,
                                     int channels, uint8_t *int24_dst,
                                     int nsamples) {
  const int BATCH = 3;
  const int BATCHED_BLOCK_SIZE = 8;
  const int SINGLE_BLOCK_SIZE = 8;
  const int bathed_channels = channels / BATCH * BATCH;
  const int blocked_size = nsamples / BATCHED_BLOCK_SIZE * BATCHED_BLOCK_SIZE /
                           SINGLE_BLOCK_SIZE * SINGLE_BLOCK_SIZE;

  int i, c;

  static uint8_t MAP01[] = {0,  1,  2,  4,  5,  6,  8,  9,
                            10, 12, 13, 14, 16, 17, 18, 20};
  uint8x16_t map01 = vld1q_u8(MAP01);
  int32x4_t min24 = vdupq_n_s32(RANGE_MIN_24BIT);
  int32x4_t max24 = vdupq_n_s32(RANGE_MAX_24BIT);

  for (c = 0; c < bathed_channels; c += BATCH) {
    for (i = 0; i < blocked_size; i += BATCHED_BLOCK_SIZE) {
      float32x4x2_t in_a_01 = { { vld1q_f32(src + next_channel * (c + 0) + i), vld1q_f32(src + next_channel * (c + 0) + i + 4) } };
      float32x4x2_t in_b_01 = { { vld1q_f32(src + next_channel * (c + 1) + i), vld1q_f32(src + next_channel * (c + 1) + i + 4) } };
      float32x4x2_t in_c_01 = { { vld1q_f32(src + next_channel * (c + 2) + i), vld1q_f32(src + next_channel * (c + 2) + i + 4) } };

      int32x4_t s32_a_0 = cvt_clamp_for_int24_s32(min24, in_a_01.val[0], max24);
      int32x4_t s32_b_0 = cvt_clamp_for_int24_s32(min24, in_b_01.val[0], max24);
      int32x4_t s32_c_0 = cvt_clamp_for_int24_s32(min24, in_c_01.val[0], max24);

      int32x4_t s32_a_1 = cvt_clamp_for_int24_s32(min24, in_a_01.val[1], max24);
      int32x4_t s32_b_1 = cvt_clamp_for_int24_s32(min24, in_b_01.val[1], max24);
      int32x4_t s32_c_1 = cvt_clamp_for_int24_s32(min24, in_c_01.val[1], max24);
      int32x4_t zeros = vdupq_n_s32(0);

      int32x4x4_t transposed_abcd_0 =
          transpose_s32_4x4(s32_a_0, s32_b_0, s32_c_0, zeros);
      uint8x16_t out_full_0 = vreinterpretq_u8_s32(transposed_abcd_0.val[0]);
      uint8x16_t out_full_1 = vreinterpretq_u8_s32(transposed_abcd_0.val[1]);
      uint8x16_t out_full_2 = vreinterpretq_u8_s32(transposed_abcd_0.val[2]);
      uint8x16_t out_full_3 = vreinterpretq_u8_s32(transposed_abcd_0.val[3]);

      int32x4x4_t transposed_abcd_1 =
          transpose_s32_4x4(s32_a_1, s32_b_1, s32_c_1, zeros);
      uint8x16_t out_full_4 = vreinterpretq_u8_s32(transposed_abcd_1.val[0]);
      uint8x16_t out_full_5 = vreinterpretq_u8_s32(transposed_abcd_1.val[1]);
      uint8x16_t out_full_6 = vreinterpretq_u8_s32(transposed_abcd_1.val[2]);
      uint8x16_t out_full_7 = vreinterpretq_u8_s32(transposed_abcd_1.val[3]);

      const uint8_t out_secondpart_0 = vgetq_lane_u8(out_full_0, 10);
      const uint8_t out_secondpart_1 = vgetq_lane_u8(out_full_1, 10);
      const uint8_t out_secondpart_2 = vgetq_lane_u8(out_full_2, 10);
      const uint8_t out_secondpart_3 = vgetq_lane_u8(out_full_3, 10);
      const uint8_t out_secondpart_4 = vgetq_lane_u8(out_full_4, 10);
      const uint8_t out_secondpart_5 = vgetq_lane_u8(out_full_5, 10);
      const uint8_t out_secondpart_6 = vgetq_lane_u8(out_full_6, 10);
      const uint8_t out_secondpart_7 = vgetq_lane_u8(out_full_7, 10);

      uint8x8_t out_firstpart_0 = vget_low_u8(tbl1q(out_full_0, map01));
      uint8x8_t out_firstpart_1 = vget_low_u8(tbl1q(out_full_1, map01));
      uint8x8_t out_firstpart_2 = vget_low_u8(tbl1q(out_full_2, map01));
      uint8x8_t out_firstpart_3 = vget_low_u8(tbl1q(out_full_3, map01));
      uint8x8_t out_firstpart_4 = vget_low_u8(tbl1q(out_full_4, map01));
      uint8x8_t out_firstpart_5 = vget_low_u8(tbl1q(out_full_5, map01));
      uint8x8_t out_firstpart_6 = vget_low_u8(tbl1q(out_full_6, map01));
      uint8x8_t out_firstpart_7 = vget_low_u8(tbl1q(out_full_7, map01));

      uint8_t *ptr = int24_dst + ((i + 0) * channels + c) * 3;
      const int step = channels * 3;
      write_consecutive_int24(ptr + step * 0, out_firstpart_0,
                              out_secondpart_0);
      write_consecutive_int24(ptr + step * 1, out_firstpart_1,
                              out_secondpart_1);
      write_consecutive_int24(ptr + step * 2, out_firstpart_2,
                              out_secondpart_2);
      write_consecutive_int24(ptr + step * 3, out_firstpart_3,
                              out_secondpart_3);
      write_consecutive_int24(ptr + step * 4, out_firstpart_4,
                              out_secondpart_4);
      write_consecutive_int24(ptr + step * 5, out_firstpart_5,
                              out_secondpart_5);
      write_consecutive_int24(ptr + step * 6, out_firstpart_6,
                              out_secondpart_6);
      write_consecutive_int24(ptr + step * 7, out_firstpart_7,
                              out_secondpart_7);
    }
  }

  for (c = bathed_channels; c < channels; ++c) {
    for (i = 0; i < blocked_size; i += SINGLE_BLOCK_SIZE) {
      float32x4x2_t in_a_01 = { { vld1q_f32(src + next_channel * c + i), vld1q_f32(src + next_channel * c + i + 4) } };

      uint8x16_t out_full_0 = vreinterpretq_u8_s32(
          cvt_clamp_for_int24_s32(min24, in_a_01.val[0], max24));
      uint8x16_t out_full_1 = vreinterpretq_u8_s32(
          cvt_clamp_for_int24_s32(min24, in_a_01.val[1], max24));

      uint64_t out_low_0 = vgetq_lane_u64(vreinterpretq_u64_u8(out_full_0), 0);
      uint64_t out_high_0 = vgetq_lane_u64(vreinterpretq_u64_u8(out_full_0), 1);
      uint64_t out_low_1 = vgetq_lane_u64(vreinterpretq_u64_u8(out_full_1), 0);
      uint64_t out_high_1 = vgetq_lane_u64(vreinterpretq_u64_u8(out_full_1), 1);

      uint8_t *ptr = int24_dst + ((i + 0) * channels + c) * 3;
      const int step = channels * 3;
      write_pair_int24(ptr + step * 0, step, out_low_0);
      write_pair_int24(ptr + step * 2, step, out_high_0);
      write_pair_int24(ptr + step * 4, step, out_low_1);
      write_pair_int24(ptr + step * 6, step, out_high_1);
    }
  }

  return blocked_size;
}

static int float2int32_zip_1channels(const float *src, int32_t *int32_dst,
                                     int nsamples) {
  const int BLOCK_SIZE = 32;
  const int blocked_size = nsamples / BLOCK_SIZE * BLOCK_SIZE;

  int i;

  for (i = 0; i < blocked_size; i += BLOCK_SIZE) {
    float32x4x2_t in_a_01 = { { vld1q_f32(src + i +  0), vld1q_f32(src + i +  4) } };
    float32x4x2_t in_a_23 = { { vld1q_f32(src + i +  8), vld1q_f32(src + i + 12) } };
    float32x4x2_t in_a_45 = { { vld1q_f32(src + i + 16), vld1q_f32(src + i + 20) } };
    float32x4x2_t in_a_67 = { { vld1q_f32(src + i + 24), vld1q_f32(src + i + 28) } };

    int32x4x2_t out_01 = cvt_for_int32_x2(in_a_01.val[0], in_a_01.val[1]);
    int32x4x2_t out_23 = cvt_for_int32_x2(in_a_23.val[0], in_a_23.val[1]);
    int32x4x2_t out_45 = cvt_for_int32_x2(in_a_45.val[0], in_a_45.val[1]);
    int32x4x2_t out_67 = cvt_for_int32_x2(in_a_67.val[0], in_a_67.val[1]);

    vst1q_s32(int32_dst + i +  0, out_01.val[0]);
    vst1q_s32(int32_dst + i +  4, out_01.val[1]);
    vst1q_s32(int32_dst + i +  8, out_23.val[0]);
    vst1q_s32(int32_dst + i + 12, out_23.val[1]);
    vst1q_s32(int32_dst + i + 16, out_45.val[0]);
    vst1q_s32(int32_dst + i + 20, out_45.val[1]);
    vst1q_s32(int32_dst + i + 24, out_67.val[0]);
    vst1q_s32(int32_dst + i + 28, out_67.val[1]);
  }

  return blocked_size;
}

static int float2int32_zip_2channels(const float *src, int next_channel,
                                     int32_t *int32_dst, int nsamples) {
  const int BLOCK_SIZE = 16;
  const int blocked_size = nsamples / BLOCK_SIZE * BLOCK_SIZE;

  int i;

  for (i = 0; i < blocked_size; i += BLOCK_SIZE) {
    float32x4x2_t in_a_01 = { { vld1q_f32(src + i + 0), vld1q_f32(src + i +  4) } };
    float32x4x2_t in_a_23 = { { vld1q_f32(src + i + 8), vld1q_f32(src + i + 12) } };
    float32x4x2_t in_b_01 = { { vld1q_f32(src + next_channel + i + 0), vld1q_f32(src + next_channel + i +  4) } };
    float32x4x2_t in_b_23 = { { vld1q_f32(src + next_channel + i + 8), vld1q_f32(src + next_channel + i + 12) } };

    int32x4x2_t out_ab_0 = cvt_for_int32_x2(in_a_01.val[0], in_b_01.val[0]);
    int32x4x2_t out_ab_1 = cvt_for_int32_x2(in_a_01.val[1], in_b_01.val[1]);
    int32x4x2_t out_ab_2 = cvt_for_int32_x2(in_a_23.val[0], in_b_23.val[0]);
    int32x4x2_t out_ab_3 = cvt_for_int32_x2(in_a_23.val[1], in_b_23.val[1]);

    vst2q_s32(int32_dst + i * 2 + 0, out_ab_0);
    vst2q_s32(int32_dst + i * 2 + 8, out_ab_1);
    vst2q_s32(int32_dst + i * 2 + 16, out_ab_2);
    vst2q_s32(int32_dst + i * 2 + 24, out_ab_3);
  }

  return blocked_size;
}

static int float2int32_zip_3channels(const float *src, int next_channel,
                                     int32_t *int32_dst, int nsamples) {
  const int BLOCK_SIZE = 16;
  const int blocked_size = nsamples / BLOCK_SIZE * BLOCK_SIZE;

  int i;

  for (i = 0; i < blocked_size; i += BLOCK_SIZE) {
    float32x4x2_t in_a_01 = { { vld1q_f32(src + i + 0), vld1q_f32(src + i +  4) } };
    float32x4x2_t in_a_23 = { { vld1q_f32(src + i + 8), vld1q_f32(src + i + 12) } };
    float32x4x2_t in_b_01 = { { vld1q_f32(src + next_channel + i + 0), vld1q_f32(src + next_channel + i +  4) } };
    float32x4x2_t in_b_23 = { { vld1q_f32(src + next_channel + i + 8), vld1q_f32(src + next_channel + i + 12) } };
    float32x4x2_t in_c_01 = { { vld1q_f32(src + next_channel * 2 + i + 0), vld1q_f32(src + next_channel * 2 + i +  4) } };
    float32x4x2_t in_c_23 = { { vld1q_f32(src + next_channel * 2 + i + 8), vld1q_f32(src + next_channel * 2 + i + 12) } };

    int32x4x3_t out_abc_0 =
        cvt_for_int32_x3(in_a_01.val[0], in_b_01.val[0], in_c_01.val[0]);
    int32x4x3_t out_abc_1 =
        cvt_for_int32_x3(in_a_01.val[1], in_b_01.val[1], in_c_01.val[1]);
    int32x4x3_t out_abc_2 =
        cvt_for_int32_x3(in_a_23.val[0], in_b_23.val[0], in_c_23.val[0]);
    int32x4x3_t out_abc_3 =
        cvt_for_int32_x3(in_a_23.val[1], in_b_23.val[1], in_c_23.val[1]);

    vst3q_s32(int32_dst + i * 3 + 0, out_abc_0);
    vst3q_s32(int32_dst + i * 3 + 12, out_abc_1);
    vst3q_s32(int32_dst + i * 3 + 24, out_abc_2);
    vst3q_s32(int32_dst + i * 3 + 36, out_abc_3);
  }

  return blocked_size;
}

static int float2int32_zip_nchannels(const float *src, int next_channel,
                                     int channels, int32_t *int32_dst,
                                     int nsamples) {
  const int BATCH = 4;
  const int BATCHED_BLOCK_SIZE = 8;
  const int SINGLE_BLOCK_SIZE = 16;
  const int bathed_channels = channels / BATCH * BATCH;
  const int blocked_size = nsamples / BATCHED_BLOCK_SIZE * BATCHED_BLOCK_SIZE /
                           SINGLE_BLOCK_SIZE * SINGLE_BLOCK_SIZE;

  int i, c;

  for (c = 0; c < bathed_channels; c += BATCH) {
    for (i = 0; i < blocked_size; i += BATCHED_BLOCK_SIZE) {
      float32x4x2_t in_a_01 = { { vld1q_f32(src + next_channel * (c + 0) + i), vld1q_f32(src + next_channel * (c + 0) + i + 4) } };
      float32x4x2_t in_b_01 = { { vld1q_f32(src + next_channel * (c + 1) + i), vld1q_f32(src + next_channel * (c + 1) + i + 4) } };
      float32x4x2_t in_c_01 = { { vld1q_f32(src + next_channel * (c + 2) + i), vld1q_f32(src + next_channel * (c + 2) + i + 4) } };
      float32x4x2_t in_d_01 = { { vld1q_f32(src + next_channel * (c + 3) + i), vld1q_f32(src + next_channel * (c + 3) + i + 4) } };

      int32x4x2_t s32_a_01 = cvt_for_int32_x2(in_a_01.val[0], in_a_01.val[1]);
      int32x4x2_t s32_b_01 = cvt_for_int32_x2(in_b_01.val[0], in_b_01.val[1]);
      int32x4x2_t s32_c_01 = cvt_for_int32_x2(in_c_01.val[0], in_c_01.val[1]);
      int32x4x2_t s32_d_01 = cvt_for_int32_x2(in_d_01.val[0], in_d_01.val[1]);

      int32x4x4_t transposed_0 = transpose_s32_4x4(
          s32_a_01.val[0], s32_b_01.val[0], s32_c_01.val[0], s32_d_01.val[0]);
      int32x4x4_t transposed_1 = transpose_s32_4x4(
          s32_a_01.val[1], s32_b_01.val[1], s32_c_01.val[1], s32_d_01.val[1]);

      int32_t *ptr = int32_dst + i * channels + c;
      const int step = channels;
      vst1q_s32(ptr + step * 0, transposed_0.val[0]);
      vst1q_s32(ptr + step * 1, transposed_0.val[1]);
      vst1q_s32(ptr + step * 2, transposed_0.val[2]);
      vst1q_s32(ptr + step * 3, transposed_0.val[3]);
      vst1q_s32(ptr + step * 4, transposed_1.val[0]);
      vst1q_s32(ptr + step * 5, transposed_1.val[1]);
      vst1q_s32(ptr + step * 6, transposed_1.val[2]);
      vst1q_s32(ptr + step * 7, transposed_1.val[3]);
    }
  }

  for (c = bathed_channels; c < channels; ++c) {
    for (i = 0; i < blocked_size; i += SINGLE_BLOCK_SIZE) {
      float32x4x2_t in_a_01 = { { vld1q_f32(src + next_channel * c + i + 0), vld1q_f32(src + next_channel * c + i +  4) } };
      float32x4x2_t in_a_23 = { { vld1q_f32(src + next_channel * c + i + 8), vld1q_f32(src + next_channel * c + i + 12) } };

      int32x4_t out_scattered_a_0 = cvt_for_int32(in_a_01.val[0]);
      int32x4_t out_scattered_a_1 = cvt_for_int32(in_a_01.val[1]);
      int32x4_t out_scattered_a_2 = cvt_for_int32(in_a_23.val[0]);
      int32x4_t out_scattered_a_3 = cvt_for_int32(in_a_23.val[1]);

      int64_t out_low_a_0 =
          vgetq_lane_u64(vreinterpretq_u64_s32(out_scattered_a_0), 0);
      int64_t out_high_a_0 =
          vgetq_lane_u64(vreinterpretq_u64_s32(out_scattered_a_0), 1);
      int64_t out_low_a_1 =
          vgetq_lane_u64(vreinterpretq_u64_s32(out_scattered_a_1), 0);
      int64_t out_high_a_1 =
          vgetq_lane_u64(vreinterpretq_u64_s32(out_scattered_a_1), 1);
      int64_t out_low_a_2 =
          vgetq_lane_u64(vreinterpretq_u64_s32(out_scattered_a_2), 0);
      int64_t out_high_a_2 =
          vgetq_lane_u64(vreinterpretq_u64_s32(out_scattered_a_2), 1);
      int64_t out_low_a_3 =
          vgetq_lane_u64(vreinterpretq_u64_s32(out_scattered_a_3), 0);
      int64_t out_high_a_3 =
          vgetq_lane_u64(vreinterpretq_u64_s32(out_scattered_a_3), 1);

      int32_t *ptr = int32_dst + i * channels + c;
      const int step = channels;
      ptr[step * 0] = (int32_t)((out_low_a_0 >> 0) & 0xffffffff);
      ptr[step * 1] = (int32_t)((out_low_a_0 >> 32) & 0xffffffff);
      ptr[step * 2] = (int32_t)((out_high_a_0 >> 0) & 0xffffffff);
      ptr[step * 3] = (int32_t)((out_high_a_0 >> 32) & 0xffffffff);
      ptr[step * 4] = (int32_t)((out_low_a_1 >> 0) & 0xffffffff);
      ptr[step * 5] = (int32_t)((out_low_a_1 >> 32) & 0xffffffff);
      ptr[step * 6] = (int32_t)((out_high_a_1 >> 0) & 0xffffffff);
      ptr[step * 7] = (int32_t)((out_high_a_1 >> 32) & 0xffffffff);
      ptr[step * 8] = (int32_t)((out_low_a_2 >> 0) & 0xffffffff);
      ptr[step * 9] = (int32_t)((out_low_a_2 >> 32) & 0xffffffff);
      ptr[step * 10] = (int32_t)((out_high_a_2 >> 0) & 0xffffffff);
      ptr[step * 11] = (int32_t)((out_high_a_2 >> 32) & 0xffffffff);
      ptr[step * 12] = (int32_t)((out_low_a_3 >> 0) & 0xffffffff);
      ptr[step * 13] = (int32_t)((out_low_a_3 >> 32) & 0xffffffff);
      ptr[step * 14] = (int32_t)((out_high_a_3 >> 0) & 0xffffffff);
      ptr[step * 15] = (int32_t)((out_high_a_3 >> 32) & 0xffffffff);
    }
  }

  return blocked_size;
}

void float2int16_zip_channels_neon(const float *src, int next_channel,
                                   int channels, int16_t *int16_dst,
                                   int nsamples) {
  int processed = 0;

  switch (channels) {
    case 1:
      processed = float2int16_zip_1channels(src, int16_dst, nsamples);
      break;
    case 2:
      processed =
          float2int16_zip_2channels(src, next_channel, int16_dst, nsamples);
      break;
    case 3:
      processed =
          float2int16_zip_3channels(src, next_channel, int16_dst, nsamples);
      break;
    default:
      processed = float2int16_zip_nchannels(src, next_channel, channels,
                                            int16_dst, nsamples);
      break;
  }

  // Let C version handle the residuals
  float2int16_zip_channels_c(src + processed, next_channel, channels,
                             int16_dst + processed * channels,
                             nsamples - processed);
}

void float2int24_zip_channels_neon(const float *src, int next_channel,
                                   int channels, uint8_t *int24_dst,
                                   int nsamples) {
  int processed = 0;

  switch (channels) {
    case 1:
      processed = float2int24_zip_1channels(src, int24_dst, nsamples);
      break;
    case 2:
      processed =
          float2int24_zip_2channels(src, next_channel, int24_dst, nsamples);
      break;
    default:
      processed = float2int24_zip_nchannels(src, next_channel, channels,
                                            int24_dst, nsamples);
      break;
  }

  // Let C version handle the residuals
  float2int24_zip_channels_c(src + processed, next_channel, channels,
                             int24_dst + processed * channels * 3,
                             nsamples - processed);
}

void float2int32_zip_channels_neon(const float *src, int next_channel,
                                   int channels, int32_t *int32_dst,
                                   int nsamples) {
  int processed = 0;

  switch (channels) {
    case 1:
      processed = float2int32_zip_1channels(src, int32_dst, nsamples);
      break;
    case 2:
      processed =
          float2int32_zip_2channels(src, next_channel, int32_dst, nsamples);
      break;
    case 3:
      processed =
          float2int32_zip_3channels(src, next_channel, int32_dst, nsamples);
      break;
    default:
      processed = float2int32_zip_nchannels(src, next_channel, channels,
                                            int32_dst, nsamples);
      break;
  }

  // Let C version handle the residuals
  float2int32_zip_channels_c(src + processed, next_channel, channels,
                             int32_dst + processed * channels,
                             nsamples - processed);
}

#endif /* IAMF_ARCH_DETECTED_ARM */
