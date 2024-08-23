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
 * @file IAMF_types.h
 * @brief Internal definition.
 * @version 0.1
 * @date Created 03/03/2023
 **/

#ifndef IAMF_TYPES_H_
#define IAMF_TYPES_H_

typedef enum {
  IA_CH_RE_L,
  IA_CH_RE_C,
  IA_CH_RE_R,
  IA_CH_RE_LS,
  IA_CH_RE_RS,
  IA_CH_RE_LTF,
  IA_CH_RE_RTF,
  IA_CH_RE_LB,
  IA_CH_RE_RB,
  IA_CH_RE_LTB,
  IA_CH_RE_RTB,
  IA_CH_RE_LFE,
  IA_CH_RE_COUNT,

  IA_CH_RE_RSS = IA_CH_RE_RS,
  IA_CH_RE_LSS = IA_CH_RE_LS,
  IA_CH_RE_RTR = IA_CH_RE_RTB,
  IA_CH_RE_LTR = IA_CH_RE_LTB,
  IA_CH_RE_RSR = IA_CH_RE_RB,
  IA_CH_RE_LRS = IA_CH_RE_LB,
} IAReconChannel;

typedef enum {
  IA_CH_INVALID,
  IA_CH_L7,
  IA_CH_R7,
  IA_CH_C,
  IA_CH_LFE,
  IA_CH_SL7,
  IA_CH_SR7,
  IA_CH_BL7,
  IA_CH_BR7,
  IA_CH_HFL,
  IA_CH_HFR,
  IA_CH_HBL,
  IA_CH_HBR,
  IA_CH_MONO,
  IA_CH_L2,
  IA_CH_R2,
  IA_CH_TL,
  IA_CH_TR,
  IA_CH_L3,
  IA_CH_R3,
  IA_CH_SL5,
  IA_CH_SR5,
  IA_CH_HL,
  IA_CH_HR,
  IA_CH_FL,     // M+060 in 9.1.6
  IA_CH_FR,     // M-060 in 9.1.6
  IA_CH_TPSIL,  // U+090
  IA_CH_TPSIR,  // U-090
  IA_CH_COUNT,

  IA_CH_L5 = IA_CH_L7,
  IA_CH_R5 = IA_CH_R7,

  IA_CH_FLC = IA_CH_L7,    // M+030
  IA_CH_FRC = IA_CH_R7,    // M-030
  IA_CH_FC = IA_CH_C,      // M+000
  IA_CH_SIL = IA_CH_SL7,   // M+090
  IA_CH_SIR = IA_CH_SR7,   // M-090
  IA_CH_BL = IA_CH_BL7,    // M+135
  IA_CH_BR = IA_CH_BR7,    // M-135
  IA_CH_TPFL = IA_CH_HFL,  // U+045
  IA_CH_TPFR = IA_CH_HFR,  // U-045
  IA_CH_TPBL = IA_CH_HBL,  // U+135
  IA_CH_TPBR = IA_CH_HBR,  // U-135
  IA_CH_LFE1 = IA_CH_LFE,
} IAChannel;

typedef enum {
  AUDIO_FRAME_PLANE = 0x1,
} AFlag;

typedef enum {
  STREAM_MODE_AMBISONICS_NONE,
  STREAM_MODE_AMBISONICS_MONO,
  STREAM_MODE_AMBISONICS_PROJECTION
} IAMF_Stream_Mode;

/**
 * @brief Expanded loudspeaker layout in @expanded_loudspeaker_layout
 */
typedef enum {
  IAMF_EXPANDED_LOUDSPEAKER_LAYOUT_LFE = 0,      // LFE
  IAMF_EXPANDED_LOUDSPEAKER_LAYOUT_STEREO_S,     // Ls/Rs
  IAMF_EXPANDED_LOUDSPEAKER_LAYOUT_STEREO_SS,    // Lss/Rss
  IAMF_EXPANDED_LOUDSPEAKER_LAYOUT_STEREO_RS,    // Lrs/Rrs
  IAMF_EXPANDED_LOUDSPEAKER_LAYOUT_STEREO_TF,    // Ltf/Rtf
  IAMF_EXPANDED_LOUDSPEAKER_LAYOUT_STEREO_TB,    // Ltb/Rtb
  IAMF_EXPANDED_LOUDSPEAKER_LAYOUT_TOP_4CH,      // Ltf/Rtf/Ltb/Rtb
  IAMF_EXPANDED_LOUDSPEAKER_LAYOUT_3CH,          // L/C/R
  IAMF_EXPANDED_LOUDSPEAKER_LAYOUT_916,          // 9.1.6
  IAMF_EXPANDED_LOUDSPEAKER_LAYOUT_STEREO_F,     // FL/FR
  IAMF_EXPANDED_LOUDSPEAKER_LAYOUT_STEREO_Si,    // SiL/SiR
  IAMF_EXPANDED_LOUDSPEAKER_LAYOUT_STEREO_TpSi,  // TpSiL/TpSiR
  IAMF_EXPANDED_LOUDSPEAKER_LAYOUT_TOP_6CH,  // TpFL/TpFR/TpSiL/TpSiR/TpBL/TpBR
  IAMF_EXPANDED_LOUDSPEAKER_LAYOUT_COUNT,
} IAMFExpandedLayoutType;

typedef struct MixFactors {
  float alpha;
  float beta;
  float gamma;
  float delta;
  int w_idx_offset;
} MixFactors;

/// @brief The IA profile.
typedef enum {
  /// @brief the simple profile.
  IAMF_PROFILE_SIMPLE,
  /// @brief the base profile.
  IAMF_PROFILE_BASE,
  /// @brief the base-enhanced profile.
  IAMF_PROFILE_BASE_ENHANCED,
  /// @brief the count of profiles.
  IAMF_PROFILE_COUNT,
  /// @brief the default profile.
  IAMF_PROFILE_DEFAULT = IAMF_PROFILE_BASE_ENHANCED,
} IAMF_Profile;

#define U8_MASK 0xFF
#define U16_MASK 0xFFFF

#define IA_CH_LAYOUT_MAX_CHANNELS 25

#define OPUS_FRAME_SIZE 960
#define MAX_OPUS_FRAME_SIZE OPUS_FRAME_SIZE * 6
#define AAC_FRAME_SIZE 1024
#define MAX_AAC_FRAME_SIZE 2048
#define MAX_FRAME_SIZE AAC_FRAME_SIZE * 6
#define MAX_FLAC_FRAME_SIZE 32768

#define MAX_STREAMS 255

#define IA_CH_CATE_SURROUND 0x100
#define IA_CH_CATE_WEIGHT 0X200
#define IA_CH_CATE_TOP 0X400

#define IAMF_LOUDSPEAKER_LAYOUT_EXPANDED 15
#define IAMF_LOUDSPEAKER_LAYOUT_EXPANDED_MASK 0x10

/// @brief The maximun number of sub mixes for the mix presentation.
#define IAMF_MIX_PRESENTATION_MAX_SUBS 1

#endif /* IAMF_TYPES_H_ */
