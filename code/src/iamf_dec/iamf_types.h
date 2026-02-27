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
 * @file iamf_types.h
 * @brief Internal definition.
 * @version 0.1
 * @date Created 03/03/2023
 **/

#ifndef __IAMF_TYPES_H__
#define __IAMF_TYPES_H__

#include "IAMF_defines.h"

typedef enum EIamfCodecId {
  ck_iamf_codec_id_none = 0,
  ck_iamf_codec_id_opus = 0x7375704f,  // 'Opus'
  ck_iamf_codec_id_aac = 0x6134706d,   // 'mp4a' (AAC)
  ck_iamf_codec_id_flac = 0x43614c66,  // 'fLaC' (FLAC)
  ck_iamf_codec_id_lpcm = 0x6d637069,  // 'ipcm' (LPCM)
} iamf_codec_id_t;

typedef enum EIamfCodecType {
  ck_iamf_codec_type_none,
  ck_iamf_codec_type_opus,
  ck_iamf_codec_type_aac,
  ck_iamf_codec_type_flac,
  ck_iamf_codec_type_lpcm,
} iamf_codec_type_t;

typedef enum EIamfReconChannel {
  ck_iamf_channel_recon_l,
  ck_iamf_channel_recon_c,
  ck_iamf_channel_recon_r,
  ck_iamf_channel_recon_ls,
  ck_iamf_channel_recon_rs,
  ck_iamf_channel_recon_ltf,
  ck_iamf_channel_recon_rtf,
  ck_iamf_channel_recon_lb,
  ck_iamf_channel_recon_rb,
  ck_iamf_channel_recon_ltb,
  ck_iamf_channel_recon_rtb,
  ck_iamf_channel_recon_lfe,
  ck_iamf_channel_recon_count,

  ck_iamf_channel_recon_rss = ck_iamf_channel_recon_rs,
  ck_iamf_channel_recon_lss = ck_iamf_channel_recon_ls,
  ck_iamf_channel_recon_rtr = ck_iamf_channel_recon_rtb,
  ck_iamf_channel_recon_ltr = ck_iamf_channel_recon_ltb,
  ck_iamf_channel_recon_rsr = ck_iamf_channel_recon_rb,
  ck_iamf_channel_recon_lrs = ck_iamf_channel_recon_lb,
} iamf_recon_channel_t;

typedef enum EIamfChannel {
  /// @brief None channel
  ck_iamf_channel_none,
  /// @brief Left 7.1 channel
  ck_iamf_channel_l7,
  /// @brief Right 7.1 channel
  ck_iamf_channel_r7,
  /// @brief Center channel
  ck_iamf_channel_c,
  /// @brief Low frequency effects channel
  ck_iamf_channel_lfe,
  /// @brief Side left 7.1 channel
  ck_iamf_channel_sl7,
  /// @brief Side right 7.1 channel
  ck_iamf_channel_sr7,
  /// @brief Back left 7.1 channel
  ck_iamf_channel_bl7,
  /// @brief Back right 7.1 channel
  ck_iamf_channel_br7,
  /// @brief High front left channel
  ck_iamf_channel_hfl,
  /// @brief High front right channel
  ck_iamf_channel_hfr,
  /// @brief High back left channel
  ck_iamf_channel_hbl,
  /// @brief High back right channel
  ck_iamf_channel_hbr,
  /// @brief Mono channel
  ck_iamf_channel_mono,
  /// @brief Left 2.0 channel
  ck_iamf_channel_l2,
  /// @brief Right 2.0 channel
  ck_iamf_channel_r2,
  /// @brief Top left channel
  ck_iamf_channel_tl,
  /// @brief Top right channel
  ck_iamf_channel_tr,
  /// @brief Left 3.0 channel
  ck_iamf_channel_l3,
  /// @brief Right 3.0 channel
  ck_iamf_channel_r3,
  /// @brief Side left 5.1 channel
  ck_iamf_channel_sl5,
  /// @brief Side right 5.1 channel
  ck_iamf_channel_sr5,
  /// @brief High left channel
  ck_iamf_channel_hl,
  /// @brief High right channel
  ck_iamf_channel_hr,
  /// @brief Channel count
  ck_iamf_channel_count,

  /// @brief Left 5.1 channel (alias for left 7.1)
  ck_iamf_channel_l5 = ck_iamf_channel_l7,
  /// @brief Right 5.1 channel (alias for right 7.1)
  ck_iamf_channel_r5 = ck_iamf_channel_r7,
} iamf_channel_t;

typedef enum EIamfAudioFrameFlag {
  ck_audio_frame_planar = 0x1,
} iamf_audio_frame_flag;

