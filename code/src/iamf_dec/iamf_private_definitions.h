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
 * @file iamf_private_definitions.h
 * @brief IAMF private definitions.
 * @version 2.0.0
 * @date Created 12/11/2025
 **/

#ifndef __IAMF_PRIVATE_DEFINITIONS_H__
#define __IAMF_PRIVATE_DEFINITIONS_H__

#include "IAMF_defines.h"
#include "clog.h"
#include "definitions.h"

#define def_iamf_ustr "IAMF"

#define def_pass 1
#define def_error 0
#define def_fatal (-1)

#define def_true 1
#define def_false 0

#define def_iamf_profile_default ck_iamf_profile_advanced_2
#define def_iamf_profile_count (def_iamf_profile_default + 1)

#define def_audio_sample_format_default 0x10
#define def_default_sampling_rate 48000

#define def_limiter_max_true_peak -1.0f
#define def_default_loudness_lkfs 0.0f
#define def_default_loudness_gain 1.0f

#define def_i32_id_none (-1)
#define def_i64_id_none (-1LL)

#define def_iamf_decoder_config_mix_presentation 0x1
#define def_iamf_decoder_config_output_layout 0x2
#define def_iamf_decoder_config_presentation 0x100

#define def_err_msg_enomem(obj, tag) \
  error("Fail to allocate memory for " obj " of " tag ".")

#define def_lsb_16bits(a) ((a) & 0xFFFF)
#define def_lsb_32bits(a) ((a) & 0xFFFFFFFFULL)

#define def_dmx_mode_none -1
#define def_dmx_weight_index_none -1

#define def_base_enhanced_audio_elements 28
#define def_base_enhanced_audio_channels def_base_enhanced_audio_elements
#define def_base_enhanced_ambisonic_audio_channels 25

#define def_max_audio_streams def_base_enhanced_ambisonic_audio_channels
#define def_max_audio_channels def_base_enhanced_audio_channels
#define def_hash_map_capacity_elements 32
#define def_hash_map_capacity_audio_frames def_hash_map_capacity_elements
#define def_max_channel_groups 8
#define def_max_sub_mixes 2
#define def_max_codec_configs 2

#define def_iamf_loudspeaker_layout_expanded 15
#define def_iamf_loudspeaker_layout_expanded_mask 0x10

#define def_default_recon_gain 1.0f

#define def_ccs_str_size 1024
#define def_cc_str_size 128
#define def_q78_num_bits 16
#define def_azimuth_num_bits 9
#define def_elevation_num_bits 8
#define def_distance_num_bits 7

// Assumed bit depths for cartesian coordinates
#define def_cartesian_8_num_bits 8
#define def_cartesian_16_num_bits 16

#define def_max_audio_objects 2

#define def_max_opus_frame_size 960 * 6
#define def_max_aac_frame_size 2048
#define def_max_flac_frame_size 32768

#define def_clip3(min, max, val) (val < min ? min : val > max ? max : val)
#define def_azimuth_clip3(val) def_clip3(-180, 180, val)
#define def_elevation_clip3(val) def_clip3(-90, 90, val)

#define def_ptr(type, a) ((type##_t *)(a))
#define def_cast(type, a) ((type)(a))
#define def_ia_profile_cast(a) def_cast(IA_Profile, (a))
#define def_iamf_profile_cast(a) def_cast(iamf_profile_t, (a))
#define def_iamf_codec_id_cast(a) def_cast(IAMF_CodecID, (a))

#define def_rshift(a) (1 << (a))

#define def_sound_system_layout_instance(layout)            \
  (iamf_layout_t) {                                         \
    .type = ck_iamf_layout_type_loudspeakers_ss_convention, \
    .sound_system = layout                                  \
  }
#define def_binaural_layout_instance() \
  (iamf_layout_t) { .type = ck_iamf_layout_type_binaural }

#endif  // __IAMF_PRIVATE_DEFINITIONS_H__
