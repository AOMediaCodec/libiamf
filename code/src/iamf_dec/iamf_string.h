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
 * @file iamf_string.h
 * @brief IAMF string APIs.
 * @version 2.0.0
 * @date Created 12/11/2025
 **/

#ifndef __IAMF_STRING_H__
#define __IAMF_STRING_H__

#include "iamf_obu_all.h"

const char *iamf_error_code_string(int errno_);
const char *iamf_profile_type_string(iamf_profile_t profile);
const char *iamf_obu_type_string(iamf_obu_type_t type);
const char *iamf_obu_header_flag2_string(iamf_obu_type_t type);
const char *iamf_codec_type_string(iamf_codec_type_t type);
const char *iamf_audio_element_type_string(iamf_audio_element_type_t type);
const char *iamf_parameter_block_type_string(iamf_parameter_type_t type);
const char *iamf_ambisonics_mode_string(iamf_ambisonics_mode_t mode);
const char *iamf_sound_system_string(iamf_sound_system_t sound_system);
const char *iamf_loudspeaker_layout_string(iamf_loudspeaker_layout_t layout);
const char *iamf_expanded_loudspeaker_layout_string(
    iamf_expanded_loudspeaker_layout_t layout);
const char *iamf_layout_type_string(iamf_layout_type_t type);
const char *iamf_layout_string(iamf_layout_t layout);
const char *iamf_channel_name(iamf_channel_t ch);

#endif  //__IAMF_STRING_H__