typedef enum EStreamMode {
  ck_stream_mode_ambisonics_none,
  ck_stream_mode_ambisonics_mono,
  ck_stream_mode_ambisonics_projection
} stream_mode_t;

typedef enum EIamfLoudspeakerLayout {
  ck_iamf_loudspeaker_layout_none = -1,
  ck_iamf_loudspeaker_layout_mono = 0,  // 1.0.0
  ck_iamf_loudspeaker_layout_stereo,    // 2.0.0
  ck_iamf_loudspeaker_layout_510,       // 5.1.0
  ck_iamf_loudspeaker_layout_512,       // 5.1.2
  ck_iamf_loudspeaker_layout_514,       // 5.1.4
  ck_iamf_loudspeaker_layout_710,       // 7.1.0
  ck_iamf_loudspeaker_layout_712,       // 7.1.2
  ck_iamf_loudspeaker_layout_714,       // 7.1.4
  ck_iamf_loudspeaker_layout_312,       // 3.1.2
  ck_iamf_loudspeaker_layout_binaural,  // binaural
  ck_iamf_loudspeaker_layout_count,

  // expanded layout for version 1.1.
  /// @brief The low-frequency effects subset (LFE) of 7.1.4ch
  ck_iamf_loudspeaker_layout_expanded_lfe = 0x10,
  /// @brief The surround subset (Ls/Rs) of 5.1.4ch
  ck_iamf_loudspeaker_layout_expanded_stereo_s,
  /// @brief The side surround subset (Lss/Rss) of 7.1.4ch
  ck_iamf_loudspeaker_layout_expanded_stereo_ss,
  /// @brief The rear surround subset (Lrs/Rrs) of 7.1.4ch
  ck_iamf_loudspeaker_layout_expanded_stereo_rs,
  /// @brief The top front subset (Ltf/Rtf) of 7.1.4ch
  ck_iamf_loudspeaker_layout_expanded_stereo_tf,
  /// @brief The top back subset (Ltb/Rtb) of 7.1.4ch
  ck_iamf_loudspeaker_layout_expanded_stereo_tb,
  /// @brief The top 4 channels (Ltf/Rtf/Ltb/Rtb) of 7.1.4ch
  ck_iamf_loudspeaker_layout_expanded_top_4ch,
  /// @brief The front 3 channels (L/C/R) of 7.1.4ch
  ck_iamf_loudspeaker_layout_expanded_3ch,
  /// @brief The subset of Loudspeaker configuration for Sound System H (9+10+3)
  ///        of [ITU-2051-3]
  ck_iamf_loudspeaker_layout_expanded_916,
  /// @brief The front subset (FL/FR) of 9.1.6ch
  ck_iamf_loudspeaker_layout_expanded_stereo_f,
  /// @brief The side subset (SiL/SiR) of 9.1.6ch
  ck_iamf_loudspeaker_layout_expanded_stereo_si,
  /// @brief The top side subset (TpSiL/TpSiR) of 9.1.6ch
  ck_iamf_loudspeaker_layout_expanded_stereo_tpsi,
  /// @brief The top 6 channels (TpFL/TpFR/TpSiL/TpSiR/TpBL/TpBR) of 9.1.6ch
  ck_iamf_loudspeaker_layout_expanded_top_6ch,
  /// @brief Loudspeaker configuration for Sound System H (9+10+3) of
  ///        [ITU-2051-3]
  ck_iamf_loudspeaker_layout_expanded_a293,
  /// @brief The low-frequency effects subset (LFE1/LFE2) of 10.2.9.3ch
  ck_iamf_loudspeaker_layout_expanded_lfe_pair,
  /// @brief The bottom 3 channels (BtFL/BtFC/BtFR) of 10.2.9.3ch
  ck_iamf_loudspeaker_layout_expanded_bottom_3ch,
  /// @brief Loudspeaker configuration with the top and the bottom speakers
  ///        added to Loudspeaker configuration for Sound System J (4+7+0) of
  ///        [ITU-2051-3]
  ck_iamf_loudspeaker_layout_expanded_7154,
  /// @brief The bottom 4 channels (BtFL/BtFR/BtBL/BtBR) of 7.1.5.4ch
  ck_iamf_loudspeaker_layout_expanded_bottom_4ch,
  /// @brief The top subset (TpC) of 7.1.5.4ch
  ck_iamf_loudspeaker_layout_expanded_top_1ch,
  /// @brief The top 5 channels (Ltf/Rtf/TpC/Ltb/Rtb) of 7.1.5.4ch
  ck_iamf_loudspeaker_layout_expanded_top_5ch,
  /// @brief The end of expanded loudspeaker layout
  ck_iamf_loudspeaker_layout_expanded_end,
} iamf_loudspeaker_layout_t;

