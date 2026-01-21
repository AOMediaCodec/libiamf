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
 * @file iamf_audio_block.c
 * @brief IAMF audio block implementation.
 * @version 2.0.0
 * @date Created 12/11/2025
 **/

#include "iamf_audio_block.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "iamf_private_definitions.h"

static int16_t _f32_to_i16(float x) {
  x = x * 32768.f;
  x = def_max(x, -32768.f);
  x = def_min(x, 32767.f);
  return (int16_t)lrintf(x);
}

static int32_t _f32_to_i24(float x) {
  x = x * 8388608.f;
  x = def_max(x, -8388608.f);
  x = def_min(x, 8388607.f);
  return (int32_t)lrintf(x);
}

static int32_t _f32_to_i32(float x) {
  x = x * 2147483648.f;
  if (x > -2147483648.f && x < 2147483647.f)
    return (int32_t)lrintf(x);
  else
    return (x > 0.0f ? 2147483647 : (-2147483647 - 1));
}

iamf_audio_block_t* iamf_audio_block_default_new(uint32_t id) {
  iamf_audio_block_t* block = def_mallocz(iamf_audio_block_t, 1);
  if (block) block->id = id;
  return block;
}

iamf_audio_block_t* iamf_audio_block_new(uint32_t id,
                                         uint32_t capacity_per_channel,
                                         uint32_t num_channels) {
  if (!num_channels || !capacity_per_channel) return 0;

  iamf_audio_block_t* block = iamf_audio_block_default_new(id);
  if (!block) return 0;

  block->data = def_mallocz(float, (num_channels * capacity_per_channel));
  if (!block->data) {
    iamf_audio_block_delete(block);
    return 0;
  }

  block->num_channels = num_channels;
  block->capacity_per_channel = capacity_per_channel;

  return block;
}

int iamf_audio_block_resize(iamf_audio_block_t* block,
                            uint32_t capacity_per_channel,
                            uint32_t num_channels) {
  if (!block) return IAMF_ERR_BAD_ARG;

  if (block->num_samples_per_channel > 0) return IAMF_ERR_INVALID_STATE;

  if (!num_channels || !capacity_per_channel) return IAMF_ERR_BAD_ARG;

  if (block->capacity_per_channel * block->num_samples_per_channel >
      capacity_per_channel * num_channels)
    return IAMF_OK;

  float* new_data = def_mallocz(float, (num_channels * capacity_per_channel));
  if (!new_data) return -12;

  if (block->data) {
    memcpy(new_data, block->data,
           sizeof(float) * block->num_channels * block->capacity_per_channel);
    def_free(block->data);
  }

  block->data = new_data;
  block->num_channels = num_channels;
  block->capacity_per_channel = capacity_per_channel;

  return IAMF_OK;
}

void iamf_audio_block_delete(iamf_audio_block_t* block) {
  if (block) {
    def_free(block->data);
    def_free(block);
  }
}

int iamf_audio_block_samples_info_copy(iamf_audio_block_t* dst,
                                       iamf_audio_block_t* src) {
  if (!dst || !src) return -22;

  dst->skip = src->skip;
  dst->padding = src->padding;
  dst->second_skip = src->second_skip;
  dst->second_padding = src->second_padding;
  dst->num_samples_per_channel = src->num_samples_per_channel;

  return 0;
}

int iamf_audio_block_padding(iamf_audio_block_t* block, uint32_t padding) {
  if (!block) return -22;

  if (block->num_samples_per_channel + padding > block->capacity_per_channel) {
    padding = block->capacity_per_channel - block->num_samples_per_channel;
  }

  // if (block->interleaved) return -38;

  for (int ch = block->num_channels - 1; ch >= 0; --ch) {
    float* channel_data = block->data + ch * block->num_samples_per_channel;
    float* new_channel_data =
        block->data + ch * (block->num_samples_per_channel + padding);

    memmove(new_channel_data, channel_data,
            sizeof(float) * block->num_samples_per_channel);
    memset(new_channel_data + block->num_samples_per_channel, 0,
           sizeof(float) * padding);
  }

  block->num_samples_per_channel += padding;
  block->padding += padding;
  return padding;
}

int iamf_audio_block_reorder(iamf_audio_block_t* block, const uint8_t order[],
                             uint32_t num) {
  float* tmp;
  if (!block || !order || num > block->num_channels || !block->data) return -22;

  tmp = def_mallocz(float, (block->num_channels * block->capacity_per_channel));
  if (!tmp) return -12;

  for (int i = 0; i < num; ++i)
    memcpy(tmp + order[i] * block->num_samples_per_channel,
           block->data + i * block->num_samples_per_channel,
           sizeof(float) * block->num_samples_per_channel);
  def_free(block->data);
  block->data = tmp;

  return 0;
}

