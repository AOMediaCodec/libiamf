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
 * @file audio_frame_obu.h
 * @brief Audio frame OBU APIs.
 * @version 2.0.0
 * @date Created 12/11/2025
 **/

#ifndef __AUDIO_FRAME_OBU_H__
#define __AUDIO_FRAME_OBU_H__

#include "iamf_obu.h"

/**
 *  Audio Frame OBU.
 * */

#define def_audio_frame_obu_ptr(a) ((iamf_audio_frame_obu_t *)(a))
typedef struct IamfAudioFrameObu {
  iamf_obu_t obu;

  uint32_t audio_substream_id;

  uint32_t num_samples_to_trim_at_end;
  uint32_t num_samples_to_trim_at_start;

  buffer_wrap_t *audio_frame;
} iamf_audio_frame_obu_t;

iamf_audio_frame_obu_t *iamf_audio_frame_obu_new(io_context_t *ior,
                                                 iamf_obu_header_t *header);
void iamf_audio_frame_obu_free(iamf_audio_frame_obu_t *obu);
void iamf_audio_frame_obu_display(iamf_audio_frame_obu_t *obu);

#endif  // __AUDIO_FRAME_OBU_H__
