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
 * @file IAMF_utils.c
 * @brief Utils.
 * @version 0.1
 * @date Created 03/03/2023
 **/

#include "IAMF_utils.h"

void iamf_freep(void **p) {
  if (p && *p) {
    free(*p);
    *p = 0;
  }
}

#define TAG(a, b, c, d) ((a) | (b) << 8 | (c) << 16 | (d) << 24)
IAMF_CodecID iamf_codec_4cc_get_codecID(uint32_t id) {
  switch (id) {
    case TAG('m', 'p', '4', 'a'):
      return IAMF_CODEC_AAC;

    case TAG('O', 'p', 'u', 's'):
      return IAMF_CODEC_OPUS;

    case TAG('f', 'L', 'a', 'C'):
      return IAMF_CODEC_FLAC;

    case TAG('i', 'p', 'c', 'm'):
      return IAMF_CODEC_PCM;

    default:
      return IAMF_CODEC_UNKNOWN;
  }
}

int iamf_codec_check(IAMF_CodecID cid) {
  return cid >= IAMF_CODEC_OPUS && cid < IAMF_CODEC_COUNT;
}

static const char *gIAMFCodecName[IAMF_CODEC_COUNT] = {"None", "OPUS", "AAC-LC",
                                                       "FLAC", "PCM"};

const char *iamf_codec_name(IAMF_CodecID cid) {
  if (iamf_codec_check(cid)) {
    return gIAMFCodecName[cid];
  }
  return "UNKNOWN";
}

static const char *gIAECString[] = {"Ok",
                                    "Bad argments",
                                    "Unknown",
                                    "Internal error",
                                    "Invalid packet",
                                    "Invalid state",
                                    "Unimplemented",
                                    "Memory allocation failure"};

const char *ia_error_code_string(int ec) {
  int cnt = sizeof(gIAECString) / sizeof(char *);
  int idx = -ec;
  if (idx >= 0 && idx < cnt) {
    return gIAECString[idx];
  }
  return "Unknown";
}

int iamf_audio_layer_base_layout_check(IAChannelLayoutType type) {
  return type >= IA_CHANNEL_LAYOUT_MONO && type < IA_CHANNEL_LAYOUT_COUNT;
}

int iamf_audio_layer_expanded_layout_check(IAMFExpandedLayoutType type) {
  return type >= IAMF_EXPANDED_LOUDSPEAKER_LAYOUT_LFE &&
         type < IAMF_EXPANDED_LOUDSPEAKER_LAYOUT_COUNT;
}

IAChannelLayoutType iamf_audio_layer_layout_get(
    IAChannelLayoutType base, IAMFExpandedLayoutType expanded) {
  if (iamf_audio_layer_base_layout_check(base)) return base;
  if (iamf_audio_layer_expanded_layout_check(expanded) &&
      base == IAMF_LOUDSPEAKER_LAYOUT_EXPANDED)
    return IAMF_LOUDSPEAKER_LAYOUT_EXPANDED_MASK | expanded;
  return IA_CHANNEL_LAYOUT_INVALID;
}

static const char *gIAChName[] = {
    "unknown", "l7/l5/l", "r7/r5/r", "c",   "lfe", "sl7/sl", "sr7/sr",
    "bl7/bl",  "br7/br",  "hfl",     "hfr", "hbl", "hbr",    "mono",
    "l2",      "r2",      "tl",      "tr",  "l3",  "r3",     "sl5",
    "sr5",     "hl",      "hr",      "wl",  "wr",  "hsl",    "hsr"};

const char *ia_channel_name(IAChannel ch) {
  return ch < IA_CH_COUNT ? gIAChName[ch] : "unknown";
}

int bit1_count(uint32_t value) {
  int n = 0;
  for (; value; ++n) {
    value &= (value - 1);
  }
  return n;
}

int iamf_valid_mix_mode(int mode) { return mode >= 0 && mode != 3 && mode < 7; }

const MixFactors mix_factors_mat[] = {
    {1.0, 1.0, 0.707, 0.707, -1},   {0.707, 0.707, 0.707, 0.707, -1},
    {1.0, 0.866, 0.866, 0.866, -1}, {0, 0, 0, 0, 0},
    {1.0, 1.0, 0.707, 0.707, 1},    {0.707, 0.707, 0.707, 0.707, 1},
    {1.0, 0.866, 0.866, 0.866, 1},  {0, 0, 0, 0, 0}};

const MixFactors *iamf_get_mix_factors(int mode) {
  if (iamf_valid_mix_mode(mode)) return &mix_factors_mat[mode];
  return 0;
}
