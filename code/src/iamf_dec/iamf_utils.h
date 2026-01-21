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
 * @file iamf_utils.h
 * @brief Utils APIs.
 * @version 0.1
 * @date Created 03/03/2023
 **/

#ifndef __IAMF_UITLS_H__
#define __IAMF_UITLS_H__

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "IAMF_defines.h"
#include "cvalue.h"
#include "iamf_types.h"
#include "oar_base.h"

iamf_codec_type_t iamf_codec_type_get(uint32_t codec_id);
int iamf_codec_type_check(iamf_codec_type_t cid);
int iamf_codec_id_check(iamf_codec_id_t cid);

int iamf_audio_layer_base_layout_check(iamf_loudspeaker_layout_t type);
int iamf_audio_layer_expanded_layout_check(
    iamf_expanded_loudspeaker_layout_t type);
iamf_expanded_loudspeaker_layout_t iamf_audio_layer_expanded_layout_get(
    iamf_loudspeaker_layout_t type);
iamf_loudspeaker_layout_t iamf_audio_layer_layout_get(
    iamf_loudspeaker_layout_t base,
    iamf_expanded_loudspeaker_layout_t expanded);

int bit1_count(uint32_t value);

int iamf_valid_mix_mode(int mode);
const mix_factors_t *iamf_get_mix_factors(int mode);

float iamf_q15_to_float(int16_t q);
float iamf_gain_q78_to_db(int16_t q78);
float iamf_gain_q78_to_linear(int16_t q78);
float iamf_recon_gain_linear(int8_t gain);
float iamf_divide_128f(uint8_t val);
float f32_db_to_linear(float db);

int iamf_ambisionisc_get_order(uint32_t channels);
int iamf_sound_system_check(iamf_sound_system_t ss);

// int iamf_layout_is_equal(iamf_layout_t a, iamf_layout_t b);
uint32_t iamf_fraction_transform(fraction_t f, uint32_t dem);
float iamf_fraction_transform_float(fraction_t f, uint32_t dem);

int16_t iamf_u32_to_i16(uint32_t v, uint32_t bits);
float iamf_u32_to_f32(uint32_t v, uint32_t bits);
float iamf_i16_to_f32(int16_t v, uint32_t bits);

#endif /* __IAMF_UITLS_H__ */
