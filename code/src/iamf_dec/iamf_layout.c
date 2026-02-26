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
 * @version 2.0.0
 * @date Created 08/05/2024
 **/

#include "iamf_layout.h"

static const iamf_layout_info_t iamf_layouts[] = {
    {
        .id = ck_iamf_layout_id_mono,
        .name = "Mono",
        .flags = ck_iamf_layout_flag_in | ck_iamf_layout_flag_out |
                 ck_iamf_layout_flag_scalable,
        .channels = 1,
        .surround = 1,
        .lfe1 = def_lfe_none,
        .lfe2 = def_lfe_none,
        .sound_system = SOUND_SYSTEM_MONO,
        .layout = ck_iamf_loudspeaker_layout_mono,
        .decoding_map = {0},
        .channel_layout = {ck_iamf_channel_mono},
    },
    {
        .id = ck_iamf_layout_id_stereo,
        .name = "Stereo",
        .flags = ck_iamf_layout_flag_in | ck_iamf_layout_flag_out |
                 ck_iamf_layout_flag_scalable,
        .channels = 2,
        .surround = 2,
        .lfe1 = def_lfe_none,
        .lfe2 = def_lfe_none,
        .sound_system = SOUND_SYSTEM_A,
        .layout = ck_iamf_loudspeaker_layout_stereo,
        .decoding_map = {0, 1},
        .channel_layout = {ck_iamf_channel_l2, ck_iamf_channel_r2},
    },
    {
        .id = ck_iamf_layout_id_5_1,
        .name = "5.1",
        .flags = ck_iamf_layout_flag_in | ck_iamf_layout_flag_out |
                 ck_iamf_layout_flag_scalable,
        .channels = 6,
        .surround = 5,
        .lfe1 = 3,
        .lfe2 = def_lfe_none,
        .sound_system = SOUND_SYSTEM_B,
        .layout = ck_iamf_loudspeaker_layout_510,
        .decoding_map = {0, 1, 4, 5, 2, 3},
        .channel_layout = {ck_iamf_channel_l5, ck_iamf_channel_r5,
                           ck_iamf_channel_c, ck_iamf_channel_lfe,
                           ck_iamf_channel_sl5, ck_iamf_channel_sr5},
    },
    {
        .id = ck_iamf_layout_id_5_1_2,
        .name = "5.1.2",
        .flags = ck_iamf_layout_flag_in | ck_iamf_layout_flag_out |
                 ck_iamf_layout_flag_scalable,
        .channels = 8,
        .height = 2,
        .surround = 5,
        .lfe1 = 3,
        .lfe2 = def_lfe_none,
        .sound_system = SOUND_SYSTEM_C,
        .layout = ck_iamf_loudspeaker_layout_512,
        .decoding_map = {0, 1, 4, 5, 6, 7, 2, 3},
        .channel_layout = {ck_iamf_channel_l5, ck_iamf_channel_r5,
                           ck_iamf_channel_c, ck_iamf_channel_lfe,
                           ck_iamf_channel_sl5, ck_iamf_channel_sr5,
                           ck_iamf_channel_hl, ck_iamf_channel_hr},
    },
    {
        .id = ck_iamf_layout_id_5_1_4,
        .name = "5.1.4",
        .flags = ck_iamf_layout_flag_in | ck_iamf_layout_flag_out |
                 ck_iamf_layout_flag_scalable,
        .channels = 10,
        .height = 4,
        .surround = 5,
        .lfe1 = 3,
        .lfe2 = def_lfe_none,
        .sound_system = SOUND_SYSTEM_D,
        .layout = ck_iamf_loudspeaker_layout_514,
        .decoding_map = {0, 1, 4, 5, 6, 7, 8, 9, 2, 3},
        .channel_layout = {ck_iamf_channel_l5, ck_iamf_channel_r5,
                           ck_iamf_channel_c, ck_iamf_channel_lfe,
                           ck_iamf_channel_sl5, ck_iamf_channel_sr5,
                           ck_iamf_channel_hfl, ck_iamf_channel_hfr,
                           ck_iamf_channel_hbl, ck_iamf_channel_hbr},
    },
    {
        .id = ck_iamf_layout_id_7_1,
        .name = "7.1",
        .flags = ck_iamf_layout_flag_in | ck_iamf_layout_flag_out |
                 ck_iamf_layout_flag_scalable,
        .channels = 8,
        .surround = 7,
        .lfe1 = 3,
        .lfe2 = def_lfe_none,
        .sound_system = SOUND_SYSTEM_I,
        .layout = ck_iamf_loudspeaker_layout_710,
        .decoding_map = {0, 1, 4, 5, 6, 7, 2, 3},
        .channel_layout = {ck_iamf_channel_l7, ck_iamf_channel_r7,
                           ck_iamf_channel_c, ck_iamf_channel_lfe,
                           ck_iamf_channel_sl7, ck_iamf_channel_sr7,
                           ck_iamf_channel_bl7, ck_iamf_channel_br7},
    },
    {
        .id = ck_iamf_layout_id_7_1_2,
        .name = "7.1.2",
        .flags = ck_iamf_layout_flag_in | ck_iamf_layout_flag_out |
                 ck_iamf_layout_flag_scalable,
        .channels = 10,
        .height = 2,
        .surround = 7,
        .lfe1 = 3,
        .lfe2 = def_lfe_none,
        .sound_system = SOUND_SYSTEM_EXT_712,
        .layout = ck_iamf_loudspeaker_layout_712,
        .decoding_map = {0, 1, 4, 5, 6, 7, 8, 9, 2, 3},
        .channel_layout = {ck_iamf_channel_l7, ck_iamf_channel_r7,
                           ck_iamf_channel_c, ck_iamf_channel_lfe,
                           ck_iamf_channel_sl7, ck_iamf_channel_sr7,
                           ck_iamf_channel_bl7, ck_iamf_channel_br7,
                           ck_iamf_channel_hl, ck_iamf_channel_hr},
    },
    {
        .id = ck_iamf_layout_id_7_1_4,
        .name = "7.1.4",
        .flags = ck_iamf_layout_flag_in | ck_iamf_layout_flag_out |
                 ck_iamf_layout_flag_scalable,
        .channels = 12,
        .height = 4,
        .surround = 7,
        .sound_system = SOUND_SYSTEM_J,
        .layout = ck_iamf_loudspeaker_layout_714,
        .lfe1 = 3,
        .lfe2 = def_lfe_none,
        .decoding_map = {0, 1, 4, 5, 6, 7, 8, 9, 10, 11, 2, 3},
        .channel_layout = {ck_iamf_channel_l7, ck_iamf_channel_r7,
                           ck_iamf_channel_c, ck_iamf_channel_lfe,
                           ck_iamf_channel_sl7, ck_iamf_channel_sr7,
                           ck_iamf_channel_bl7, ck_iamf_channel_br7,
                           ck_iamf_channel_hfl, ck_iamf_channel_hfr,
                           ck_iamf_channel_hbl, ck_iamf_channel_hbr},
    },
    {
        .id = ck_iamf_layout_id_3_1_2,
        .name = "3.1.2",
        .flags = ck_iamf_layout_flag_in | ck_iamf_layout_flag_out |
                 ck_iamf_layout_flag_scalable,
        .channels = 6,
        .height = 2,
        .surround = 3,
        .lfe1 = 3,
        .lfe2 = def_lfe_none,
        .sound_system = SOUND_SYSTEM_EXT_312,
        .layout = ck_iamf_loudspeaker_layout_312,
        .decoding_map = {0, 1, 4, 5, 2, 3},
        .channel_layout = {ck_iamf_channel_l3, ck_iamf_channel_r3,
                           ck_iamf_channel_c, ck_iamf_channel_lfe,
                           ck_iamf_channel_tl, ck_iamf_channel_tr},
    },
    {
        .id = ck_iamf_layout_id_binaural,
        .name = "binaural",
        .flags = ck_iamf_layout_flag_in | ck_iamf_layout_flag_out,
        .channels = 2,
        .surround = 2,
        .lfe1 = def_lfe_none,
        .lfe2 = def_lfe_none,
        .layout = ck_iamf_loudspeaker_layout_binaural,
    },
    {
        .id = ck_iamf_layout_id_sound_system_e,
        .name = "Sound System E",
        .flags = ck_iamf_layout_flag_out,
        .channels = 11,
        .surround = 5,
        .height = 4,
        .bottom = 1,
        .lfe1 = 3,
        .lfe2 = def_lfe_none,
        .sound_system = SOUND_SYSTEM_E,
        .layout = ck_iamf_loudspeaker_layout_none,
    },
    {
        .id = ck_iamf_layout_id_sound_system_f,
        .name = "Sound System F",
        .flags = ck_iamf_layout_flag_out,
        .channels = 12,
        .surround = 7,
        .height = 3,
        .lfe1 = 10,
        .lfe2 = 11,
        .sound_system = SOUND_SYSTEM_F,
        .layout = ck_iamf_loudspeaker_layout_none,
    },
    {
        .id = ck_iamf_layout_id_sound_system_g,
        .name = "Sound System G",
        .flags = ck_iamf_layout_flag_out,
        .channels = 14,
        .surround = 9,
        .height = 4,
        .lfe1 = 3,
        .lfe2 = def_lfe_none,
        .sound_system = SOUND_SYSTEM_G,
        .layout = ck_iamf_loudspeaker_layout_none,
    },
    {
        .id = ck_iamf_layout_id_expanded_lfe,
        .name = "LFE",
        .flags = ck_iamf_layout_flag_in | ck_iamf_layout_flag_expanded |
                 ck_iamf_layout_flag_subset,
        .channels = 1,
        .layout = ck_iamf_loudspeaker_layout_expanded_lfe,
        .reference_layout = ck_iamf_loudspeaker_layout_714,
        // .decoding_map = {3},
        .decoding_map = {0},
    },
    {
        .id = ck_iamf_layout_id_expanded_stereo_s,
        .name = "Stereo-S",
        .flags = ck_iamf_layout_flag_in | ck_iamf_layout_flag_expanded |
                 ck_iamf_layout_flag_subset,
        .channels = 2,
        .layout = ck_iamf_loudspeaker_layout_expanded_stereo_s,
        .reference_layout = ck_iamf_loudspeaker_layout_514,
        // .decoding_map = {4, 5},
        .decoding_map = {0, 1},
    },
    {
        .id = ck_iamf_layout_id_expanded_stereo_ss,
        .name = "STEREO-SS",
        .flags = ck_iamf_layout_flag_in | ck_iamf_layout_flag_expanded |
                 ck_iamf_layout_flag_subset,
        .channels = 2,
        .layout = ck_iamf_loudspeaker_layout_expanded_stereo_ss,
        .reference_layout = ck_iamf_loudspeaker_layout_714,
        // .decoding_map = {4, 5},
        .decoding_map = {0, 1},
    },
    {
        .id = ck_iamf_layout_id_expanded_stereo_rs,
        .name = "STEREO-RS",
        .flags = ck_iamf_layout_flag_in | ck_iamf_layout_flag_expanded |
                 ck_iamf_layout_flag_subset,
        .channels = 2,
        .layout = ck_iamf_loudspeaker_layout_expanded_stereo_rs,
        .reference_layout = ck_iamf_loudspeaker_layout_714,
        // .decoding_map = {6, 7},
        .decoding_map = {0, 1},
    },
    {
        .id = ck_iamf_layout_id_expanded_stereo_tf,
        .name = "STEREO-TF",
        .flags = ck_iamf_layout_flag_in | ck_iamf_layout_flag_expanded |
                 ck_iamf_layout_flag_subset,
        .channels = 2,
        .layout = ck_iamf_loudspeaker_layout_expanded_stereo_tf,
        .reference_layout = ck_iamf_loudspeaker_layout_714,
        // .decoding_map = {8, 9},
        .decoding_map = {0, 1},
    },
    {
        .id = ck_iamf_layout_id_expanded_stereo_tb,
        .name = "STEREO-TB",
        .flags = ck_iamf_layout_flag_in | ck_iamf_layout_flag_expanded |
                 ck_iamf_layout_flag_subset,
        .channels = 2,
        .layout = ck_iamf_loudspeaker_layout_expanded_stereo_tb,
        .reference_layout = ck_iamf_loudspeaker_layout_714,
        // .decoding_map = {10, 11},
        .decoding_map = {0, 1},
    },
    {
        .id = ck_iamf_layout_id_expanded_top_4ch,
        .name = "Top-4ch",
        .flags = ck_iamf_layout_flag_in | ck_iamf_layout_flag_expanded |
                 ck_iamf_layout_flag_subset,
        .channels = 4,
        .layout = ck_iamf_loudspeaker_layout_expanded_top_4ch,
        .reference_layout = ck_iamf_loudspeaker_layout_714,
        // .decoding_map = {8, 9, 10, 11},
        .decoding_map = {0, 1, 2, 3},
    },
    {
        .id = ck_iamf_layout_id_expanded_3ch,
        .name = "3.0ch",
        .flags = ck_iamf_layout_flag_in | ck_iamf_layout_flag_expanded |
                 ck_iamf_layout_flag_subset,
        .channels = 3,
        .layout = ck_iamf_loudspeaker_layout_expanded_3ch,
        .reference_layout = ck_iamf_loudspeaker_layout_714,
        .decoding_map = {0, 1, 2},
    },
    {
        .id = ck_iamf_layout_id_expanded_9_1_6,
        .name = "9.1.6ch",
        .flags = ck_iamf_layout_flag_in | ck_iamf_layout_flag_out |
                 ck_iamf_layout_flag_expanded,
        // .flags = ck_iamf_layout_flag_in | ck_iamf_layout_flag_out |
        //          ck_iamf_layout_flag_expanded | ck_iamf_layout_flag_subset,
        .channels = 16,
        .surround = 9,
        .height = 6,
        .lfe1 = 3,
        .lfe2 = def_lfe_none,
        .sound_system = SOUND_SYSTEM_EXT_916,
        .layout = ck_iamf_loudspeaker_layout_expanded_916,
        // .reference_layout = ck_iamf_loudspeaker_layout_expanded_a293,
        .decoding_map = {6, 7, 0, 1, 8, 9, 4, 5, 10, 11, 14, 15, 12, 13, 2, 3},
        // .decoding_map = {6, 7, 0, 1, 10, 11, 4, 5, 12, 13, 18, 19, 16, 17, 2,
        //                  3}, // LFE1->LFE2 (9)
    },
    {
        .id = ck_iamf_layout_id_expanded_stereo_f,
        .name = "Stereo-F",
        .flags = ck_iamf_layout_flag_in | ck_iamf_layout_flag_expanded |
                 ck_iamf_layout_flag_subset,
        .channels = 2,
        .layout = ck_iamf_loudspeaker_layout_expanded_stereo_f,
        .reference_layout = ck_iamf_loudspeaker_layout_expanded_916,
        .decoding_map = {0, 1},
    },
    {
        .id = ck_iamf_layout_id_expanded_stereo_si,
        .name = "Stereo-Si",
        .flags = ck_iamf_layout_flag_in | ck_iamf_layout_flag_expanded |
                 ck_iamf_layout_flag_subset,
        .channels = 2,
        .layout = ck_iamf_loudspeaker_layout_expanded_stereo_si,
        .reference_layout = ck_iamf_loudspeaker_layout_expanded_916,
        // .decoding_map = {8, 9}, // 9.1.6
        // .decoding_map = {10, 11}, // 22.2
        .decoding_map = {0, 1},
    },
    {
        .id = ck_iamf_layout_id_expanded_stereo_tpsi,
        .name = "Stereo-TpSi",
        .flags = ck_iamf_layout_flag_in | ck_iamf_layout_flag_expanded |
                 ck_iamf_layout_flag_subset,
        .channels = 2,
        .layout = ck_iamf_loudspeaker_layout_expanded_stereo_tpsi,
        .reference_layout = ck_iamf_loudspeaker_layout_expanded_916,
        // .decoding_map = {14, 15}, // 9.1.6
        // .decoding_map = {18, 19}, // 22.2
        .decoding_map = {0, 1},
    },
    {
        .id = ck_iamf_layout_id_expanded_top_6ch,
        .name = "Top-6ch",
        .flags = ck_iamf_layout_flag_in | ck_iamf_layout_flag_expanded |
                 ck_iamf_layout_flag_subset,
        .channels = 6,
        .layout = ck_iamf_loudspeaker_layout_expanded_top_6ch,
        .reference_layout = ck_iamf_loudspeaker_layout_expanded_916,
        // .decoding_map = {10, 11, 14, 15, 12, 13}, // 9.1.6
        // .decoding_map = {12, 13, 18, 19, 16, 17}, // 22.2
        .decoding_map = {0, 1, 4, 5, 2, 3},
    },
    {
        .id = ck_iamf_layout_id_expanded_a_2_9_3,
        .name = "10.2.9.3ch (Sound System H)",
        .flags = ck_iamf_layout_flag_in | ck_iamf_layout_flag_out |
                 ck_iamf_layout_flag_expanded,
        .channels = 24,
        .surround = 10,
        .height = 9,
        .bottom = 3,
        .lfe1 = 3,
        .lfe2 = 9,
        .sound_system = SOUND_SYSTEM_H,
        .layout = ck_iamf_loudspeaker_layout_expanded_a293,
        .decoding_map = {6,  7,  0,  1,  10, 11, 4,  5,  12, 13, 18, 19,
                         16, 17, 22, 23, 2,  8,  14, 15, 20, 21, 3,  9},
    },
    {
        .id = ck_iamf_layout_id_expanded_lfe_pair,
        .name = "LFE-Pair",
        .flags = ck_iamf_layout_flag_in | ck_iamf_layout_flag_expanded |
                 ck_iamf_layout_flag_subset,
        .channels = 2,
        .layout = ck_iamf_loudspeaker_layout_expanded_lfe_pair,
        .reference_layout = ck_iamf_loudspeaker_layout_expanded_a293,
        // .decoding_map = {3, 9},
        .decoding_map = {0, 1},
    },
    {
        .id = ck_iamf_layout_id_expanded_bottom_3ch,
        .name = "Bottom-3ch",
        .flags = ck_iamf_layout_flag_in | ck_iamf_layout_flag_expanded |
                 ck_iamf_layout_flag_subset,
        .channels = 3,
        .bottom = 3,
        .layout = ck_iamf_loudspeaker_layout_expanded_bottom_3ch,
        .reference_layout = ck_iamf_loudspeaker_layout_expanded_a293,
        // .decoding_map = {22, 23, 21},
        .decoding_map = {1, 2, 0},
    },
    {
        .id = ck_iamf_layout_id_expanded_7_1_5_4,
        .name = "7.1.5.4ch",
        .flags = ck_iamf_layout_flag_in | ck_iamf_layout_flag_out |
                 ck_iamf_layout_flag_expanded,
        .channels = 17,
        .surround = 7,
        .height = 9,
        .bottom = 4,
        .lfe1 = 3,
        .lfe2 = def_lfe_none,
        .sound_system = SOUND_SYSTEM_EXT_7154,
        .layout = ck_iamf_loudspeaker_layout_expanded_7154,
        .decoding_map = {0, 1, 4, 5, 6, 7, 8, 9, 11, 12, 13, 14, 15, 16, 2, 10,
                         3},
    },

    {
        .id = ck_iamf_layout_id_expanded_bottom_4ch,
        .name = "Bottom-4ch",
        .flags = ck_iamf_layout_flag_in | ck_iamf_layout_flag_expanded |
                 ck_iamf_layout_flag_subset,
        .channels = 4,
        .layout = ck_iamf_loudspeaker_layout_expanded_bottom_4ch,
        .reference_layout = ck_iamf_loudspeaker_layout_expanded_7154,
        // .decoding_map = {13, 14, 15, 16},
        .decoding_map = {0, 1, 2, 3},
    },
    {
        .id = ck_iamf_layout_id_expanded_top_1ch,
        .name = "Top-1ch",
        .flags = ck_iamf_layout_flag_in | ck_iamf_layout_flag_expanded |
                 ck_iamf_layout_flag_subset,
        .channels = 1,
        .layout = ck_iamf_loudspeaker_layout_expanded_top_1ch,
        .reference_layout = ck_iamf_loudspeaker_layout_expanded_7154,
        // .decoding_map = {10},
        .decoding_map = {0},
    },
    {
        .id = ck_iamf_layout_id_expanded_top_5ch,
        .name = "Top-5ch",
        .flags = ck_iamf_layout_flag_in | ck_iamf_layout_flag_expanded |
                 ck_iamf_layout_flag_subset,
        .channels = 5,
        .layout = ck_iamf_loudspeaker_layout_expanded_top_5ch,
        .reference_layout = ck_iamf_loudspeaker_layout_expanded_7154,
        // .decoding_map = {8, 9, 11, 12, 10},
        .decoding_map = {0, 1, 3, 4, 2},
    },
};

