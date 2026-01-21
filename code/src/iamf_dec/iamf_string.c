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
 * @file iamf_string.c
 * @brief IAMF string implementation.
 * @version 2.0.0
 * @date Created 12/11/2025
 **/

#include "iamf_string.h"

#include "iamf_private_definitions.h"
#include "iamf_utils.h"

static const char* _g_error_code_strings[] = {"Ok",
                                              "Bad argments",
                                              "Buffer too small",
                                              "Internal error",
                                              "Invalid packet",
                                              "Invalid state",
                                              "Unimplemented",
                                              "Memory allocation failure"
                                              "Pending"};

const char* iamf_error_code_string(int errno_) {
  int cnt = sizeof(_g_error_code_strings) / sizeof(char*);
  int idx = -errno_;
  if (idx >= 0 && idx < cnt) {
    return _g_error_code_strings[idx];
  }
  return "None";
}

static const char* _g_profile_type_strings[] = {
    "Simple Profile",        "Base Profile",       "Base Enhanced Profile",
    "Base Advanced Profile", "Advanced Profile 1", "Advanced Profile 2"};

const char* iamf_profile_type_string(iamf_profile_t profile) {
  return profile > ck_iamf_profile_none && profile < def_iamf_profile_count
             ? _g_profile_type_strings[profile]
             : "None";
}

static const char* _g_obu_type_string[] = {
    "Codec Config",     "Audio Element",      "Mix Presentation",
    "Parameter Block",  "Temporal Delimiter", "Audio Frame",
    "Audio Frame ID0",  "Audio Frame ID1",    "Audio Frame ID2",
    "Audio Frame ID3",  "Audio Frame ID4",    "Audio Frame ID5",
    "Audio Frame ID6",  "Audio Frame ID7",    "Audio Frame ID8",
    "Audio Frame ID9",  "Audio Frame ID10",   "Audio Frame ID11",
    "Audio Frame ID12", "Audio Frame ID13",   "Audio Frame ID14",
    "Audio Frame ID15", "Audio Frame ID16",   "Audio Frame ID17",
    "Metadata",         "Reserved",           "Reserved",
    "Reserved",         "Reserved",           "Reserved",
    "Reserved",         "IA Sequence Header"};

const char* iamf_obu_type_string(iamf_obu_type_t type) {
  return type > ck_iamf_obu_none && type < ck_iamf_obu_count
             ? _g_obu_type_string[type]
             : "None";
}

const char* iamf_obu_header_flag2_string(iamf_obu_type_t type) {
  if (type == ck_iamf_obu_mix_presentation) {
    return "optional_fields_flag";
  } else if (type == ck_iamf_obu_temporal_delimiter) {
    return "is_not_key_frame";
  } else if ((type > ck_iamf_obu_temporal_delimiter &&
              type < ck_iamf_obu_metadata) ||
             (type >= ck_iamf_obu_audio_frame &&
              type <= ck_iamf_obu_audio_frame_id17)) {
    return "obu_trimming_status_flag";
  } else {
    return "reserved";
  }
}

static const char* _g_iamf_codec_type_strings[] = {"None", "OPUS", "AAC-LC",
                                                   "FLAC", "LPCM"};

const char* iamf_codec_type_string(iamf_codec_type_t type) {
  if (iamf_codec_type_check(type)) {
    return _g_iamf_codec_type_strings[type];
  }
  return "None";
}

static const char* _g_audio_element_type_strings[] = {
    "Channel Based", "Scene Based", "Object Based"};

const char* iamf_audio_element_type_string(iamf_audio_element_type_t type) {
  return type > ck_audio_element_type_none && type < ck_audio_element_type_count
             ? _g_audio_element_type_strings[type]
             : "None";
}

static const char* _g_parameter_block_type_strings[] = {
    "Mix Gain",         "Demixing",          "Recon Gain", "Polar",
    "Cartesian8",       "Cartesian16",       "Dual Polar", "Dual Cartesian8",
    "Dual Cartesian16", "Momentary Loudness"};
const char* iamf_parameter_block_type_string(iamf_parameter_type_t type) {
  return type > ck_iamf_parameter_type_none &&
                 type < ck_iamf_parameter_type_count
             ? _g_parameter_block_type_strings[type]
             : "None";
}