/**
 * @brief Expanded loudspeaker layout in @expanded_loudspeaker_layout
 */
typedef enum EIamfExpandedLoudspeakerLayout {
  ck_iamf_expanded_loudspeaker_layout_lfe = 0,      // LFE
  ck_iamf_expanded_loudspeaker_layout_stereo_s,     // Ls/Rs
  ck_iamf_expanded_loudspeaker_layout_stereo_ss,    // Lss/Rss
  ck_iamf_expanded_loudspeaker_layout_stereo_rs,    // Lrs/Rrs
  ck_iamf_expanded_loudspeaker_layout_stereo_tf,    // Ltf/Rtf
  ck_iamf_expanded_loudspeaker_layout_stereo_tb,    // Ltb/Rtb
  ck_iamf_expanded_loudspeaker_layout_top_4ch,      // Ltf/Rtf/Ltb/Rtb
  ck_iamf_expanded_loudspeaker_layout_3ch,          // L/C/R
  ck_iamf_expanded_loudspeaker_layout_916,          // 9.1.6
  ck_iamf_expanded_loudspeaker_layout_stereo_f,     // FL/FR
  ck_iamf_expanded_loudspeaker_layout_stereo_si,    // SiL/SiR
  ck_iamf_expanded_loudspeaker_layout_stereo_tpsi,  // TpSiL/TpSiR
  ck_iamf_expanded_loudspeaker_layout_top_6ch,  // TpFL/TpFR/TpSiL/TpSiR/TpBL/TpBR
  ck_iamf_expanded_loudspeaker_layout_a293,     // Loudspeaker configuration for
                                                // Sound System H (9+10+3) of
                                                // [ITU-2051-3]
  ck_iamf_expanded_loudspeaker_layout_lfe_pair,    // LFE1/LFE2 of 10.2.9.3ch
  ck_iamf_expanded_loudspeaker_layout_bottom_3ch,  // BtFL/BtFC/BtFR
                                                   // of 10.2.9.3ch
  ck_iamf_expanded_loudspeaker_layout_7154,  // Loudspeaker configuration with
                                             // top and bottom speakers added to
                                             // Sound System J (4+7+0) of
                                             // [ITU-2051-3]
  ck_iamf_expanded_loudspeaker_layout_bottom_4ch,  // BtFL/BtFR/BtBL/BtBR
                                                   // of 7.1.5.4ch
  ck_iamf_expanded_loudspeaker_layout_top_1ch,     // TpC of 7.1.5.4ch
  ck_iamf_expanded_loudspeaker_layout_top_5ch,     // Ltf/Rtf/TpC/Ltb/Rtb
                                                   // of 7.1.5.4ch
  ck_iamf_expanded_loudspeaker_layout_count,
} iamf_expanded_loudspeaker_layout_t;

typedef enum EIamfOutputGainChannel {
  ck_iamf_channel_gain_rtf,
  ck_iamf_channel_gain_ltf,
  ck_iamf_channel_gain_rs,
  ck_iamf_channel_gain_ls,
  ck_iamf_channel_gain_r,
  ck_iamf_channel_gain_l,
  ck_iamf_channel_gain_count
} iamf_output_gain_channel_t;

typedef enum EElementGainOffsetType {
  ck_element_gain_offset_type_value = 0,
  ck_element_gain_offset_type_range,
} element_gain_offset_type_t;

typedef struct MixFactors {
  float alpha;
  float beta;
  float gamma;
  float delta;
  int w_idx_offset;
} mix_factors_t;

typedef struct AudioCodecParameter {
  iamf_codec_type_t type;

  uint32_t sample_rate;
  union {
    uint32_t sample_format;
    struct {
      uint8_t sample_type;
      uint8_t bits_per_sample;
      uint8_t big_endian;
      uint8_t interleaved;
    };
  };
  uint32_t frame_size;
} audio_codec_parameter_t;

typedef enum EIamfLayoutType {
  ck_iamf_layout_type_not_defined = 0,
  ck_iamf_layout_type_loudspeakers_ss_convention = 2,
  ck_iamf_layout_type_binaural,
} iamf_layout_type_t;

typedef struct IamfLayout {
  iamf_layout_type_t type;
  union {
    int sound_system;
  };
} iamf_layout_t;

typedef struct {
  uint32_t azimuth;
  uint32_t elevation;
  uint32_t distance;
} encoded_polar_t;

typedef struct {
  uint32_t x;
  uint32_t y;
  uint32_t z;
} encoded_cartesian_t;

typedef IAMF_SoundSystem iamf_sound_system_t;
typedef IAMF_SamplingRate iamf_sampling_rate_t;
typedef IAMF_SampleBitDepth iamf_sample_bit_depth_t;

#endif /* __IAMF_TYPES_H__ */