const iamf_layout_info_t *iamf_layout_get_info(iamf_layout_t layout) {
  const iamf_layout_info_t *info = 0;

  if (layout.type == ck_iamf_layout_type_loudspeakers_ss_convention) {
    iamf_sound_system_t ss = layout.sound_system;
    for (uint32_t i = 0; i < sizeof(iamf_layouts) / sizeof(iamf_layouts[0]);
         ++i) {
      if (iamf_layouts[i].sound_system == ss &&
          (iamf_layouts[i].flags & ck_iamf_layout_flag_out)) {
        info = &iamf_layouts[i];
        break;
      }
    }
  } else if (layout.type == ck_iamf_layout_type_binaural)
    info =
        iamf_loudspeaker_layout_get_info(ck_iamf_loudspeaker_layout_binaural);
  return info;
}

const iamf_layout_info_t *iamf_loudspeaker_layout_get_info(
    iamf_loudspeaker_layout_t type) {
  const iamf_layout_info_t *info = 0;
  for (uint32_t i = 0; i < sizeof(iamf_layouts) / sizeof(iamf_layouts[0]);
       ++i) {
    if (iamf_layouts[i].layout == type &&
        (iamf_layouts[i].flags & ck_iamf_layout_flag_in)) {
      info = &iamf_layouts[i];
      break;
    }
  }
  return info;
}