static const char* _g_ambisonics_mode_strings[] = {
    "Ambisonics Mono Mode", "Ambisonics Projection Mode"};

const char* iamf_ambisonics_mode_string(iamf_ambisonics_mode_t mode) {
  return mode > ck_ambisonics_mode_none && mode < ck_ambisonics_mode_count
             ? _g_ambisonics_mode_strings[mode]
             : "None";
}

static const char* _g_sound_system_strings[] = {
    "Sound System A (0+2+0)",
    "Sound System B (0+5+0)",
    "Sound System C (2+5+0)",
    "Sound System D (4+5+0)",
    "Sound System E (4+5+1)",
    "Sound System F (3+7+0)",
    "Sound System G (4+9+0)",
    "Sound System H (9+10+3)",
    "Sound System I (0+7+0)",
    "Sound System J (4+7+0)",
    "Sound System Ext. 712 (2+7+0)",
    "Sound System Ext. 312 (2+3+0)",
    "Mono",
    "Sound System Ext. 916 (6+9+0)",
    "Sound System Ext. 7154 (5+7+4)"};

const char* iamf_sound_system_string(iamf_sound_system_t sound_system) {
  return sound_system > SOUND_SYSTEM_NONE && sound_system < SOUND_SYSTEM_END
             ? _g_sound_system_strings[sound_system]
             : "None";
}

static const char* _g_loudspeaker_layout_strings[] = {
    "Mono",  "Stereo",  "5.1ch",   "5.1.2ch", "5.1.4ch",
    "7.1ch", "7.1.2ch", "7.1.4ch", "3.1.2ch", "Binaural"};

const char* iamf_loudspeaker_layout_string(iamf_loudspeaker_layout_t layout) {
  if (iamf_audio_layer_base_layout_check(layout))
    return _g_loudspeaker_layout_strings[layout];
  if (layout == def_iamf_loudspeaker_layout_expanded) return "Expanded";
  return "None";
}

static const char* _g_expanded_loudspeaker_layout_strings[] = {
    "LFE",        "Stereo-S",    "Stereo-SS",  "Stereo-RS",  "Stereo-TF",
    "Stereo-TB",  "Top-4ch",     "3.0ch",      "9.1.6ch",    "Stereo-F",
    "Stereo-Si",  "Stereo-TpSi", "Top-6ch",    "10.2.9.3ch", "LFE-Pair",
    "Bottom-3ch", "7.1.5.4ch",   "Bottom-4ch", "Top-1ch",    "Top-5ch"};

const char* iamf_expanded_loudspeaker_layout_string(
    iamf_expanded_loudspeaker_layout_t layout) {
  return iamf_audio_layer_expanded_layout_check(layout)
             ? _g_expanded_loudspeaker_layout_strings[layout]
             : "None";
}

static const char* _g_layout_type_strings[] = {
    "Not Defined", "Reserved", "Loudspeakers SS Convention", "Binaural"};

const char* iamf_layout_type_string(iamf_layout_type_t type) {
  return type >= ck_iamf_layout_type_not_defined &&
                 type <= ck_iamf_layout_type_binaural
             ? _g_layout_type_strings[type]
             : "None";
}

static const char* _g_channel_strings[] = {
    "none",   "l7/l5/l", "r7/r5/r", "c",   "lfe", "sl7/sl", "sr7/sr",
    "bl7/bl", "br7/br",  "hfl",     "hfr", "hbl", "hbr",    "mono",
    "l2",     "r2",      "tl",      "tr",  "l3",  "r3",     "sl5",
    "sr5",    "hl",      "hr",      "wl",  "wr",  "hsl",    "hsr"};

const char* iamf_channel_name(iamf_channel_t ch) {
  return ch < ck_iamf_channel_count ? _g_channel_strings[ch] : "none";
}

const char* iamf_layout_string(iamf_layout_t layout) {
  switch (layout.type) {
    case ck_iamf_layout_type_loudspeakers_ss_convention:
      return iamf_sound_system_string((iamf_sound_system_t)layout.sound_system);
    case ck_iamf_layout_type_binaural:
      return "Binaural";
    default:
      return "None";
  }
}

static const char* _g_binaural_filter_profile_strings[] = {
    "Ambient", "Direct", "Reverberant", "Reserved"};

