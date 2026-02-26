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
 * @file iamf_post_processor.c
 * @brief IAMF post processor implementation.
 * @version 0.1
 * @date Created 12/11/2025
 **/

#include "iamf_post_processor.h"

#include "IAMF_defines.h"
#include "audio_effect_peak_limiter.h"
#include "clog.h"
#include "cvalue.h"
#include "iamf_private_definitions.h"
#include "iamf_utils.h"
#include "speex_resampler.h"

#define def_default_speex_resampler_quality 4

#define def_limiter_maximum_true_peak -1.0f
#define def_limiter_attack_sec 0.001f
#define def_limiter_release_sec 0.200f
#define def_limiter_look_ahead_sec 0.005f
#define def_limiter_look_ahead 240

typedef enum EStatus {
  ck_status_none,
  ck_status_init,
  ck_status_process,
  ck_status_flush,
} status_t;

typedef struct IamfResampler {
  iamf_audio_block_t *buffer[2];
  uint32_t in_sample_rate;
  SpeexResamplerState *speex_resampler;
} iamf_resampler_t;

struct IamfPostProcessor {
  status_t status;
  uint32_t sample_rate;
  uint32_t channels;

  uint32_t delay_resampler;
  uint32_t delay_limiter;

  iamf_resampler_t *resampler;
  AudioEffectPeakLimiter *limiter;
};

static iamf_audio_block_t *iamf_post_processor_priv_resample(
    iamf_post_processor_t *self, iamf_audio_block_t *audio_block) {
  iamf_audio_block_t *out = 0;
  iamf_resampler_t *resampler = self->resampler;
  SpeexResamplerState *speex_resampler = resampler->speex_resampler;
  uint32_t input_size = 0, resample_size = 0;
  float *data = 0;

  if (audio_block) {
    input_size = audio_block->num_samples_per_channel;

    if (!resampler->buffer[0] ||
        resampler->buffer[0]->capacity_per_channel < input_size) {
      if (resampler->buffer[0]) {
        iamf_audio_block_delete(resampler->buffer[0]);
        resampler->buffer[0] = 0;
      }
      resampler->buffer[0] =
          iamf_audio_block_new(0, input_size, self->channels);
      if (!resampler->buffer[0]) return 0;
    }

    resampler->buffer[0]->num_samples_per_channel = input_size;
    iamf_audio_block_copy(resampler->buffer[0], audio_block,
                          ck_sample_format_f32_interleaved);

    resample_size = iamf_fraction_transform(
        (fraction_t){.numerator = input_size,
                     .denominator = resampler->in_sample_rate},
        self->sample_rate);
    data = resampler->buffer[0]->data;
  } else {
    resample_size = speex_resampler_get_output_latency(speex_resampler);
    input_size = speex_resampler_get_input_latency(speex_resampler);
  }

  if (resample_size > 0) {
    out = iamf_audio_block_new(0, resample_size, self->channels);
    if (!out) return 0;

    if (!resampler->buffer[1] ||
        resampler->buffer[1]->capacity_per_channel < resample_size) {
      if (resampler->buffer[1]) {
        iamf_audio_block_delete(resampler->buffer[1]);
        resampler->buffer[1] = 0;
      }
      resampler->buffer[1] =
          iamf_audio_block_new(0, resample_size, self->channels);
      if (!resampler->buffer[1]) {
        iamf_audio_block_delete(out);
        return 0;
      }
    }

    speex_resampler_process_interleaved_float(
        speex_resampler, data, &input_size, resampler->buffer[1]->data,
        &resample_size);
    resampler->buffer[1]->num_samples_per_channel = resample_size;

    iamf_audio_block_copy(out, resampler->buffer[1],
                          ck_sample_format_f32_planar);
  } else {
    warning("no samples to resample.");
  }

  return out;
}

static iamf_audio_block_t *iamf_post_processor_priv_limit(
    iamf_post_processor_t *self, iamf_audio_block_t *audio_block) {
  iamf_audio_block_t *out = 0;
  int num_samples = 0;
  if (audio_block) {
    out = iamf_audio_block_new(
        audio_block->id, audio_block->num_samples_per_channel, self->channels);
    if (!out) return 0;

    num_samples = audio_effect_peak_limiter_process_block(
        self->limiter, audio_block->data, out->data,
        audio_block->num_samples_per_channel);
  } else {
    float *in = def_mallocz(float, self->delay_limiter * self->channels);
    if (!in) return 0;
    out = iamf_audio_block_new(0, self->delay_limiter, self->channels);
    if (!out) {
      def_free(in);
      return 0;
    }

    num_samples = audio_effect_peak_limiter_process_block(
        self->limiter, in, out->data, self->delay_limiter);
    def_free(in);
  }

  out->num_samples_per_channel = num_samples;

  return out;
}

