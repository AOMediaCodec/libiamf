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
 * @file IAMF_defines.h
 * @brief AMF Common defines
 * @version 2.0.0
 * @date Created 3/3/2023
 **/

#ifndef __IAMF_DEFINES_H__
#define __IAMF_DEFINES_H__

#include <stdint.h>

typedef enum IA_Profile {
  IA_PROFILE_NONE = -1,
  IA_PROFILE_SIMPLE,
  IA_PROFILE_BASE,
  IA_PROFILE_BASE_ENHANCED,
  IA_PROFILE_BASE_ADVANCED,
  IA_PROFILE_ADVANCED_1,
  IA_PROFILE_ADVANCED_2,
} IA_Profile;

/**
 * Audio Element Type
 * */

typedef enum IAMF_AudioElementType {
  AUDIO_ELEMENT_INVALID = -1,
  AUDIO_ELEMENT_CHANNEL_BASED,
  AUDIO_ELEMENT_SCENE_BASED,
  AUDIO_ELEMENT_OBJECT_BASED,
  AUDIO_ELEMENT_COUNT
} IAMF_AudioElementType;

typedef enum IAMF_LayoutType {
  IAMF_LAYOUT_TYPE_NOT_DEFINED = 0,
  IAMF_LAYOUT_TYPE_LOUDSPEAKERS_SS_CONVENTION = 2,
  IAMF_LAYOUT_TYPE_BINAURAL,
} IAMF_LayoutType;

typedef enum IAMF_SoundSystem {
  SOUND_SYSTEM_INVALID = -1,
  SOUND_SYSTEM_NONE = SOUND_SYSTEM_INVALID,
  SOUND_SYSTEM_A,         // 0+2+0, 0
  SOUND_SYSTEM_B,         // 0+5+0, 1
  SOUND_SYSTEM_C,         // 2+5+0, 1
  SOUND_SYSTEM_D,         // 4+5+0, 1
  SOUND_SYSTEM_E,         // 4+5+1, 1
  SOUND_SYSTEM_F,         // 3+7+0, 2
  SOUND_SYSTEM_G,         // 4+9+0, 1
  SOUND_SYSTEM_H,         // 9+10+3, 2
  SOUND_SYSTEM_I,         // 0+7+0, 1
  SOUND_SYSTEM_J,         // 4+7+0, 1
  SOUND_SYSTEM_EXT_712,   // 2+7+0, 1
  SOUND_SYSTEM_EXT_312,   // 2+3+0, 1
  SOUND_SYSTEM_MONO,      // 0+1+0, 0
  SOUND_SYSTEM_EXT_916,   // 6+9+0, 1
  SOUND_SYSTEM_EXT_7154,  // 5+7+4, 1
  SOUND_SYSTEM_END
} IAMF_SoundSystem;

/**
 * Codec ID
 * */
typedef enum IAMF_CodecID {
  IAMF_CODEC_UNKNOWN = 0,
  IAMF_CODEC_NONE = IAMF_CODEC_UNKNOWN,  // no codec
  IAMF_CODEC_OPUS,
  IAMF_CODEC_AAC,
  IAMF_CODEC_FLAC,
  IAMF_CODEC_PCM,
  IAMF_CODEC_COUNT
} IAMF_CodecID;

/**
 * Error codes.
 * */

enum {
  IAMF_OK = 0,
  IAMF_ERR_BAD_ARG = -1,
  IAMF_ERR_BUFFER_TOO_SMALL = -2,
  IAMF_ERR_INTERNAL = -3,
  IAMF_ERR_INVALID_PACKET = -4,
  IAMF_ERR_INVALID_STATE = -5,
  IAMF_ERR_UNIMPLEMENTED = -6,
  IAMF_ERR_ALLOC_FAIL = -7,
  IAMF_ERR_PENDING = -8,
};

/**
 * IA channel layout type.
 * */

