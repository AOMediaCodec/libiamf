/*
 * Copyright (c) 2025, Alliance for Open Media. All rights reserved
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
 * @file mix_presentation_obu.h
 * @brief Mix presentation OBU APIs.
 * @version 2.0.0
 * @date Created 12/11/2025
 **/

#ifndef __MIX_PRESENTATION_OBU_H__
#define __MIX_PRESENTATION_OBU_H__

#include "cvector.h"
#include "iamf_obu.h"

/**
 * Mix Presentation OBU
 * */

#define def_mix_presentation_obu_ptr(a) ((iamf_mix_presentation_obu_t *)a)

#define def_rendering_config_flag_element_gain_offset (1 << 5)

#define def_loudness_info_type_true_peak 1
#define def_loudness_info_type_anchored 2
#define def_loudness_info_type_live 4
#define def_loudness_info_type_momentary 8
#define def_loudness_info_type_range 0x10

#define def_loudness_info_type_all                                      \
  (def_loudness_info_type_true_peak | def_loudness_info_type_anchored | \
   def_loudness_info_type_live | def_loudness_info_type_momentary |     \
   def_loudness_info_type_range)

typedef enum EPreferredLoudspeakerRenderer {
  ck_loudspeaker_renderer_none = 0,
} preferred_loudspeaker_renderer_t;

typedef enum EPreferredBinauralRenderer {
  ck_binaural_renderer_none = 0,
} preferred_binaural_renderer_t;

typedef struct SubMix obu_sub_mix_t;

typedef struct IamfMixPresentationObu {
  iamf_obu_t obu;

  uint32_t mix_presentation_id;
  array_t *annotations_languages;               // array<string128_t *>
  array_t *localized_presentation_annotations;  // array<string128_t *>

  array_t *sub_mixes;  // array<obu_sub_mix_t *>;

  array_t *mix_presentation_tags;  // array<iamf_tag_t *>

  uint32_t preferred_loudspeaker_renderer;
  uint32_t preferred_binaural_renderer;
} iamf_mix_presentation_obu_t;

typedef struct AnchoredLoudnessInfo {
  uint32_t anchor_element;
  int anchored_loudness;
} obu_anchored_loudness_info_t;

typedef struct MomentaryLoudness {
  momentary_loudness_parameter_base_t momentary_loudness_param;
  uint8_t num_bin_pairs_minus_one;
  uint8_t bin_width_minus_one;
  uint8_t first_bin_center;
  array_t *bin_count;  // array<uint32_t>
} momentary_loudness_t;

typedef struct LoudnessInfo {
  uint32_t info_type;
  int integrated_loudness;
  int digital_peak;
  int true_peak;

  array_t *anchored_loudnesses;  // array<obu_anchored_loudness_info_t>
  momentary_loudness_t momentary_loudness;
  uint32_t loudness_range;
} obu_loudness_info_t;

typedef struct RenderingConfig {
  uint32_t headphones_rendering_mode;
  uint32_t flags;
  uint32_t binaural_filter_profile;
  array_t *parameters;  // array<parameter_base_t>

  uint32_t element_gain_offset_type;  // 0: value, 1: range
  struct {
    int16_t value;
    int16_t min;
    int16_t max;
  } element_gain_offset;
  struct {
    float value;
    float min;
    float max;
  } element_gain_offset_db;

  uint32_t rendering_config_extension_size;
} obu_rendering_config_t;

typedef struct AudioElementConfig {
  uint32_t element_id;
  array_t *localized_element_annotations;  // array<string128_t>
  obu_rendering_config_t rendering_config;
  mix_gain_parameter_base_t element_mix_gain;
} obu_audio_element_config_t;

struct SubMix {
  array_t *audio_element_configs;  // array<obu_audio_element_config_t>

  mix_gain_parameter_base_t output_mix_gain;

  array_t *loudness_layouts;  // array<iamf_layout_t>;
  array_t *loudness;          // array<obu_loudness_info_t>;
};

iamf_mix_presentation_obu_t *iamf_mix_presentation_obu_new(
    io_context_t *ior, iamf_obu_header_t *header);
void iamf_mix_presentation_obu_free(iamf_mix_presentation_obu_t *obu);
int iamf_mix_presentation_obu_check_profile(iamf_mix_presentation_obu_t *obu,
                                            vector_t *audio_element_obus,
                                            iamf_profile_t profile);
#endif  // __MIX_PRESENTATION_OBU_H__