int iamf_loudspeaker_layout_get_decoding_channels(
    iamf_loudspeaker_layout_t type, iamf_channel_t *channels, uint32_t count) {
  const iamf_layout_info_t *info = iamf_loudspeaker_layout_get_info(type);
  for (uint32_t i = 0; i < info->channels; ++i)
    channels[i] = info->channel_layout[info->decoding_map[i]];
  return info->channels;
}

int iamf_loudspeaker_layout_get_rendering_channels(
    iamf_loudspeaker_layout_t type, iamf_channel_t *channels, uint32_t count) {
  const iamf_layout_info_t *info = iamf_loudspeaker_layout_get_info(type);
  if (count < info->channels) return IAMF_ERR_BUFFER_TOO_SMALL;
  for (uint32_t i = 0; i < info->channels; ++i)
    channels[i] = info->channel_layout[i];
  return info->channels;
}

int iamf_layout_channels_count(iamf_layout_t *layout) {
  int ret = 0;
  if (layout->type == ck_iamf_layout_type_loudspeakers_ss_convention) {
    const iamf_layout_info_t *info = iamf_layout_get_info(*layout);
    if (info) ret = info->channels;
  } else if (layout->type == ck_iamf_layout_type_binaural) {
    ret = 2;
  }

  return ret;
}