const char* iamf_binaural_filter_profile_string(
    IAMF_BinauralFilterProfile profile) {
  return profile >= BINAURAL_FILTER_PROFILE_AMBIENT &&
                 profile <= BINAURAL_FILTER_PROFILE_RESERVED
             ? _g_binaural_filter_profile_strings[profile]
             : "None";
}

static const char* _g_headphones_rendering_mode_strings[] = {
    "World Locked Restricted", "World Locked", "Head Locked", "Reserved"};

const char* iamf_headphones_rendering_mode_string(
    IAMF_HeadphonesRenderingMode mode) {
  return mode >= HEADPHONES_RENDERING_MODE_WORLD_LOCKED_RESTRICTED &&
                 mode <= HEADPHONES_RENDERING_MODE_RESERVED
             ? _g_headphones_rendering_mode_strings[mode]
             : "None";
}

static const char* _g_metadata_type_strings[] = {"Reserved",
                                                 "ITU-T T.35",
                                                 "IAMF Tags",
                                                 "Reserved",
                                                 "Unregistered User Private",
                                                 "Reserved Future"};

const char* iamf_metadata_type_string(iamf_metadata_type_t type) {
  switch (type) {
    case ck_iamf_metadata_type_reserved_0:
    case ck_iamf_metadata_type_itut_t35:
    case ck_iamf_metadata_type_iamf_tags:
      return _g_metadata_type_strings[type];
    default:
      if (type >= ck_iamf_metadata_type_reserved_start &&
          type <= ck_iamf_metadata_type_reserved_end) {
        return _g_metadata_type_strings[3];
      } else if (type >=
                     ck_iamf_metadata_type_unregistered_user_private_start &&
                 type <= ck_iamf_metadata_type_unregistered_user_private_end) {
        return _g_metadata_type_strings[4];
      } else if (type >= ck_iamf_metadata_type_reserved_future_start) {
        return _g_metadata_type_strings[5];
      }
      return "Unknown";
  }
}

static const char* _g_anchor_element_strings[] = {"Unknown",   // 0
                                                  "Dialogue",  // 1
                                                  "Album",     // 2
                                                  "Reserved for future use"};

const char* iamf_anchor_element_string(uint32_t anchor_element) {
  if (anchor_element > 2) anchor_element = 3;
  return _g_anchor_element_strings[anchor_element];
}

const char* iamf_loudness_info_type_string(uint32_t type) {
  static char buffer[256];
  buffer[0] = '\0';

  if (type == 0) return "None";

  int len = 0;
  const char* separator = "";

  if (type & def_loudness_info_type_true_peak) {
    len +=
        snprintf(buffer + len, sizeof(buffer) - len, "%sTrue Peak", separator);
    separator = " | ";
  }

  if (type & def_loudness_info_type_anchored) {
    len +=
        snprintf(buffer + len, sizeof(buffer) - len, "%sAnchored", separator);
    separator = " | ";
  }

  if (type & def_loudness_info_type_live) {
    len += snprintf(buffer + len, sizeof(buffer) - len, "%sLive", separator);
    separator = " | ";
  }

  if (type & def_loudness_info_type_momentary) {
    len +=
        snprintf(buffer + len, sizeof(buffer) - len, "%sMomentary", separator);
    separator = " | ";
  }

  if (type & def_loudness_info_type_range) {
    len += snprintf(buffer + len, sizeof(buffer) - len, "%sRange", separator);
    separator = " | ";
  }

  if (type & ~def_loudness_info_type_all) {
    snprintf(buffer + len, sizeof(buffer) - len, "%sUnknown", separator);
  }

  return buffer[0] ? buffer : "None";
}

static const char* _g_animation_type_strings[] = {
    "Step",          // ck_iamf_animation_type_step
    "Linear",        // ck_iamf_animation_type_linear
    "Bezier",        // ck_iamf_animation_type_bezier
    "Inter Linear",  // ck_iamf_animation_type_inter_linear
    "Inter Bezier"   // ck_iamf_animation_type_inter_bezier
};

const char* iamf_animation_type_string(iamf_animation_type_t type) {
  int count = sizeof(_g_animation_type_strings) / sizeof(char*);
  if (type >= 0 && type < count) {
    return _g_animation_type_strings[type];
  }
  return "Unknown";
}