typedef enum IAMF_LoudspeakerLayoutType {
  IA_CHANNEL_LAYOUT_INVALID = -1,
  IA_CHANNEL_LAYOUT_NONE = IA_CHANNEL_LAYOUT_INVALID,
  IA_CHANNEL_LAYOUT_MONO = 0,  // 1.0.0
  IA_CHANNEL_LAYOUT_STEREO,    // 2.0.0
  IA_CHANNEL_LAYOUT_510,       // 5.1.0
  IA_CHANNEL_LAYOUT_512,       // 5.1.2
  IA_CHANNEL_LAYOUT_514,       // 5.1.4
  IA_CHANNEL_LAYOUT_710,       // 7.1.0
  IA_CHANNEL_LAYOUT_712,       // 7.1.2
  IA_CHANNEL_LAYOUT_714,       // 7.1.4
  IA_CHANNEL_LAYOUT_312,       // 3.1.2
  IA_CHANNEL_LAYOUT_BINAURAL,  // binaural
  IA_CHANNEL_LAYOUT_COUNT,

  // expanded layout for version 1.1.
  /// @brief The low-frequency effects subset (LFE) of 7.1.4ch
  IA_CHANNEL_LAYOUT_EXPANDED_LFE = 0x10,
  /// @brief The surround subset (Ls/Rs) of 5.1.4ch
  IA_CHANNEL_LAYOUT_EXPANDED_STEREO_S,
  /// @brief The side surround subset (Lss/Rss) of 7.1.4ch
  IA_CHANNEL_LAYOUT_EXPANDED_STEREO_SS,
  /// @brief The rear surround subset (Lrs/Rrs) of 7.1.4ch
  IA_CHANNEL_LAYOUT_EXPANDED_STEREO_RS,
  /// @brief The top front subset (Ltf/Rtf) of 7.1.4ch
  IA_CHANNEL_LAYOUT_EXPANDED_STEREO_TF,
  /// @brief The top back subset (Ltb/Rtb) of 7.1.4ch
  IA_CHANNEL_LAYOUT_EXPANDED_STEREO_TB,
  /// @brief The top 4 channels (Ltf/Rtf/Ltb/Rtb) of 7.1.4ch
  IA_CHANNEL_LAYOUT_EXPANDED_TOP_4CH,
  /// @brief The front 3 channels (L/C/R) of 7.1.4ch
  IA_CHANNEL_LAYOUT_EXPANDED_3CH,
  /// @brief The subset of Loudspeaker configuration for Sound System H (9+10+3)
  ///        of [ITU-2051-3]
  IA_CHANNEL_LAYOUT_EXPANDED_916,
  /// @brief The front subset (FL/FR) of 9.1.6ch
  IA_CHANNEL_LAYOUT_EXPANDED_STEREO_F,
  /// @brief The side subset (SiL/SiR) of 9.1.6ch
  IA_CHANNEL_LAYOUT_EXPANDED_STEREO_SI,
  /// @brief The top side subset (TpSiL/TpSiR) of 9.1.6ch
  IA_CHANNEL_LAYOUT_EXPANDED_STEREO_TPSI,
  /// @brief The top 6 channels (TpFL/TpFR/TpSiL/TpSiR/TpBL/TpBR) of 9.1.6ch
  IA_CHANNEL_LAYOUT_EXPANDED_TOP_6CH,
  /// @brief Loudspeaker configuration for Sound System H (9+10+3) of
  ///        [ITU-2051-3]
  IA_CHANNEL_LAYOUT_EXPANDED_A293,
  /// @brief The low-frequency effects subset (LFE1/LFE2) of 10.2.9.3ch
  IA_CHANNEL_LAYOUT_EXPANDED_LFE_PAIR,
  /// @brief The bottom 3 channels (BtFL/BtFC/BtFR) of 10.2.9.3ch
  IA_CHANNEL_LAYOUT_EXPANDED_BOTTOM_3CH,
  /// @brief Loudspeaker configuration with the top and the bottom speakers
  ///        added to Loudspeaker configuration for Sound System J (4+7+0) of
  ///        [ITU-2051-3]
  IA_CHANNEL_LAYOUT_EXPANDED_7154,
  /// @brief The bottom 4 channels (BtFL/BtFR/BtBL/BtBR) of 7.1.5.4ch
  IA_CHANNEL_LAYOUT_EXPANDED_BOTTOM_4CH,
  /// @brief The top subset (TpC) of 7.1.5.4ch
  IA_CHANNEL_LAYOUT_EXPANDED_TOP_1CH,
  /// @brief The top 5 channels (Ltf/Rtf/TpC/Ltb/Rtb) of 7.1.5.4ch
  IA_CHANNEL_LAYOUT_EXPANDED_TOP_5CH,
  /// @brief The end of expanded loudspeaker layout
  IA_CHANNEL_LAYOUT_EXPANDED_END,
} IAChannelLayoutType;

typedef enum IAMF_SamplingRate {
  SAMPLING_RATE_NONE = 0,
  SAMPLING_RATE_7350 = 7350,
  SAMPLING_RATE_8000 = 8000,
  SAMPLING_RATE_11025 = 11025,
  SAMPLING_RATE_16000 = 16000,
  SAMPLING_RATE_22050 = 22050,
  SAMPLING_RATE_24000 = 24000,
  SAMPLING_RATE_32000 = 32000,
  SAMPLING_RATE_44100 = 44100,
  SAMPLING_RATE_48000 = 48000,
  SAMPLING_RATE_64000 = 64000,
  SAMPLING_RATE_88200 = 88200,
  SAMPLING_RATE_96000 = 96000,
  SAMPLING_RATE_176400 = 176400,
  SAMPLING_RATE_192000 = 192000,
} IAMF_SamplingRate;

typedef enum IAMF_SampleBitDepth {
  SAMPLE_BIT_DEPTH_NONE = 0,
  SAMPLE_BIT_DEPTH_16 = 16,
  SAMPLE_BIT_DEPTH_24 = 24,
  SAMPLE_BIT_DEPTH_32 = 32,
} IAMF_SampleBitDepth;

typedef enum IAMF_HeadphonesRenderingMode {
  HEADPHONES_RENDERING_MODE_WORLD_LOCKED_RESTRICTED = 0,
  HEADPHONES_RENDERING_MODE_WORLD_LOCKED,
  HEADPHONES_RENDERING_MODE_HEAD_LOCKED,
  HEADPHONES_RENDERING_MODE_RESERVED,
} IAMF_HeadphonesRenderingMode;

typedef enum IAMF_BinauralFilterProfile {
  BINAURAL_FILTER_PROFILE_AMBIENT = 0,
  BINAURAL_FILTER_PROFILE_DIRECT,
  BINAURAL_FILTER_PROFILE_REVERBERANT,
  BINAURAL_FILTER_PROFILE_RESERVED,
} IAMF_BinauralFilterProfile;

#endif /* __IAMF_DEFINES_H__ */
