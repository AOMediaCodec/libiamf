/*
BSD 3-Clause Clear License The Clear BSD License

Copyright (c) 2023, Alliance for Open Media.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/**
 * @file downmix_renderer.c
 * @brief DMRenderer.
 * @version 0.1
 * @date Created 06/21/2023
 **/

#ifdef IA_TAG
#undef IA_TAG
#endif

#define IA_TAG "IAMF_DMR"

#include "downmix_renderer.h"

#include "IAMF_debug.h"
#include "IAMF_utils.h"
#include "fixedp11_5.h"

typedef struct DependOnChannel {
  IAChannel ch;
  float s;
  float *sp;
} DependOnChannel;

struct Downmixer {
  int mode;
  int w_idx;
  IAChannel chs_in[IA_CH_LAYOUT_MAX_CHANNELS];
  IAChannel chs_out[IA_CH_LAYOUT_MAX_CHANNELS];
  int chs_icount;
  int chs_ocount;
  float *chs_data[IA_CH_COUNT];
  DependOnChannel *deps[IA_CH_COUNT];
  MixFactors mix_factors;
};

static DependOnChannel chmono[] = {{IA_CH_R2, 0.5f}, {IA_CH_L2, 0.5}, {0}};
static DependOnChannel chl2[] = {{IA_CH_L3, 1.f}, {IA_CH_C, 0.707}, {0}};
static DependOnChannel chr2[] = {{IA_CH_R3, 1.f}, {IA_CH_C, 0.707}, {0}};
static DependOnChannel chtl[] = {{IA_CH_HL, 1.f}, {IA_CH_SL5}, {0}};
static DependOnChannel chtr[] = {{IA_CH_HR, 1.f}, {IA_CH_SR5}, {0}};
static DependOnChannel chl3[] = {{IA_CH_L5, 1.f}, {IA_CH_SL5}, {0}};
static DependOnChannel chr3[] = {{IA_CH_R5, 1.f}, {IA_CH_SR5}, {0}};
static DependOnChannel chsl5[] = {{IA_CH_SL7}, {IA_CH_BL7}, {0}};
static DependOnChannel chsr5[] = {{IA_CH_SR7}, {IA_CH_BR7}, {0}};
static DependOnChannel chhl[] = {{IA_CH_HFL, 1.f}, {IA_CH_HBL}, {0}};
static DependOnChannel chhr[] = {{IA_CH_HFR, 1.f}, {IA_CH_HBR}, {0}};

static int _valid_channel_layout(IAChannelLayoutType in) {
  return ia_channel_layout_type_check(in) && in != IA_CHANNEL_LAYOUT_BINAURAL;
}

static int _valid_downmix(IAChannelLayoutType in, IAChannelLayoutType out) {
  uint32_t s1, s2, t1, t2;

  s1 = ia_channel_layout_get_category_channels_count(in, IA_CH_CATE_SURROUND);
  s2 = ia_channel_layout_get_category_channels_count(out, IA_CH_CATE_SURROUND);
  t1 = ia_channel_layout_get_category_channels_count(in, IA_CH_CATE_TOP);
  t2 = ia_channel_layout_get_category_channels_count(out, IA_CH_CATE_TOP);

  if ((!t2 && t1) || s2 < 2) return 0;
  return s1 > s2 || t1 > t2;
}

static void _downmix_dump(DMRenderer *this, IAChannel c) {
  DependOnChannel *cs = this->deps[c];
  if (this->chs_data[c]) return;
  if (!this->deps[c]) {
    ia_loge("channel %s(%d) can not be found.", ia_channel_name(c), c);
    return;
  }

  while (cs->ch) {
    if (cs->sp) {
      ia_logd("channel %s(%d), scale point %f%s", ia_channel_name(cs->ch),
              cs->ch, *cs->sp, this->chs_data[cs->ch] ? ", s." : " m.");
      _downmix_dump(this, cs->ch);
    } else {
      ia_logd("channel %s(%d), scale %f%s", ia_channel_name(cs->ch), cs->ch,
              cs->s, this->chs_data[cs->ch] ? ", s." : " m.");
      _downmix_dump(this, cs->ch);
    }
    ++cs;
  }
}

static float _downmix_channel_data(DMRenderer *this, IAChannel c, int i) {
  DependOnChannel *cs = this->deps[c];
  float sum = 0.f;
  if (this->chs_data[c]) return this->chs_data[c][i];
  if (!this->deps[c]) return 0.f;

  while (cs->ch) {
    if (cs->sp)
      sum += _downmix_channel_data(this, cs->ch, i) * (*cs->sp);
    else
      sum += _downmix_channel_data(this, cs->ch, i) * cs->s;
    ++cs;
  }
  return sum;
}

