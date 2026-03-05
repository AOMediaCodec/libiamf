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
 * @file iamf_audio_block.h
 * @brief IAMF audio block APIs.
 * @version 2.0.0
 * @date Created 12/11/2025
 **/

#ifndef __IAMF_AUDIO_BLOCK_H__
#define __IAMF_AUDIO_BLOCK_H__

#include <stdint.h>

typedef enum ESampleFormat {
  ck_sample_format_none = 0,
  ck_sample_format_i16_interleaved,
  ck_sample_format_i24_interleaved,
  ck_sample_format_i32_interleaved,
  ck_sample_format_f32_interleaved,
  ck_sample_format_f32_planar = 0x20,
} sample_format_t;

typedef struct IamfAudioBlock {
  uint32_t id;

  float* data;
  uint32_t num_channels;
  uint32_t num_samples_per_channel;
  uint32_t capacity_per_channel;  // in samples

  uint32_t skip;            // in samples
  uint32_t padding;         // in samples
  uint32_t second_skip;     // in samples
  uint32_t second_padding;  // in samples
} iamf_audio_block_t;

iamf_audio_block_t* iamf_audio_block_default_new(uint32_t id);
iamf_audio_block_t* iamf_audio_block_new(uint32_t id,
                                         uint32_t capacity_per_channel,
                                         uint32_t num_channels);
int iamf_audio_block_resize(iamf_audio_block_t* block,
                            uint32_t capacity_per_channel,
                            uint32_t num_channels);
void iamf_audio_block_delete(iamf_audio_block_t* block);
int iamf_audio_block_samples_info_copy(iamf_audio_block_t* dst,
                                       iamf_audio_block_t* src);
int iamf_audio_block_padding(iamf_audio_block_t* block, uint32_t padding);
int iamf_audio_block_reorder(iamf_audio_block_t* block, const uint8_t order[],
                             uint32_t num);
int iamf_audio_block_channels_concat(iamf_audio_block_t* dst,
                                     iamf_audio_block_t* src[], uint32_t n);
int iamf_audio_block_copy(iamf_audio_block_t* dst, iamf_audio_block_t* src,
                          sample_format_t format);
int iamf_audio_block_copy_data(iamf_audio_block_t* src, uint8_t* data,
                               sample_format_t format);
int iamf_audio_block_partial_copy_data(iamf_audio_block_t* dst,
                                       uint32_t dst_offset,
                                       iamf_audio_block_t* src,
                                       uint32_t src_offset, uint32_t size);
iamf_audio_block_t* iamf_audio_block_samples_concat(
    iamf_audio_block_t* blocks[], uint32_t n);
int iamf_audio_block_trim(iamf_audio_block_t* block);
uint32_t iamf_audio_block_available_samples(iamf_audio_block_t* block);

#endif  //__IAMF_AUDIO_BLOCK_H__
