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
 * @file IAMF_layout.c
 * @brief Channel layout.
 * @version 0.1
 * @date Created 08/05/2024
 **/

#include "IAMF_layout.h"

static const IAMF_LayoutInfo iamf_layouts[] = {
    {
        .id = IAMF_LAYOUT_ID_MONO,
        .name = "Mono",
        .flags = IAMF_LAYOUT_FLAG_IN | IAMF_LAYOUT_FLAG_OUT |
                 IAMF_LAYOUT_FLAG_SCALABLE,
        .channels = 1,
        .height = 0,
        .surround = 1,
        .lfe1 = LFE_NONE,
        .lfe2 = LFE_NONE,
        .sound_system = SOUND_SYSTEM_MONO,
        .type = IA_CHANNEL_LAYOUT_MONO,
        .rendering_id_in = IAMF_MONO,
        .rendering_id_out = IAMF_MONO,
        .decoding_map = {0},
        .channel_layout = {IA_CH_MONO},
    },
    {
        .id = IAMF_LAYOUT_ID_STEREO,
        .name = "Stereo",
        .flags = IAMF_LAYOUT_FLAG_IN | IAMF_LAYOUT_FLAG_OUT |
                 IAMF_LAYOUT_FLAG_SCALABLE,
        .channels = 2,
        .height = 0,
        .surround = 2,
        .lfe1 = LFE_NONE,
        .lfe2 = LFE_NONE,
        .sound_system = SOUND_SYSTEM_A,
        .type = IA_CHANNEL_LAYOUT_STEREO,
        .rendering_id_in = IAMF_STEREO,
        .rendering_id_out = BS2051_A,
        .decoding_map = {0, 1},
        .channel_layout = {IA_CH_L2, IA_CH_R2},
    },
    {
        .id = IAMF_LAYOUT_ID_5_1,
        .name = "5.1",
        .flags = IAMF_LAYOUT_FLAG_IN | IAMF_LAYOUT_FLAG_OUT |
                 IAMF_LAYOUT_FLAG_SCALABLE,
        .channels = 6,
        .height = 0,
        .surround = 5,
        .lfe1 = 3,
        .lfe2 = LFE_NONE,
        .sound_system = SOUND_SYSTEM_B,
        .type = IA_CHANNEL_LAYOUT_510,
        .rendering_id_in = IAMF_51,
        .rendering_id_out = BS2051_B,
        .decoding_map = {0, 1, 4, 5, 2, 3},
        .channel_layout = {IA_CH_L5, IA_CH_R5, IA_CH_C, IA_CH_LFE, IA_CH_SL5,
                           IA_CH_SR5},
    },
    {
        .id = IAMF_LAYOUT_ID_5_1_2,
        .name = "5.1.2",
        .flags = IAMF_LAYOUT_FLAG_IN | IAMF_LAYOUT_FLAG_OUT |
                 IAMF_LAYOUT_FLAG_SCALABLE,
        .channels = 8,
        .height = 2,
        .surround = 5,
        .lfe1 = 3,
        .lfe2 = LFE_NONE,
        .sound_system = SOUND_SYSTEM_C,
        .type = IA_CHANNEL_LAYOUT_512,
        .rendering_id_in = IAMF_512,
        .rendering_id_out = BS2051_C,
        .decoding_map = {0, 1, 4, 5, 6, 7, 2, 3},
        .channel_layout = {IA_CH_L5, IA_CH_R5, IA_CH_C, IA_CH_LFE, IA_CH_SL5,
                           IA_CH_SR5, IA_CH_HL, IA_CH_HR},
    },
    {
        .id = IAMF_LAYOUT_ID_5_1_4,
        .name = "5.1.4",
        .flags = IAMF_LAYOUT_FLAG_IN | IAMF_LAYOUT_FLAG_OUT |
                 IAMF_LAYOUT_FLAG_SCALABLE,
        .channels = 10,
        .height = 4,
        .surround = 5,
        .lfe1 = 3,
        .lfe2 = LFE_NONE,
        .sound_system = SOUND_SYSTEM_D,
        .type = IA_CHANNEL_LAYOUT_514,
        .rendering_id_in = IAMF_514,
        .rendering_id_out = BS2051_D,
        .decoding_map = {0, 1, 4, 5, 6, 7, 8, 9, 2, 3},
        .channel_layout = {IA_CH_L5, IA_CH_R5, IA_CH_C, IA_CH_LFE, IA_CH_SL5,
                           IA_CH_SR5, IA_CH_HFL, IA_CH_HFR, IA_CH_HBL,
                           IA_CH_HBR},
    },
    {
        .id = IAMF_LAYOUT_ID_7_1,
        .name = "7.1",
        .flags = IAMF_LAYOUT_FLAG_IN | IAMF_LAYOUT_FLAG_OUT |
                 IAMF_LAYOUT_FLAG_SCALABLE,
        .channels = 8,
        .height = 0,
        .surround = 7,
        .lfe1 = 3,
        .lfe2 = LFE_NONE,
        .sound_system = SOUND_SYSTEM_I,
        .type = IA_CHANNEL_LAYOUT_710,
        .rendering_id_in = IAMF_71,
        .rendering_id_out = BS2051_I,
        .decoding_map = {0, 1, 4, 5, 6, 7, 2, 3},
        .channel_layout = {IA_CH_L7, IA_CH_R7, IA_CH_C, IA_CH_LFE, IA_CH_SL7,
                           IA_CH_SR7, IA_CH_BL7, IA_CH_BR7},
    },
    {
        .id = IAMF_LAYOUT_ID_7_1_2,
        .name = "7.1.2",
        .flags = IAMF_LAYOUT_FLAG_IN | IAMF_LAYOUT_FLAG_OUT |
                 IAMF_LAYOUT_FLAG_SCALABLE,
        .channels = 10,
        .height = 2,
        .surround = 7,
        .lfe1 = 3,
        .lfe2 = LFE_NONE,
        .sound_system = SOUND_SYSTEM_EXT_712,
        .type = IA_CHANNEL_LAYOUT_712,
        .rendering_id_in = IAMF_712,
        .rendering_id_out = IAMF_712,
        .decoding_map = {0, 1, 4, 5, 6, 7, 8, 9, 2, 3},
        .channel_layout = {IA_CH_L7, IA_CH_R7, IA_CH_C, IA_CH_LFE, IA_CH_SL7,
                           IA_CH_SR7, IA_CH_BL7, IA_CH_BR7, IA_CH_HL, IA_CH_HR},
    },
    {
        .id = IAMF_LAYOUT_ID_7_1_4,
        .name = "7.1.4",
        .flags = IAMF_LAYOUT_FLAG_IN | IAMF_LAYOUT_FLAG_OUT |
                 IAMF_LAYOUT_FLAG_SCALABLE,
        .channels = 12,
        .height = 4,
        .surround = 7,
        .sound_system = SOUND_SYSTEM_J,
        .type = IA_CHANNEL_LAYOUT_714,
        .lfe1 = 3,
        .lfe2 = LFE_NONE,
        .rendering_id_in = IAMF_714,
        .rendering_id_out = BS2051_J,
        .decoding_map = {0, 1, 4, 5, 6, 7, 8, 9, 10, 11, 2, 3},
        .channel_layout = {IA_CH_L7, IA_CH_R7, IA_CH_C, IA_CH_LFE, IA_CH_SL7,
                           IA_CH_SR7, IA_CH_BL7, IA_CH_BR7, IA_CH_HFL,
                           IA_CH_HFR, IA_CH_HBL, IA_CH_HBR},
    },
    {
        .id = IAMF_LAYOUT_ID_3_1_2,
        .name = "3.1.2",
        .flags = IAMF_LAYOUT_FLAG_IN | IAMF_LAYOUT_FLAG_OUT |
                 IAMF_LAYOUT_FLAG_SCALABLE,
        .channels = 6,
        .height = 2,
        .surround = 3,
        .lfe1 = 3,
        .lfe2 = LFE_NONE,
        .sound_system = SOUND_SYSTEM_EXT_312,
        .type = IA_CHANNEL_LAYOUT_312,
        .rendering_id_in = IAMF_312,
        .rendering_id_out = IAMF_312,
        .decoding_map = {0, 1, 4, 5, 2, 3},
        .channel_layout = {IA_CH_L3, IA_CH_R3, IA_CH_C, IA_CH_LFE, IA_CH_TL,
                           IA_CH_TR},
    },
    {
        .id = IAMF_LAYOUT_ID_BINAURAL,
        .name = "binaural",
        .flags = IAMF_LAYOUT_FLAG_IN | IAMF_LAYOUT_FLAG_OUT,
        .channels = 2,
        .lfe1 = LFE_NONE,
        .lfe2 = LFE_NONE,
        .type = IA_CHANNEL_LAYOUT_BINAURAL,
        .rendering_id_in = IAMF_BINAURAL,
        .rendering_id_out = IAMF_BINAURAL,
    },
    {
        .id = IAMF_LAYOUT_ID_SOUND_SYSTEM_E,
        .name = "Sound System E",
        .flags = IAMF_LAYOUT_FLAG_OUT,
        .channels = 11,
        .lfe1 = 3,
        .lfe2 = LFE_NONE,
        .sound_system = SOUND_SYSTEM_E,
        .type = IA_CHANNEL_LAYOUT_INVALID,
        .rendering_id_out = BS2051_E,
    },
    {
        .id = IAMF_LAYOUT_ID_SOUND_SYSTEM_F,
        .name = "Sound System F",
        .flags = IAMF_LAYOUT_FLAG_OUT,
        .channels = 12,
        .lfe1 = 10,
        .lfe2 = 11,
        .sound_system = SOUND_SYSTEM_F,
        .type = IA_CHANNEL_LAYOUT_INVALID,
        .rendering_id_out = BS2051_F,
    },
    {
        .id = IAMF_LAYOUT_ID_SOUND_SYSTEM_G,
        .name = "Sound System G",
        .flags = IAMF_LAYOUT_FLAG_OUT,
        .channels = 14,
        .lfe1 = 3,
        .lfe2 = LFE_NONE,
        .sound_system = SOUND_SYSTEM_G,
        .type = IA_CHANNEL_LAYOUT_INVALID,
        .rendering_id_out = BS2051_G,
    },
    {
        .id = IAMF_LAYOUT_ID_SOUND_SYSTEM_H,
        .name = "Sound System H",
        .flags = IAMF_LAYOUT_FLAG_OUT,
        .channels = 24,
        .lfe1 = 3,
        .lfe2 = 9,
        .sound_system = SOUND_SYSTEM_H,
        .type = IA_CHANNEL_LAYOUT_INVALID,
        .rendering_id_out = BS2051_H,
    },
    {
        .id = IAMF_LAYOUT_ID_EXPANDED_LFE,
        .name = "LFE",
        .flags = IAMF_LAYOUT_FLAG_IN | IAMF_LAYOUT_FLAG_EXPANDED,
        .channels = 1,
        .type = IA_CHANNEL_LAYOUT_EXPANDED_LFE,
        .reference_id = IAMF_LAYOUT_ID_7_1_4,
        .sp_labels = SP_LFE1,
        .decoding_map = {0},
        .channel_layout = {IA_CH_LFE},
    },
    {
        .id = IAMF_LAYOUT_ID_EXPANDED_STEREO_S,
        .name = "Stereo-S",
        .flags = IAMF_LAYOUT_FLAG_IN | IAMF_LAYOUT_FLAG_EXPANDED,
        .channels = 2,
        .type = IA_CHANNEL_LAYOUT_EXPANDED_STEREO_S,
        .reference_id = IAMF_LAYOUT_ID_5_1_4,
        .sp_labels = SP_MP110 | SP_MM110,
        .decoding_map = {0, 1},
        .channel_layout = {IA_CH_SL5, IA_CH_SR5},
    },
    {
        .id = IAMF_LAYOUT_ID_EXPANDED_STEREO_SS,
        .name = "STEREO-SS",
        .flags = IAMF_LAYOUT_FLAG_IN | IAMF_LAYOUT_FLAG_EXPANDED,
        .channels = 2,
        .type = IA_CHANNEL_LAYOUT_EXPANDED_STEREO_SS,
        .reference_id = IAMF_LAYOUT_ID_7_1_4,
        .sp_labels = SP_MP090 | SP_MM090,
        .decoding_map = {0, 1},
        .channel_layout = {IA_CH_SL7, IA_CH_SR7},
    },
    {
        .id = IAMF_LAYOUT_ID_EXPANDED_STEREO_RS,
        .name = "STEREO-RS",
        .flags = IAMF_LAYOUT_FLAG_IN | IAMF_LAYOUT_FLAG_EXPANDED,
        .channels = 2,
        .type = IA_CHANNEL_LAYOUT_EXPANDED_STEREO_RS,
        .reference_id = IAMF_LAYOUT_ID_7_1_4,
        .sp_labels = SP_MP135 | SP_MM135,
        .decoding_map = {0, 1},
        .channel_layout = {IA_CH_BL7, IA_CH_BR7},
    },
    {
        .id = IAMF_LAYOUT_ID_EXPANDED_STEREO_TF,
        .name = "STEREO-TF",
        .flags = IAMF_LAYOUT_FLAG_IN | IAMF_LAYOUT_FLAG_EXPANDED,
        .channels = 2,
        .type = IA_CHANNEL_LAYOUT_EXPANDED_STEREO_TF,
        .reference_id = IAMF_LAYOUT_ID_7_1_4,
        .sp_labels = SP_UP045 | SP_UM045,
        .decoding_map = {0, 1},
        .channel_layout = {IA_CH_HFL, IA_CH_HFR},
    },
    {
        .id = IAMF_LAYOUT_ID_EXPANDED_STEREO_TB,
        .name = "STEREO-TB",
        .flags = IAMF_LAYOUT_FLAG_IN | IAMF_LAYOUT_FLAG_EXPANDED,
        .channels = 2,
        .type = IA_CHANNEL_LAYOUT_EXPANDED_STEREO_TB,
        .reference_id = IAMF_LAYOUT_ID_7_1_4,
        .sp_labels = SP_UP135 | SP_UM135,
        .decoding_map = {0, 1},
        .channel_layout = {IA_CH_HBL, IA_CH_HBR},
    },
    {
        .id = IAMF_LAYOUT_ID_EXPANDED_TOP_4CH,
        .name = "Top-4ch",
        .flags = IAMF_LAYOUT_FLAG_IN | IAMF_LAYOUT_FLAG_EXPANDED,
        .channels = 4,
        .type = IA_CHANNEL_LAYOUT_EXPANDED_TOP_4CH,
        .reference_id = IAMF_LAYOUT_ID_7_1_4,
        .sp_labels = SP_UP045 | SP_UM045 | SP_UP135 | SP_UM135,
        .decoding_map = {0, 1, 2, 3},
        .channel_layout = {IA_CH_HFL, IA_CH_HFR, IA_CH_HBL, IA_CH_HBR},
    },
    {
        .id = IAMF_LAYOUT_ID_EXPANDED_3CH,
        .name = "3.0ch",
        .flags = IAMF_LAYOUT_FLAG_IN | IAMF_LAYOUT_FLAG_EXPANDED,
        .channels = 3,
        .type = IA_CHANNEL_LAYOUT_EXPANDED_3CH,
        .reference_id = IAMF_LAYOUT_ID_7_1_4,
        .sp_labels = SP_MP030 | SP_MP000 | SP_MM030,
        .decoding_map = {0, 1, 2},
        .channel_layout = {IA_CH_L7, IA_CH_R7, IA_CH_C},
    },
    {
        .id = IAMF_LAYOUT_ID_EXPANDED_9_1_6,
        .name = "9.1.6",
        .flags = IAMF_LAYOUT_FLAG_IN | IAMF_LAYOUT_FLAG_OUT |
                 IAMF_LAYOUT_FLAG_EXPANDED,
        .channels = 16,
        .lfe1 = 3,
        .lfe2 = LFE_NONE,
        .sound_system = SOUND_SYSTEM_EXT_916,
        .type = IA_CHANNEL_LAYOUT_EXPANDED_916,
        .rendering_id_in = IAMF_916,
        .rendering_id_out = IAMF_916,
        .decoding_map = {6, 7, 0, 1, 8, 9, 4, 5, 10, 11, 14, 15, 12, 13, 2, 3},
        .channel_layout = {IA_CH_FL, IA_CH_FR, IA_CH_FC, IA_CH_LFE1, IA_CH_BL,
                           IA_CH_BR, IA_CH_FLC, IA_CH_FRC, IA_CH_SIL, IA_CH_SIR,
                           IA_CH_TPFL, IA_CH_TPFR, IA_CH_TPBL, IA_CH_TPBR,
                           IA_CH_TPSIL, IA_CH_TPSIR},
    },
    {
        .id = IAMF_LAYOUT_ID_EXPANDED_STEREO_F,
        .name = "Stereo-F",
        .flags = IAMF_LAYOUT_FLAG_IN | IAMF_LAYOUT_FLAG_EXPANDED,
        .channels = 2,
        .type = IA_CHANNEL_LAYOUT_EXPANDED_STEREO_F,
        .reference_id = IAMF_LAYOUT_ID_EXPANDED_9_1_6,
        .sp_labels = SP_MP060 | SP_MM060,
        .decoding_map = {0, 1},
        .channel_layout = {IA_CH_FL, IA_CH_FR},
    },
    {
        .id = IAMF_LAYOUT_ID_EXPANDED_STEREO_SI,
        .name = "Stereo-Si",
        .flags = IAMF_LAYOUT_FLAG_IN | IAMF_LAYOUT_FLAG_EXPANDED,
        .channels = 2,
        .type = IA_CHANNEL_LAYOUT_EXPANDED_STEREO_SI,
        .reference_id = IAMF_LAYOUT_ID_EXPANDED_9_1_6,
        .sp_labels = SP_MP090 | SP_MM090,
        .decoding_map = {0, 1},
        .channel_layout = {IA_CH_SIL, IA_CH_SIR},
    },
    {
        .id = IAMF_LAYOUT_ID_EXPANDED_STEREO_TPSI,
        .name = "Stereo-TpSi",
        .flags = IAMF_LAYOUT_FLAG_IN | IAMF_LAYOUT_FLAG_EXPANDED,
        .channels = 2,
        .type = IA_CHANNEL_LAYOUT_EXPANDED_STEREO_TPSI,
        .reference_id = IAMF_LAYOUT_ID_EXPANDED_9_1_6,
        .sp_labels = SP_UP090 | SP_UM090,
        .decoding_map = {0, 1},
        .channel_layout = {IA_CH_TPSIL, IA_CH_TPSIR},
    },
    {
        .id = IAMF_LAYOUT_ID_EXPANDED_TOP_6CH,
        .name = "Top-6ch",
        .flags = IAMF_LAYOUT_FLAG_IN | IAMF_LAYOUT_FLAG_EXPANDED,
        .channels = 6,
        .type = IA_CHANNEL_LAYOUT_EXPANDED_TOP_6CH,
        .reference_id = IAMF_LAYOUT_ID_EXPANDED_9_1_6,
        .sp_labels =
            SP_UP045 | SP_UM045 | SP_UP090 | SP_UM090 | SP_UP135 | SP_UM135,
        .decoding_map = {0, 1, 4, 5, 2, 3},
        .channel_layout = {IA_CH_TPFL, IA_CH_TPFR, IA_CH_TPBL, IA_CH_TPBR,
                           IA_CH_TPSIL, IA_CH_TPSIR},
    }};