iamf_post_processor_t *iamf_post_processor_create() {
  iamf_post_processor_t *self = def_mallocz(iamf_post_processor_t, 1);
  if (!self) return 0;
  self->status = ck_status_none;
  return self;
}

void iamf_post_processor_destroy(iamf_post_processor_t *self) {
  if (!self) return;
  iamf_post_processor_disable_resampler(self);
  iamf_post_processor_disable_limiter(self);
  def_free(self);
}

int iamf_post_processor_init(iamf_post_processor_t *self, uint32_t sample_rate,
                             uint32_t channels) {
  if (!self) return IAMF_ERR_BAD_ARG;

  self->status = ck_status_init;

  if (self->sample_rate != sample_rate) self->sample_rate = sample_rate;
  if (self->channels != channels) self->channels = channels;

  self->status = ck_status_process;

  return IAMF_OK;
}

int iamf_post_processor_enable_resampler(iamf_post_processor_t *self,
                                         uint32_t in_sample_rate) {
  int err;
  iamf_resampler_t *resampler = def_mallocz(iamf_resampler_t, 1);
  if (!resampler) return IAMF_ERR_ALLOC_FAIL;
  SpeexResamplerState *speex_resampler =
      speex_resampler_init(self->channels, in_sample_rate, self->sample_rate,
                           def_default_speex_resampler_quality, &err);
  info("resample: in %u, out %u", in_sample_rate, self->sample_rate);
  if (!speex_resampler) {
    def_free(resampler);
    return IAMF_ERR_ALLOC_FAIL;
  }
  resampler->speex_resampler = speex_resampler;
  resampler->in_sample_rate = in_sample_rate;
  speex_resampler_skip_zeros(speex_resampler);

  self->resampler = resampler;
  self->delay_resampler = speex_resampler_get_output_latency(speex_resampler);

  return IAMF_OK;
}

int iamf_post_processor_disable_resampler(iamf_post_processor_t *self) {
  if (!self->resampler) return IAMF_OK;
  if (self->resampler->buffer[0])
    iamf_audio_block_delete(self->resampler->buffer[0]);
  if (self->resampler->buffer[1])
    iamf_audio_block_delete(self->resampler->buffer[1]);
  if (self->resampler->speex_resampler) {
    speex_resampler_destroy(self->resampler->speex_resampler);
  }
  def_free(self->resampler);
  self->resampler = 0;
  return IAMF_OK;
}

int iamf_post_processor_enable_limiter(iamf_post_processor_t *self,
                                       float threshold) {
  self->limiter = audio_effect_peak_limiter_create();
  if (!self->limiter) return IAMF_ERR_ALLOC_FAIL;
  self->delay_limiter = def_limiter_look_ahead_sec * self->sample_rate;
  audio_effect_peak_limiter_init(self->limiter, threshold, self->sample_rate,
                                 self->channels, def_limiter_attack_sec,
                                 def_limiter_release_sec, self->delay_limiter);
  return IAMF_OK;
}

int iamf_post_processor_disable_limiter(iamf_post_processor_t *self) {
  if (!self->limiter) return IAMF_OK;
  audio_effect_peak_limiter_destroy(self->limiter);
  self->limiter = 0;
  return IAMF_OK;
}

int iamf_post_processor_process(iamf_post_processor_t *self,
                                iamf_audio_block_t *in,
                                iamf_audio_block_t **out) {
  iamf_audio_block_t *last = in;

  if (self->status != ck_status_process) return 0;

  if (self->resampler) {
    iamf_audio_block_t *next = iamf_post_processor_priv_resample(self, last);
    if (!next) return IAMF_ERR_ALLOC_FAIL;
    last = next;
  }

  if (self->limiter) {
    iamf_audio_block_t *next = 0;
    iamf_audio_block_t *blocks[2] = {0};

    if (last) blocks[0] = iamf_post_processor_priv_limit(self, last);
    if (!in) blocks[1] = iamf_post_processor_priv_limit(self, 0);

    if (blocks[0] && blocks[1]) {
      next = iamf_audio_block_samples_concat(blocks, 2);
      iamf_audio_block_delete(blocks[0]);
      iamf_audio_block_delete(blocks[1]);
    } else if (blocks[0]) {
      next = blocks[0];
    } else if (blocks[1]) {
      next = blocks[1];
    }

    if (last != in) iamf_audio_block_delete(last);

    if (!next) return IAMF_ERR_ALLOC_FAIL;
    last = next;
  }

  if (in && last == in) {
    last = iamf_audio_block_new(in->id, in->num_samples_per_channel,
                                self->channels);

    if (!last) return IAMF_ERR_ALLOC_FAIL;
    iamf_audio_block_channels_concat(last, &in, 1);
  }

  *out = last;

  if (!in) self->status = ck_status_flush;
  return IAMF_OK;
}

uint32_t iamf_post_processor_get_delay(iamf_post_processor_t *self) {
  return self->delay_limiter + self->delay_resampler;
}
