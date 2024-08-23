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
 * @file IAMF_layout.h
 * @brief Channel Layout APIs.
 * @version 0.1
 * @date Created 08/05/2024
 **/

#ifndef IAMF_LAYOUT_H
#define IAMF_LAYOUT_H

#include <stdint.h>

#include "IAMF_utils.h"
#include "ae_rdr.h"

#define LFE_NONE -1

typedef struct IAMF_LayoutInfo IAMF_LayoutInfo;

typedef enum {
  IAMF_LAYOUT_ID_NONE,

  /// @brief output channel layout in version 1.0
  IAMF_LAYOUT_ID_SOUND_SYSTEM_E = 0x01002000,
  IAMF_LAYOUT_ID_SOUND_SYSTEM_F,
  IAMF_LAYOUT_ID_SOUND_SYSTEM_G,
  IAMF_LAYOUT_ID_SOUND_SYSTEM_H,

  /// @brief input/output channel layout in version 1.0
  IAMF_LAYOUT_ID_MONO = 0x01003000,
  IAMF_LAYOUT_ID_STEREO,  // SOUND_SYSTEM_A
  IAMF_LAYOUT_ID_5_1,     // SOUND_SYSTEM_B
  IAMF_LAYOUT_ID_5_1_2,   // SOUND_SYSTEM_C
  IAMF_LAYOUT_ID_5_1_4,   // SOUND_SYSTEM_D
  IAMF_LAYOUT_ID_7_1,     // SOUND_SYSTEM_I
  IAMF_LAYOUT_ID_7_1_2,
  IAMF_LAYOUT_ID_7_1_4,  // SOUND_SYSTEM_J
  IAMF_LAYOUT_ID_3_1_2,
  IAMF_LAYOUT_ID_BINAURAL,

  /// @brief input channel layout in version 1.1
  IAMF_LAYOUT_ID_EXPANDED_LFE = 0x01011000,
  IAMF_LAYOUT_ID_EXPANDED_STEREO_S,
  IAMF_LAYOUT_ID_EXPANDED_STEREO_SS,
  IAMF_LAYOUT_ID_EXPANDED_STEREO_RS,
  IAMF_LAYOUT_ID_EXPANDED_STEREO_TF,
  IAMF_LAYOUT_ID_EXPANDED_STEREO_TB,
  IAMF_LAYOUT_ID_EXPANDED_TOP_4CH,
  IAMF_LAYOUT_ID_EXPANDED_3CH,
  IAMF_LAYOUT_ID_EXPANDED_STEREO_F,
  IAMF_LAYOUT_ID_EXPANDED_STEREO_SI,
  IAMF_LAYOUT_ID_EXPANDED_STEREO_TPSI,
  IAMF_LAYOUT_ID_EXPANDED_TOP_6CH,

  /// @brief input/output channel layout in version 1.1
  IAMF_LAYOUT_ID_EXPANDED_9_1_6 = 0x01013000,
} IAMF_LayoutID;

enum IAMF_LayoutFlag {
  IAMF_LAYOUT_FLAG_IN = 0x00000001,
  IAMF_LAYOUT_FLAG_OUT = 0x00000002,
  IAMF_LAYOUT_FLAG_SCALABLE = 0x00000004,
  IAMF_LAYOUT_FLAG_EXPANDED = 0x00000008,
};

struct IAMF_LayoutInfo {
  IAMF_LayoutID id;
  const char *name;
  uint32_t flags;
  uint32_t channels;

  uint32_t surround;
  uint32_t height;

  int lfe1;
  int lfe2;

  IAMF_SoundSystem sound_system;
  IAChannelLayoutType type;

  IAMF_SOUND_SYSTEM rendering_id_in;
  IAMF_SOUND_SYSTEM rendering_id_out;

  IAMF_LayoutID reference_id;
  BS2051_SPLABEL sp_labels;

  uint8_t decoding_map[IA_CH_LAYOUT_MAX_CHANNELS];
  IAChannel channel_layout[IA_CH_LAYOUT_MAX_CHANNELS];
};

const IAMF_LayoutInfo *iamf_sound_system_get_layout_info(IAMF_SoundSystem);
const IAMF_LayoutInfo *iamf_audio_layer_get_layout_info(IAChannelLayoutType);
const IAMF_LayoutInfo *iamf_get_layout_info(IAMF_LayoutID);

int iamf_audio_layer_layout_get_decoding_channels(IAChannelLayoutType,
                                                  IAChannel *, uint32_t);
int iamf_audio_layer_layout_get_rendering_channels(IAChannelLayoutType,
                                                   IAChannel *, uint32_t);
#endif  // IAMF_LAYOUT_H