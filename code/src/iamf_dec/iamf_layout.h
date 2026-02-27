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
 * @file iamf_layout.h
 * @brief Channel Layout APIs.
 * @version 0.1
 * @date Created 08/05/2024
 **/

#ifndef __IAMF_LAYOUT_H__
#define __IAMF_LAYOUT_H__

#include <stdint.h>

#include "iamf_private_definitions.h"
#include "iamf_utils.h"

#define def_lfe_none -1

typedef struct IamfLayoutInfo iamf_layout_info_t;

typedef enum EIamfLayoutID {
  /// @brief No layout.
  ck_iamf_layout_id_none,

  /// @brief output channel layout in version 1.0
  ck_iamf_layout_id_sound_system_e = 0x01002000,
  ck_iamf_layout_id_sound_system_f,
  ck_iamf_layout_id_sound_system_g,

  /// @brief input/output channel layout in version 1.0
  ck_iamf_layout_id_mono = 0x01003000,
  ck_iamf_layout_id_stereo,  // SOUND_SYSTEM_A
  ck_iamf_layout_id_5_1,     // SOUND_SYSTEM_B
  ck_iamf_layout_id_5_1_2,   // SOUND_SYSTEM_C
  ck_iamf_layout_id_5_1_4,   // SOUND_SYSTEM_D
  ck_iamf_layout_id_7_1,     // SOUND_SYSTEM_I
  ck_iamf_layout_id_7_1_2,
  ck_iamf_layout_id_7_1_4,  // SOUND_SYSTEM_J
  ck_iamf_layout_id_3_1_2,
  ck_iamf_layout_id_binaural,

  /// @brief input channel layout in version 1.1
  ck_iamf_layout_id_expanded_lfe = 0x01011000,
  ck_iamf_layout_id_expanded_stereo_s,
  ck_iamf_layout_id_expanded_stereo_ss,
  ck_iamf_layout_id_expanded_stereo_rs,
  ck_iamf_layout_id_expanded_stereo_tf,
  ck_iamf_layout_id_expanded_stereo_tb,
  ck_iamf_layout_id_expanded_top_4ch,
  ck_iamf_layout_id_expanded_3ch,
  ck_iamf_layout_id_expanded_stereo_f,
  ck_iamf_layout_id_expanded_stereo_si,
  ck_iamf_layout_id_expanded_stereo_tpsi,
  ck_iamf_layout_id_expanded_top_6ch,
  ck_iamf_layout_id_expanded_lfe_pair,
  ck_iamf_layout_id_expanded_bottom_3ch,
  ck_iamf_layout_id_expanded_bottom_4ch,
  ck_iamf_layout_id_expanded_top_1ch,
  ck_iamf_layout_id_expanded_top_5ch,

  /// @brief input/output channel layout in version 1.1
  ck_iamf_layout_id_expanded_9_1_6 = 0x01013000,
  ck_iamf_layout_id_expanded_a_2_9_3,
  ck_iamf_layout_id_expanded_7_1_5_4,

} iamf_layout_id_t;

typedef enum EIamfLayoutFlag {
  ck_iamf_layout_flag_in = 0x00000001,
  ck_iamf_layout_flag_out = 0x00000002,
  ck_iamf_layout_flag_scalable = 0x00000004,
  ck_iamf_layout_flag_expanded = 0x00000008,
  // ck_iamf_layout_flag_subset = 0x00000010,
  ck_iamf_layout_flag_subset = 0x00000000,  // disable subset flag.
} iamf_layout_flag_t;

struct IamfLayoutInfo {
  iamf_layout_id_t id;
  const char *name;
  uint32_t flags;

  struct {
    uint32_t channels;

    uint32_t surround;
    uint32_t height;
    uint32_t bottom;

    int lfe1;
    int lfe2;
  };

  iamf_sound_system_t sound_system;
  iamf_loudspeaker_layout_t layout;
  iamf_loudspeaker_layout_t reference_layout;

  uint8_t decoding_map[def_max_audio_channels];
  iamf_channel_t channel_layout[def_max_audio_channels];
};

const iamf_layout_info_t *iamf_layout_get_info(iamf_layout_t);
const iamf_layout_info_t *iamf_loudspeaker_layout_get_info(
    iamf_loudspeaker_layout_t);

int iamf_loudspeaker_layout_get_decoding_channels(iamf_loudspeaker_layout_t,
                                                  iamf_channel_t *, uint32_t);
int iamf_loudspeaker_layout_get_rendering_channels(iamf_loudspeaker_layout_t,
                                                   iamf_channel_t *, uint32_t);

int iamf_layout_channels_count(iamf_layout_t *layout);
int iamf_layout_is_equal(iamf_layout_t a, iamf_layout_t b);
int iamf_layout_higher_check(iamf_layout_t layout1, iamf_layout_t layout2,
                             int equal);

#endif  // __IAMF_LAYOUT_H__
