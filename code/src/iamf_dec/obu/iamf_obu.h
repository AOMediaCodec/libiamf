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
 * @file iamf_obu.h
 * @brief OBU APIs.
 * @version 2.0.0
 * @date Created 12/11/2025
 **/

#ifndef __IAMF_OBU_H__
#define __IAMF_OBU_H__

#include "IAMF_defines.h"
#include "carray.h"
#include "cbuffer.h"
#include "parameter_base.h"

/// @brief The IA profile.
typedef enum EIamfProfile {
  /// @brief the invalid profile.
  ck_iamf_profile_none = -1,
  /// @brief the simple profile.
  ck_iamf_profile_simple,
  /// @brief the base profile.
  ck_iamf_profile_base,
  /// @brief the base-enhanced profile.
  ck_iamf_profile_base_enhanced,
  /// @brief the base advanced profile.
  ck_iamf_profile_base_advanced,
  /// @brief the advanced profile 1.
  ck_iamf_profile_advanced_1,
  /// @brief the advanced profile 2.
  ck_iamf_profile_advanced_2,
} iamf_profile_t;

typedef enum EIamfObuType {
  ck_iamf_obu_none = -1,
  ck_iamf_obu_codec_config,
  ck_iamf_obu_audio_element,
  ck_iamf_obu_mix_presentation,
  ck_iamf_obu_parameter_block,
  ck_iamf_obu_temporal_delimiter,
  ck_iamf_obu_audio_frame,
  ck_iamf_obu_audio_frame_id0,
  ck_iamf_obu_audio_frame_id1,
  ck_iamf_obu_audio_frame_id2,
  ck_iamf_obu_audio_frame_id3,
  ck_iamf_obu_audio_frame_id4,
  ck_iamf_obu_audio_frame_id5,
  ck_iamf_obu_audio_frame_id6,
  ck_iamf_obu_audio_frame_id7,
  ck_iamf_obu_audio_frame_id8,
  ck_iamf_obu_audio_frame_id9,
  ck_iamf_obu_audio_frame_id10,
  ck_iamf_obu_audio_frame_id11,
  ck_iamf_obu_audio_frame_id12,
  ck_iamf_obu_audio_frame_id13,
  ck_iamf_obu_audio_frame_id14,
  ck_iamf_obu_audio_frame_id15,
  ck_iamf_obu_audio_frame_id16,
  ck_iamf_obu_audio_frame_id17,
  ck_iamf_obu_metadata,
  ck_iamf_obu_reserved_start,
  ck_iamf_obu_reserved_end = 30,
  ck_iamf_obu_sequence_header = 31,
  ck_iamf_obu_count,
} iamf_obu_type_t;

typedef enum EIamfObuFlag {
  ck_iamf_obu_flag_redundant = 0x1,
} iamf_obu_flag_t;

typedef struct IamfObuRaw {
  io_context_t ioctx;
} iamf_obu_raw_t;

typedef struct IamfObuHeader {
  iamf_obu_type_t obu_type;
  uint8_t obu_redundant_copy;
  union {
    struct {
      uint8_t obu_trimming_status_flag;
      uint32_t num_samples_to_trim_at_end;
      uint32_t num_samples_to_trim_at_start;
    };

    uint8_t is_not_key_frame;
    uint8_t optional_fields_flag;
    uint8_t reserved;
  };
} iamf_obu_header_t;

typedef struct IamfObu {
  iamf_obu_type_t obu_type;
  uint32_t obu_flags;
} iamf_obu_t;

typedef char(string128_t)[128];

typedef struct IamfTag {
  string128_t name;
  string128_t value;
} iamf_tag_t;

typedef parameter_base_t *(*fun_get_parameter_base_t)(void *, uint32_t);
typedef array_t *(*fun_get_recon_gain_present_flags_t)(void *, uint32_t);

typedef struct IamfParameterBlockObuExtraInterfaces {
  void *this;
  fun_get_parameter_base_t get_parameter_base;
  fun_get_recon_gain_present_flags_t get_recon_gain_present_flags;
} iamf_pbo_extra_interfaces_t;

typedef struct IamfObuExtraParameters {
  iamf_pbo_extra_interfaces_t pbo_interfaces;
} iamf_obu_extra_parameters_t;

typedef struct IamfProfileInfo {
  /// @brief The maximum number of elements in a mix-presentation.
  const uint32_t max_elements;
  /// @brief The maximum number of channels in a mix-presentation.
  const uint32_t max_channels;
  /// @brief The maximum number of channels in a ambisonics element.
  const uint32_t max_amb_channels;
  /// @brief The maximum number of sub-mixes in a mix-presentation.
  const uint32_t max_sub_mixes;
  /// @brief The maximum value of headphone rendering mode.
  const uint32_t max_available_headphone_rendering_mode;
} iamf_profile_info_t;

uint32_t iamf_obu_raw_split(uint8_t *data, uint32_t size, iamf_obu_raw_t *raw);
int iamf_obu_raw_check(iamf_obu_raw_t *raw);
int iamf_obu_raw_is_reserved_obu(iamf_obu_raw_t *raw, iamf_profile_t profile);
int iamf_obu_raw_header_display(iamf_obu_raw_t *raw);

iamf_obu_t *iamf_obu_new(iamf_obu_raw_t *raw,
                         iamf_obu_extra_parameters_t *param);
void iamf_obu_free(iamf_obu_t *obu);

int iamf_obu_is_descriptor(iamf_obu_t *obu);
int iamf_obu_is_redundant_copy(iamf_obu_t *obu);

const iamf_profile_info_t *iamf_profile_info_get(iamf_profile_t profile);

iamf_tag_t *iamf_tag_new(io_context_t *ior);

#endif  // __IAMF_OBU_H__