int iamf_layout_is_equal(iamf_layout_t a, iamf_layout_t b) {
  if (a.type != b.type) return def_false;

  switch (a.type) {
    case ck_iamf_layout_type_loudspeakers_ss_convention:
      return a.sound_system == b.sound_system;

    case ck_iamf_layout_type_binaural:
      return def_true;

    case ck_iamf_layout_type_not_defined:
    default:
      break;
  }
  return def_false;
}

static iamf_layout_t _get_sound_system_layout_instance(iamf_layout_t layout) {
  if (layout.type == ck_iamf_layout_type_binaural)
    return def_sound_system_layout_instance(SOUND_SYSTEM_A);
  else if (layout.type == ck_iamf_layout_type_loudspeakers_ss_convention)
    return layout;
  else
    return def_sound_system_layout_instance(SOUND_SYSTEM_NONE);
}

int iamf_layout_higher_check(iamf_layout_t layout1, iamf_layout_t layout2,
                             int equal) {
  iamf_layout_t ss_layout1 = _get_sound_system_layout_instance(layout1);
  iamf_layout_t ss_layout2 = _get_sound_system_layout_instance(layout2);
  const iamf_layout_info_t *info1 = iamf_layout_get_info(ss_layout1);
  const iamf_layout_info_t *info2 = iamf_layout_get_info(ss_layout2);

  if (!info1) return def_false;
  if (!info2) return def_true;
  if (!equal && iamf_layout_is_equal(ss_layout1, ss_layout2)) return def_false;

  // A higher layout x'.y'.z' means that x', y', and z' are greater than or
  // equal to x, y, and z
  return (info1->surround >= info2->surround) &&
         (info1->height >= info2->height) && (info1->bottom >= info2->bottom) &&
         (info1->channels >= info2->channels);
}
