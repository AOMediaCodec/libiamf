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
 * @file iamf_post_processor.h
 * @brief IAMF post processor APIs.
 * @version 0.1
 * @date Created 12/11/2025
 **/

#ifndef __IAMF_POST_PROCESSOR_H__
#define __IAMF_POST_PROCESSOR_H__

#include "iamf_audio_block.h"

typedef struct IamfPostProcessor iamf_post_processor_t;

iamf_post_processor_t *iamf_post_processor_create();
void iamf_post_processor_destroy(iamf_post_processor_t *self);
int iamf_post_processor_init(iamf_post_processor_t *self, uint32_t sample_rate,
                             uint32_t channels);
int iamf_post_processor_enable_resampler(iamf_post_processor_t *self,
                                         uint32_t in_sample_rate);
int iamf_post_processor_disable_resampler(iamf_post_processor_t *self);
int iamf_post_processor_enable_limiter(iamf_post_processor_t *self,
                                       float threshold);
int iamf_post_processor_disable_limiter(iamf_post_processor_t *self);
int iamf_post_processor_process(iamf_post_processor_t *self,
                                iamf_audio_block_t *in,
                                iamf_audio_block_t **out);
uint32_t iamf_post_processor_get_delay(iamf_post_processor_t *self);

#endif  // __IAMF_POST_PROCESSOR_H__
