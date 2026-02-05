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
 * @file audio_element_obu.h
 * @brief Audio element OBU APIs.
 * @version 2.0.0
 * @date Created 12/11/2025
 **/

#ifndef __AUDIO_ELEMENT_OBU_H__
#define __AUDIO_ELEMENT_OBU_H__

#include "carray.h"
#include "iamf_obu.h"

/**
 * Audio Element OBU.
 * */

#define def_audio_element_obu_ptr(a) ((iamf_audio_element_obu_t *)a)
#define def_channel_based_audio_element_obu_ptr(a) \
  ((channel_based_audio_element_obu_t *)a)
#define def_scene_based_audio_element_obu_ptr(a) \
  ((scene_based_audio_element_obu_t *)a)
#define def_object_based_audio_element_obu_ptr(a) \
  ((object_based_audio_element_obu_t *)a)

typedef enum EIamfAudioElementType {
  ck_audio_element_type_none = -1,
  ck_audio_element_type_channel_based,
  ck_audio_element_type_scene_based,
  ck_audio_element_type_object_based,
  ck_audio_element_type_count
} iamf_audio_element_type_t;

typedef enum EIamfAmbisonicsMode {
  ck_ambisonics_mode_none = -1,
  ck_ambisonics_mode_mono,
  ck_ambisonics_mode_projection,
  ck_ambisonics_mode_count
} iamf_ambisonics_mode_t;

typedef struct ObuOutputGainInfo {
  uint32_t output_gain_flag;
  int output_gain;
  float output_gain_linear;
} obu_output_gain_info_t;

typedef struct ObuChannelLayerConfig {
  uint32_t loudspeaker_layout;
  uint32_t output_gain_is_present_flag;
  uint32_t recon_gain_is_present_flag;
  uint32_t substream_count;
  uint32_t coupled_substream_count;
  obu_output_gain_info_t output_gain_info;
  uint32_t expanded_loudspeaker_layout;
} obu_channel_layer_config_t;

typedef struct IamfAudioElementObu {
  iamf_obu_t obu;

  uint32_t audio_element_id;
  uint32_t audio_element_type;

  uint32_t codec_config_id;

  array_t *audio_substream_ids;  // array<uint32_t>
  array_t *parameters;           // array<parameter_base_t>

} iamf_audio_element_obu_t;

typedef struct ChannelBasedAudioElementObu {
  iamf_audio_element_obu_t base;

  uint32_t max_valid_layers;             // max valid layers
  array_t *channel_audio_layer_configs;  // array<obu_channel_layer_config_t>
} channel_based_audio_element_obu_t;

typedef struct SceneBasedAudioElementObu {
  iamf_audio_element_obu_t base;

  uint32_t ambisonics_mode;
  uint32_t output_channel_count;     // C
  uint32_t substream_count;          // N
  uint32_t coupled_substream_count;  // M
  union {
    uint8_t *channel_mapping;  // C array
    uint8_t *demixing_matrix;  // (N + M) x C matrix)
  };
  uint32_t mapping_size;
} scene_based_audio_element_obu_t;

typedef struct ObjectBasedAudioElementObu {
  iamf_audio_element_obu_t base;

  uint32_t num_objects;
} object_based_audio_element_obu_t;

typedef struct ReservedAudioElementObu {
  iamf_audio_element_obu_t base;
} reserved_audio_element_obu_t;

iamf_audio_element_obu_t *iamf_audio_element_obu_new(io_context_t *ior);
void iamf_audio_element_obu_free(iamf_audio_element_obu_t *obu);
int iamf_audio_element_obu_check_profile(iamf_audio_element_obu_t *obu,
                                         iamf_profile_t profile);
parameter_base_t *iamf_audio_element_obu_get_parameter(
    iamf_audio_element_obu_t *obu, iamf_parameter_type_t type);

#endif  // __AUDIO_ELEMENT_OBU_H__