DMRenderer *DMRenderer_open(IAChannelLayoutType in, IAChannelLayoutType out) {
  DMRenderer *this = 0;
  ia_logd("%s downmix to %s", ia_channel_layout_name(in),
          ia_channel_layout_name(out));
  if (in == out || !_valid_channel_layout(in) || !_valid_channel_layout(out))
    return 0;

  if (!_valid_downmix(in, out)) return 0;

  this = IAMF_MALLOCZ(DMRenderer, 1);
  if (!this) return 0;

  this->chs_icount = ia_channel_layout_get_channels(in, this->chs_in,
                                                    IA_CH_LAYOUT_MAX_CHANNELS);
  this->chs_ocount = ia_channel_layout_get_channels(out, this->chs_out,
                                                    IA_CH_LAYOUT_MAX_CHANNELS);

  this->mode = -1;
  this->w_idx = -1;

  this->deps[IA_CH_MONO] = chmono;
  this->deps[IA_CH_L2] = chl2;
  this->deps[IA_CH_R2] = chr2;
  this->deps[IA_CH_L3] = chl3;
  this->deps[IA_CH_R3] = chr3;
  this->deps[IA_CH_SL5] = chsl5;
  this->deps[IA_CH_SR5] = chsr5;
  this->deps[IA_CH_TL] = chtl;
  this->deps[IA_CH_TR] = chtr;
  this->deps[IA_CH_HL] = chhl;
  this->deps[IA_CH_HR] = chhr;

  this->deps[IA_CH_SR5][0].sp = this->deps[IA_CH_SL5][0].sp =
      &this->mix_factors.alpha;
  this->deps[IA_CH_SR5][1].sp = this->deps[IA_CH_SL5][1].sp =
      &this->mix_factors.beta;
  this->deps[IA_CH_L3][1].sp = this->deps[IA_CH_R3][1].sp =
      &this->mix_factors.delta;
  this->deps[IA_CH_HL][1].sp = this->deps[IA_CH_HR][1].sp =
      &this->mix_factors.gamma;

  for (int i = 0; i < this->chs_icount; ++i) this->deps[this->chs_in[i]] = 0;

  return this;
}

void DMRenderer_close(DMRenderer *this) { IAMF_FREE(this); }

int DMRenderer_set_mode_weight(DMRenderer *this, int mode, int w_idx) {
  if (!this || !iamf_valid_mix_mode(mode)) return IAMF_ERR_BAD_ARG;

  if (this->mode != mode) {
    ia_logd("dmixtypenum: %d -> %d", this->mode, mode);
    this->mode = mode;
    this->mix_factors = *iamf_get_mix_factors(mode);
    ia_logd("mode %d: a %f, b %f, g %f, d %f, w index offset %d", mode,
            this->mix_factors.alpha, this->mix_factors.beta,
            this->mix_factors.gamma, this->mix_factors.delta,
            this->mix_factors.w_idx_offset);
  }

  if (w_idx < MIN_W_INDEX || w_idx > MAX_W_INDEX) {
    int new_w_idx;

    calc_w(this->mix_factors.w_idx_offset, this->w_idx, &new_w_idx);
    ia_logd("weight state index : %d (%f) -> %d (%f)", this->w_idx,
            get_w(this->w_idx), new_w_idx, get_w(new_w_idx));
    this->w_idx = new_w_idx;

    this->deps[IA_CH_TL][1].s = this->deps[IA_CH_TR][1].s =
        this->mix_factors.gamma * get_w(new_w_idx);
  } else {
    if (mode != this->mode) ia_logd("set default demixing mode %d", mode);
    if (this->w_idx != w_idx) {
      this->w_idx = w_idx;
      ia_logd("set default weight index %d, value %f", w_idx, get_w(w_idx));
      this->deps[IA_CH_TL][1].s = this->deps[IA_CH_TR][1].s =
          this->mix_factors.gamma * get_w(w_idx);
    }
  }

  return IAMF_OK;
}

int DMRenderer_downmix(DMRenderer *this, float *in, float *out, uint32_t size) {
  int off;
  if (!this || !in || !out || !size) return IAMF_ERR_BAD_ARG;

  memset(this->chs_data, 0, IA_CH_COUNT * sizeof(float));
  for (int i = 0; i < this->chs_icount; ++i) {
    this->chs_data[this->chs_in[i]] = in + size * i;
  }

  for (int i = 0; i < this->chs_ocount; ++i) {
    ia_logd("channel %s(%d) checking...", ia_channel_name(this->chs_out[i]),
            this->chs_out[i]);
    _downmix_dump(this, this->chs_out[i]);
    off = size * i;
    for (int j = 0; j < size; ++j) {
      out[off + j] = _downmix_channel_data(this, this->chs_out[i], j);
    }
  }

  return IAMF_OK;
}