int iamf_audio_block_channels_concat(iamf_audio_block_t* dst,
                                     iamf_audio_block_t* src[], uint32_t n) {
  int num_channels = 0;
  int num_samples = 0;

  if (!dst || !src || n <= 0) return -22;

  // if (dst->interleaved) return -38;

  num_samples = src[0]->num_samples_per_channel;
  for (int i = 0; i < n; ++i) {
    if (!src[i] || num_samples > dst->capacity_per_channel ||
        src[i]->num_samples_per_channel != num_samples
        // || dst->interleaved != src[i]->interleaved
    ) {
      warning("iamf_audio_block_channels_concat error!");
      return -22;
    }
  }

  for (int i = 0; i < n; ++i) num_channels += src[i]->num_channels;
  if (dst->num_channels != num_channels) {
    error("channel number %d vs %d not match!", dst->num_channels,
          num_channels);
    return -22;
  }

  iamf_audio_block_samples_info_copy(dst, src[0]);

  num_channels = 0;
  for (int i = 0; i < n; ++i) {
    memcpy(dst->data + num_channels * num_samples, src[i]->data,
           sizeof(float) * num_samples * src[i]->num_channels);
    num_channels += src[i]->num_channels;
  }

  return 0;
}

int iamf_audio_block_copy(iamf_audio_block_t* dst, iamf_audio_block_t* src,
                          sample_format_t format) {
  uint32_t count = 0;
  if (!dst || !src) return -22;

  // Check if destination has enough capacity
  count = dst->capacity_per_channel * dst->num_channels;
  if (count < src->num_samples_per_channel * src->num_channels) return -22;

  dst->capacity_per_channel = count / src->num_channels;
  dst->num_channels = src->num_channels;
  dst->num_samples_per_channel = src->num_samples_per_channel;
  dst->skip = src->skip;
  dst->padding = src->padding;
  dst->second_skip = src->second_skip;
  dst->second_padding = src->second_padding;

  switch (format) {
    case ck_sample_format_none:
      // Direct copy - data is already in planar format
      memcpy(dst->data, src->data,
             sizeof(float) * src->num_channels * src->num_samples_per_channel);
      break;

    case ck_sample_format_f32_interleaved:
      // Convert from planar to interleaved
      for (uint32_t ch = 0; ch < src->num_channels; ++ch) {
        for (uint32_t i = 0; i < src->num_samples_per_channel; ++i) {
          dst->data[i * src->num_channels + ch] =
              src->data[ch * src->num_samples_per_channel + i];
        }
      }
      break;

    case ck_sample_format_f32_planar:
      // Convert from interleaved to planar
      for (uint32_t ch = 0; ch < src->num_channels; ++ch) {
        for (uint32_t i = 0; i < src->num_samples_per_channel; ++i) {
          dst->data[ch * src->num_samples_per_channel + i] =
              src->data[i * src->num_channels + ch];
        }
      }
      break;

    default:
      return -38;  // Unsupported format
  }

  return 0;
}

int iamf_audio_block_copy_data(iamf_audio_block_t* src, uint8_t* data,
                               sample_format_t format) {
  if (!src || !data || !src->num_channels || !src->num_samples_per_channel)
    return -22;

  if (format == ck_sample_format_i16_interleaved) {
    int16_t* int16_dst = (int16_t*)data;
    for (int c = 0; c < src->num_channels; ++c) {
      for (int i = 0; i < src->num_samples_per_channel; i++) {
        int16_dst[i * src->num_channels + c] =
            _f32_to_i16(src->data[src->num_samples_per_channel * c + i]);
      }
    }
  } else if (format == ck_sample_format_i24_interleaved) {
    uint8_t* int24_dst = (uint8_t*)data;
    for (int c = 0; c < src->num_channels; ++c) {
      for (int i = 0; i < src->num_samples_per_channel; i++) {
        int32_t tmp =
            _f32_to_i24(src->data[src->num_samples_per_channel * c + i]);
        int24_dst[(i * src->num_channels + c) * 3] = tmp & 0xff;
        int24_dst[(i * src->num_channels + c) * 3 + 1] = (tmp >> 8) & 0xff;
        int24_dst[(i * src->num_channels + c) * 3 + 2] =
            ((tmp >> 16) & 0x7f) | ((tmp >> 24) & 0x80);
      }
    }
  } else if (format == ck_sample_format_i32_interleaved) {
    int32_t* int32_dst = (int32_t*)data;
    for (int c = 0; c < src->num_channels; ++c) {
      for (int i = 0; i < src->num_samples_per_channel; i++) {
        int32_dst[i * src->num_channels + c] =
            _f32_to_i32(src->data[src->num_samples_per_channel * c + i]);
      }
    }
  }

  return 0;
}

