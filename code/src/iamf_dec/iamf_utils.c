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
 * @file IAMF_utils.c
 * @brief Utils.
 * @version 0.1
 * @date Created 03/03/2023
 **/

#include "iamf_utils.h"

#include <math.h>

#include "iamf_private_definitions.h"

iamf_codec_type_t iamf_codec_type_get(iamf_codec_id_t codec_id) {
  if (codec_id == ck_iamf_codec_id_aac) return ck_iamf_codec_type_aac;
  if (codec_id == ck_iamf_codec_id_opus) return ck_iamf_codec_type_opus;
  if (codec_id == ck_iamf_codec_id_flac) return ck_iamf_codec_type_flac;
  if (codec_id == ck_iamf_codec_id_lpcm) return ck_iamf_codec_type_lpcm;
  return ck_iamf_codec_type_none;
}

int iamf_codec_type_check(iamf_codec_type_t type) {
  return type > ck_iamf_codec_type_none && type <= ck_iamf_codec_type_lpcm;
}

int iamf_codec_id_check(iamf_codec_id_t codec_id) {
  return codec_id == ck_iamf_codec_id_opus ||
         codec_id == ck_iamf_codec_id_aac ||
         codec_id == ck_iamf_codec_id_flac || codec_id == ck_iamf_codec_id_lpcm;
}

int iamf_audio_layer_base_layout_check(iamf_loudspeaker_layout_t type) {
  return type >= ck_iamf_loudspeaker_layout_mono &&
         type < ck_iamf_loudspeaker_layout_count;
}

int iamf_audio_layer_expanded_layout_check(
    iamf_expanded_loudspeaker_layout_t type) {
  return type >= ck_iamf_expanded_loudspeaker_layout_lfe &&
         type < ck_iamf_expanded_loudspeaker_layout_count;
}

iamf_loudspeaker_layout_t iamf_audio_layer_layout_get(
    iamf_loudspeaker_layout_t base,
    iamf_expanded_loudspeaker_layout_t expanded) {
  if (iamf_audio_layer_base_layout_check(base)) return base;
  if (iamf_audio_layer_expanded_layout_check(expanded) &&
      base == def_iamf_loudspeaker_layout_expanded)
    return def_iamf_loudspeaker_layout_expanded_mask + expanded;
  return ck_iamf_loudspeaker_layout_none;
}

int bit1_count(uint32_t value) {
  int n = 0;
  for (; value; ++n) {
    value &= (value - 1);
  }
  return n;
}

int iamf_valid_mix_mode(int mode) { return mode >= 0 && mode != 3 && mode < 7; }

const mix_factors_t mix_factors_mat[] = {
    {1.0, 1.0, 0.707, 0.707, -1},   {0.707, 0.707, 0.707, 0.707, -1},
    {1.0, 0.866, 0.866, 0.866, -1}, {0, 0, 0, 0, 0},
    {1.0, 1.0, 0.707, 0.707, 1},    {0.707, 0.707, 0.707, 0.707, 1},
    {1.0, 0.866, 0.866, 0.866, 1},  {0, 0, 0, 0, 0}};

const mix_factors_t *iamf_get_mix_factors(int mode) {
  if (iamf_valid_mix_mode(mode)) return &mix_factors_mat[mode];
  return 0;
}

static float q16_1xy_float(int16_t q, int frac) {
  return ((float)q) * powf(2.0f, (float)-frac);
}

float iamf_q15_to_float(int16_t q) { return q16_1xy_float(q, 15); }
float iamf_gain_q78_to_db(int16_t q78) { return q16_1xy_float(q78, 8); }
float iamf_gain_q78_to_linear(int16_t q78) {
  return f32_db_to_linear(q16_1xy_float(q78, 8));
}

static float iamf_divide_255f(uint8_t val) { return ((float)val / 255.0f); }
float iamf_recon_gain_linear(int8_t gain) { return iamf_divide_255f(gain); }
float iamf_divide_128f(uint8_t val) { return ((float)val / (powf(2.0f, 8.f))); }

float f32_db_to_linear(float db) { return powf(10.0f, 0.05f * db); }

int iamf_ambisionisc_get_order(uint32_t channels) {
  if (channels == 1) return 0;
  if (channels == 4) return 1;
  if (channels == 9) return 2;
  if (channels == 16) return 3;
  if (channels == 25) return 4;
  return -1;
}

int iamf_sound_system_check(iamf_sound_system_t ss) {
  return ss > SOUND_SYSTEM_NONE && ss < SOUND_SYSTEM_END;
}

uint32_t iamf_fraction_transform(fraction_t f, uint32_t dem) {
  uint64_t num = 0;
  if (!dem || !f.denominator) return 0;
  if (f.denominator == dem) return f.numerator;
  num = ((uint64_t)f.numerator * dem + f.denominator - 1) / f.denominator;
  return (uint32_t)num;
}

float iamf_fraction_transform_float(fraction_t f, uint32_t dem) {
  if (!dem || !f.denominator) return 0.0f;
  if (f.denominator == dem) return (float)f.numerator;
  return ((float)f.numerator * dem) / (float)f.denominator;
}

int16_t iamf_u32_to_i16(uint32_t v, uint32_t num_bits) {
  int16_t val = (int16_t)v;
  if (num_bits >= 16) return val;
  val <<= (16 - num_bits);
  val >>= (16 - num_bits);
  return val;
}

float iamf_u32_to_f32(uint32_t v, uint32_t bits) {
  float val = (float)v;
  if (bits) val /= ((1 << bits) - 1);
  return val;
}

float iamf_i16_to_f32(int16_t v, uint32_t bits) {
  float val = (float)v;
  if (bits) val /= ((1 << (bits - 1)) - 1);
  return val;
}