const IAMF_LayoutInfo *iamf_sound_system_get_layout_info(IAMF_SoundSystem ss) {
  const IAMF_LayoutInfo *info = 0;
  for (uint32_t i = 0; i < sizeof(iamf_layouts) / sizeof(iamf_layouts[0]);
       ++i) {
    if (iamf_layouts[i].sound_system == ss &&
        (iamf_layouts[i].flags & IAMF_LAYOUT_FLAG_OUT)) {
      info = &iamf_layouts[i];
      break;
    }
  }
  return info;
}

const IAMF_LayoutInfo *iamf_audio_layer_get_layout_info(
    IAChannelLayoutType type) {
  const IAMF_LayoutInfo *info = 0;
  for (uint32_t i = 0; i < sizeof(iamf_layouts) / sizeof(iamf_layouts[0]);
       ++i) {
    if (iamf_layouts[i].type == type &&
        (iamf_layouts[i].flags & IAMF_LAYOUT_FLAG_IN)) {
      info = &iamf_layouts[i];
      break;
    }
  }
  return info;
}

const IAMF_LayoutInfo *iamf_get_layout_info(IAMF_LayoutID id) {
  const IAMF_LayoutInfo *info = 0;
  for (uint32_t i = 0; i < sizeof(iamf_layouts) / sizeof(iamf_layouts[0]);
       ++i) {
    if (iamf_layouts[i].id == id) {
      info = &iamf_layouts[i];
      break;
    }
  }
  return info;
}

int iamf_audio_layer_layout_get_decoding_channels(IAChannelLayoutType type,
                                                  IAChannel *channels,
                                                  uint32_t count) {
  const IAMF_LayoutInfo *info = iamf_audio_layer_get_layout_info(type);
  for (uint32_t i = 0; i < info->channels; ++i)
    channels[i] = info->channel_layout[info->decoding_map[i]];
  return info->channels;
}

int iamf_audio_layer_layout_get_rendering_channels(IAChannelLayoutType type,
                                                   IAChannel *channels,
                                                   uint32_t count) {
  const IAMF_LayoutInfo *info = iamf_audio_layer_get_layout_info(type);
  if (count < info->channels) return IAMF_ERR_BUFFER_TOO_SMALL;
  for (uint32_t i = 0; i < info->channels; ++i)
    channels[i] = info->channel_layout[i];
  return info->channels;
}