iamf_audio_block_t* iamf_audio_block_samples_concat(
    iamf_audio_block_t* blocks[], uint32_t n) {
  int num_channels = 0;
  int num_samples = 0;
  iamf_audio_block_t* ablock = 0;

  if (!blocks || n <= 0) return 0;

  num_channels = blocks[0]->num_channels;
  for (int i = 0; i < n; ++i) {
    if (blocks[i]->num_channels != num_channels) return 0;
  }

  for (int i = 0; i < n; ++i) {
    ablock = blocks[i];
    num_samples +=
        (ablock->num_samples_per_channel - ablock->skip - ablock->padding);
  }

  ablock = iamf_audio_block_new(0, num_samples, num_channels);
  if (ablock) {
    for (int c = 0; c < num_channels; ++c) {
      uint32_t off = 0;
      for (int i = 0; i < n; ++i) {
        uint32_t samples = blocks[i]->num_samples_per_channel -
                           blocks[i]->skip - blocks[i]->padding;
        memcpy(ablock->data + c * num_samples + off,
               blocks[i]->data + c * blocks[i]->num_samples_per_channel +
                   blocks[i]->skip,
               sizeof(float) * samples);
        off += samples;
      }
    }
    ablock->num_samples_per_channel = num_samples;
  }
  return ablock;
}

int iamf_audio_block_trim(iamf_audio_block_t* block) {
  uint32_t padding = 0;
  if (!block) return -22;

  padding =
      block->padding + block->skip + block->second_skip + block->second_padding;
  if (block->num_samples_per_channel < padding) {
    block->num_samples_per_channel = 0;
    return 0;
  }

  uint32_t num_samples = block->num_samples_per_channel;
  block->num_samples_per_channel -= padding;

  if (num_samples != block->num_samples_per_channel) {
    for (int i = 0; i < block->num_channels; ++i) {
      float* dst = block->data + i * block->num_samples_per_channel;
      float* src =
          block->data + i * num_samples + block->skip + block->second_skip;
      memmove(dst, src, sizeof(float) * block->num_samples_per_channel);
    }
    block->skip = block->padding = block->second_skip = block->second_padding =
        0;
  }
  return 0;
}

int iamf_audio_block_gain(iamf_audio_block_t* block, float gain) {
  if (!block) return -22;
  if (gain == def_default_loudness_gain) return 0;
  int num_samples = block->num_channels * block->num_samples_per_channel;

  for (int i = 0; i < num_samples; ++i) block->data[i] *= gain;
  return 0;
}

uint32_t iamf_audio_block_available_samples(iamf_audio_block_t* block) {
  if (!block) return 0;
  return block->num_samples_per_channel - block->skip - block->padding -
         block->second_skip - block->second_padding;
}

int iamf_audio_block_partial_copy_data(iamf_audio_block_t* dst,
                                       uint32_t dst_offset,
                                       iamf_audio_block_t* src,
                                       uint32_t src_offset, uint32_t size) {
  if (!dst || !src) return -22;
  if (src_offset + size > src->num_samples_per_channel) return -22;
  if (dst_offset + size > dst->capacity_per_channel) return -22;
  if (dst->num_channels != src->num_channels) return -22;

  for (uint32_t ch = 0; ch < src->num_channels; ++ch)
    memcpy(&dst->data[dst_offset + ch * dst->num_samples_per_channel],
           &src->data[src_offset + ch * src->num_samples_per_channel],
           size * sizeof(float));

  return 0;
}

void iamf_audio_block_display(iamf_audio_block_t* block) {
  if (!block) return;

  debug("iamf_audio_block_t details:");
  debug("  id: %u", block->id);
  debug("  num_channels: %u", block->num_channels);
  debug("  num_samples: %u", block->num_samples_per_channel);
  debug("  capacity_per_channel: %u", block->capacity_per_channel);
  debug("  skip: %u", block->skip);
  debug("  padding: %u", block->padding);
  debug("  second_skip: %u", block->second_skip);
  debug("  second_padding: %u", block->second_padding);
  debug("  data pointer: %p", (void*)block->data);
}
