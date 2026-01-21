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
 * @file demixer.c
 * @brief Demixer.
 * @version 0.1
 * @date Created 03/03/2023
 **/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define _USE_MATH_DEFINES
#include <math.h>

#include "IAMF_decoder.h"
#include "clog.h"
#include "demixer.h"
#include "iamf_layout.h"
#include "iamf_string.h"

#ifdef def_log_tag
#undef def_log_tag
#endif

#define def_log_tag "IAMF_DMX"

/**
 * alpha and beta are gain values used for S7to5 down-mixer,
 * gamma for T4to2 downmixer,
 * delta for S5to3 down-mixer and
 * w_idx_offset is the offset to generate a gain value w used for T2toTF2
 * down-mixer.
 * */

static struct DemixingTypeMat {
  float alpha;
  float beta;
  float gamma;
  float delta;
  int w_idx_offset;
} demixing_type_mat[] = {
    {1.0, 1.0, 0.707, 0.707, -1},   {0.707, 0.707, 0.707, 0.707, -1},
    {1.0, 0.866, 0.866, 0.866, -1}, {0, 0, 0, 0, 0},
    {1.0, 1.0, 0.707, 0.707, 1},    {0.707, 0.707, 0.707, 0.707, 1},
    {1.0, 0.866, 0.866, 0.866, 1},  {0, 0, 0, 0, 0}};

struct Demixer {
  float *ch_data[ck_iamf_channel_count];
  int weight_state_idx;
  int last_dmixtypenum;
  int last_weight_state_idx;

  int frame_size;
  int skip;
  float *hanning_filter;
  float *start_window;
  float *stop_window;
  float *large_buffer;
  float ch_last_sf[ck_iamf_channel_count];
  float ch_last_sfavg[ck_iamf_channel_count];

  iamf_loudspeaker_layout_t layout;
  iamf_channel_t chs_in[def_max_audio_channels];
  iamf_channel_t chs_out[def_max_audio_channels];
  int chs_count;

  struct {
    struct {
      iamf_channel_t ch;
      float gain;
    } ch_gain[def_max_audio_channels];
    uint8_t count;
  } chs_gain_list;

  int demixing_mode;

  struct {
    struct {
      iamf_channel_t ch;
      float recon_gain;
    } ch_recon_gain[ck_iamf_channel_recon_count];
    uint8_t count;
    uint32_t flags;
  } chs_recon_gain_list;
};

enum {
  CH_MX_S_L,
  CH_MX_S_R,
  CH_MX_S5_L,
  CH_MX_S5_R,
  CH_MX_T_L,
  CH_MX_T_R,
  CH_MX_COUNT
};

static float widx2w_table[11] = {0.0,    0.0179, 0.0391, 0.0658, 0.1038, 0.25,
                                 0.3962, 0.4342, 0.4609, 0.4821, 0.5};
static float calc_w(int w_idx_offset, int w_idx_prev, int *w_idx) {
  if (w_idx_offset > 0)
    *w_idx = def_min(w_idx_prev + 1, 10);
  else
    *w_idx = def_max(w_idx_prev - 1, 0);

  return widx2w_table[*w_idx];
}

static float get_w(int w_idx) {
  if (w_idx < 0)
    return widx2w_table[0];
  else if (w_idx > 10)
    return widx2w_table[10];

  return widx2w_table[w_idx];
}

/**
 * S1to2 de-mixer: R2 = 2 x Mono - L2
 * */
static int dmx_s2(Demixer *ths) {
  float *r = 0;
  if (!ths->ch_data[ck_iamf_channel_l2]) return IAMF_ERR_INTERNAL;
  if (ths->ch_data[ck_iamf_channel_r2]) return 0;
  if (!ths->ch_data[ck_iamf_channel_mono]) return IAMF_ERR_INTERNAL;

  debug("---- s1to2 ----");

  r = &ths->large_buffer[ths->frame_size * CH_MX_S_R];

  for (int i = 0; i < ths->frame_size; ++i) {
    r[i] = 2 * ths->ch_data[ck_iamf_channel_mono][i] -
           ths->ch_data[ck_iamf_channel_l2][i];
  }

  ths->ch_data[ck_iamf_channel_r2] = r;

  debug("reconstructed channel %s(%d) at %p, buffer at %p",
        iamf_channel_name(ck_iamf_channel_r2), ck_iamf_channel_r2,
        ths->ch_data[ck_iamf_channel_r2], ths->large_buffer);
  return 0;
}

/**
 * S2to3 de-mixer: L3 = L2 - 0.707 x C and R3 = R2 - 0.707 x C
 * */
static int dmx_s3(Demixer *ths) {
  float *l, *r;
  uint32_t fs = ths->frame_size;

  if (ths->ch_data[ck_iamf_channel_r3]) return 0;
  if (dmx_s2(ths)) return IAMF_ERR_INTERNAL;
  if (!ths->ch_data[ck_iamf_channel_c]) return IAMF_ERR_INTERNAL;

  trace("---- s2to3 ----");

  l = &ths->large_buffer[CH_MX_S_L * fs];
  r = &ths->large_buffer[CH_MX_S_R * fs];

  for (int i = 0; i < fs; i++) {
    l[i] = ths->ch_data[ck_iamf_channel_l2][i] -
           0.707 * ths->ch_data[ck_iamf_channel_c][i];
    r[i] = ths->ch_data[ck_iamf_channel_r2][i] -
           0.707 * ths->ch_data[ck_iamf_channel_c][i];
  }
  ths->ch_data[ck_iamf_channel_l3] = l;
  ths->ch_data[ck_iamf_channel_r3] = r;

  debug(
      "reconstructed channel %s(%d) at %p, channel %s(%d) at %p, buffer at "
      "%p",
      iamf_channel_name(ck_iamf_channel_l3), ck_iamf_channel_l3,
      ths->ch_data[ck_iamf_channel_l3], iamf_channel_name(ck_iamf_channel_r3),
      ck_iamf_channel_r3, ths->ch_data[ck_iamf_channel_r3], ths->large_buffer);

  return 0;
}

/**
 * S3to5 de-mixer: Ls = 1/δ(k) x (L3 - L5) and Rs = 1/δ(k) x (R3 - R5)
 * */
static int dmx_s5(Demixer *ths) {
  float *l, *r;
  uint32_t fs = ths->frame_size;

  int Typeid = ths->demixing_mode;
  int last_Typeid = ths->last_dmixtypenum;
  int i = 0;

  if (ths->ch_data[ck_iamf_channel_sr5]) return 0;
  if (dmx_s3(ths)) return IAMF_ERR_INTERNAL;
  if (!ths->ch_data[ck_iamf_channel_l5] || !ths->ch_data[ck_iamf_channel_r5])
    return IAMF_ERR_INTERNAL;

  trace("---- s3to5 ----");
  debug("Typeid %d, Lasttypeid %d", Typeid, last_Typeid);

  l = &ths->large_buffer[CH_MX_S5_L * fs];
  r = &ths->large_buffer[CH_MX_S5_R * fs];

  for (; i < ths->skip; i++) {
    l[i] = (ths->ch_data[ck_iamf_channel_l3][i] -
            ths->ch_data[ck_iamf_channel_l5][i]) /
           demixing_type_mat[last_Typeid].delta;
    r[i] = (ths->ch_data[ck_iamf_channel_r3][i] -
            ths->ch_data[ck_iamf_channel_r5][i]) /
           demixing_type_mat[last_Typeid].delta;
  }

  for (; i < fs; i++) {
    l[i] = (ths->ch_data[ck_iamf_channel_l3][i] -
            ths->ch_data[ck_iamf_channel_l5][i]) /
           demixing_type_mat[Typeid].delta;
    r[i] = (ths->ch_data[ck_iamf_channel_r3][i] -
            ths->ch_data[ck_iamf_channel_r5][i]) /
           demixing_type_mat[Typeid].delta;
  }

  ths->ch_data[ck_iamf_channel_sl5] = l;
  ths->ch_data[ck_iamf_channel_sr5] = r;

  debug(
      "reconstructed channel %s(%d) at %p, channel %s(%d) at %p, buffer at "
      "%p",
      iamf_channel_name(ck_iamf_channel_sl5), ck_iamf_channel_sl5,
      ths->ch_data[ck_iamf_channel_sl5], iamf_channel_name(ck_iamf_channel_sr5),
      ck_iamf_channel_sr5, ths->ch_data[ck_iamf_channel_sr5],
      ths->large_buffer);
  return 0;
}

/**
 * S5to7 de-mixer: Lrs = 1/β(k) x (Ls - α(k) x Lss) and
 *                 Rrs = 1/β(k) x (Rs - α(k) x Rss)
 * */
static int dmx_s7(Demixer *ths) {
  float *l, *r;
  uint32_t fs = ths->frame_size;

  int i = 0;
  int Typeid = ths->demixing_mode;
  int last_Typeid = ths->last_dmixtypenum;

  if (ths->ch_data[ck_iamf_channel_br7]) return 0;
  if (dmx_s5(ths) < 0) return IAMF_ERR_INTERNAL;
  if (!ths->ch_data[ck_iamf_channel_sl7] || !ths->ch_data[ck_iamf_channel_sr7])
    return IAMF_ERR_INTERNAL;

  trace("---- s5to7 ----");
  debug("Typeid %d, Lasttypeid %d", Typeid, last_Typeid);

  l = &ths->large_buffer[CH_MX_S_L * fs];
  r = &ths->large_buffer[CH_MX_S_R * fs];

  for (; i < ths->skip; i++) {
    l[i] = (ths->ch_data[ck_iamf_channel_sl5][i] -
            ths->ch_data[ck_iamf_channel_sl7][i] *
                demixing_type_mat[last_Typeid].alpha) /
           demixing_type_mat[last_Typeid].beta;
    r[i] = (ths->ch_data[ck_iamf_channel_sr5][i] -
            ths->ch_data[ck_iamf_channel_sr7][i] *
                demixing_type_mat[last_Typeid].alpha) /
           demixing_type_mat[last_Typeid].beta;
  }

  for (; i < ths->frame_size; i++) {
    l[i] = (ths->ch_data[ck_iamf_channel_sl5][i] -
            ths->ch_data[ck_iamf_channel_sl7][i] *
                demixing_type_mat[Typeid].alpha) /
           demixing_type_mat[Typeid].beta;
    r[i] = (ths->ch_data[ck_iamf_channel_sr5][i] -
            ths->ch_data[ck_iamf_channel_sr7][i] *
                demixing_type_mat[Typeid].alpha) /
           demixing_type_mat[Typeid].beta;
  }

  ths->ch_data[ck_iamf_channel_bl7] = l;
  ths->ch_data[ck_iamf_channel_br7] = r;

  debug(
      "reconstructed channel %s(%d) at %p, channel %s(%d) at %p, buffer at "
      "%p",
      iamf_channel_name(ck_iamf_channel_bl7), ck_iamf_channel_bl7,
      ths->ch_data[ck_iamf_channel_bl7], iamf_channel_name(ck_iamf_channel_br7),
      ck_iamf_channel_br7, ths->ch_data[ck_iamf_channel_br7],
      ths->large_buffer);
  return 0;
}

/**
 * TF2toT2 de-mixer: Ltf2 = Ltf3 - w(k) x (L3 - L5) and
 *                   Rtf2 = Rtf3 - w(k) x (R3 - R5)
 * */
static int dmx_h2(Demixer *ths) {
  float *l, *r;
  float w, lastW;
  uint32_t fs = ths->frame_size;
  int i = 0;

  int Typeid = ths->demixing_mode;
  int last_Typeid = ths->last_dmixtypenum;

  if (ths->ch_data[ck_iamf_channel_hr]) return 0;
  if (!ths->ch_data[ck_iamf_channel_tl] || !ths->ch_data[ck_iamf_channel_tr])
    return IAMF_ERR_INTERNAL;
  if (dmx_s5(ths)) return IAMF_ERR_INTERNAL;

  w = get_w(ths->weight_state_idx);
  lastW = get_w(ths->last_weight_state_idx);

  trace("---- hf2to2 ----");
  debug("Typeid %d, w %f,, Lasttypeid %d, lastW %f", Typeid, w, last_Typeid,
        lastW);

  l = &ths->large_buffer[CH_MX_T_L * fs];
  r = &ths->large_buffer[CH_MX_T_R * fs];

  for (; i < ths->skip; i++) {
    l[i] = ths->ch_data[ck_iamf_channel_tl][i] -
           demixing_type_mat[last_Typeid].delta * lastW *
               ths->ch_data[ck_iamf_channel_sl5][i];
    r[i] = ths->ch_data[ck_iamf_channel_tr][i] -
           demixing_type_mat[last_Typeid].delta * lastW *
               ths->ch_data[ck_iamf_channel_sr5][i];
  }

  for (; i < ths->frame_size; i++) {
    l[i] = ths->ch_data[ck_iamf_channel_tl][i] -
           demixing_type_mat[Typeid].delta * w *
               ths->ch_data[ck_iamf_channel_sl5][i];
    r[i] = ths->ch_data[ck_iamf_channel_tr][i] -
           demixing_type_mat[Typeid].delta * w *
               ths->ch_data[ck_iamf_channel_sr5][i];
  }

  ths->ch_data[ck_iamf_channel_hl] = l;
  ths->ch_data[ck_iamf_channel_hr] = r;

  trace("channel %s(%d) at %p, channel %s(%d) at %p, buffer at %p",
        iamf_channel_name(ck_iamf_channel_hl), ck_iamf_channel_hl,
        ths->ch_data[ck_iamf_channel_hl], iamf_channel_name(ck_iamf_channel_hr),
        ck_iamf_channel_hr, ths->ch_data[ck_iamf_channel_hr],
        ths->large_buffer);
  return 0;
}

/**
 * Ltb = 1/γ(k) x (Ltf2 - Ltf4) and Rtb = 1/γ(k) x (Rtf2 - Rtf4)
 * */
static int dmx_h4(Demixer *ths) {
  float *l, *r;
  uint32_t fs = ths->frame_size;
  int i = 0;
  int Typeid = ths->demixing_mode;
  int last_Typeid = ths->last_dmixtypenum;

  if (ths->ch_data[ck_iamf_channel_hbr]) return 0;
  if (dmx_h2(ths)) return IAMF_ERR_INTERNAL;
  if (!ths->ch_data[ck_iamf_channel_hfr] || !ths->ch_data[ck_iamf_channel_hfl])
    return IAMF_ERR_INTERNAL;

  trace("---- h2to4 ----");
  debug("Typeid %d, Lasttypeid %d", Typeid, last_Typeid);

  l = &ths->large_buffer[CH_MX_T_L * fs];
  r = &ths->large_buffer[CH_MX_T_R * fs];

  for (; i < ths->skip; i++) {
    l[i] = (ths->ch_data[ck_iamf_channel_hl][i] -
            ths->ch_data[ck_iamf_channel_hfl][i]) /
           demixing_type_mat[last_Typeid].gamma;
    r[i] = (ths->ch_data[ck_iamf_channel_hr][i] -
            ths->ch_data[ck_iamf_channel_hfr][i]) /
           demixing_type_mat[last_Typeid].gamma;
  }

  for (; i < ths->frame_size; i++) {
    l[i] = (ths->ch_data[ck_iamf_channel_hl][i] -
            ths->ch_data[ck_iamf_channel_hfl][i]) /
           demixing_type_mat[Typeid].gamma;
    r[i] = (ths->ch_data[ck_iamf_channel_hr][i] -
            ths->ch_data[ck_iamf_channel_hfr][i]) /
           demixing_type_mat[Typeid].gamma;
  }

  ths->ch_data[ck_iamf_channel_hbl] = l;
  ths->ch_data[ck_iamf_channel_hbr] = r;

  trace("channel %s(%d) at %p, channel %s(%d) at %p, buffer at %p",
        iamf_channel_name(ck_iamf_channel_hbl), ck_iamf_channel_hbl,
        ths->ch_data[ck_iamf_channel_hbl],
        iamf_channel_name(ck_iamf_channel_hbr), ck_iamf_channel_hbr,
        ths->ch_data[ck_iamf_channel_hbr], ths->large_buffer);
  return 0;
}

static int dmx_channel(Demixer *ths, iamf_channel_t ch) {
  int ret = IAMF_ERR_INTERNAL;
  trace("demix channel %s(%d) pos %p", iamf_channel_name(ch), ch,
        ths->ch_data[ch]);

  if (ths->ch_data[ch]) {
    return IAMF_OK;
  }

  debug("reconstruct channel %s(%d)", iamf_channel_name(ch), ch);

  switch (ch) {
    case ck_iamf_channel_r2:
      ret = dmx_s2(ths);
      break;
    case ck_iamf_channel_l3:
    case ck_iamf_channel_r3:
      ret = dmx_s3(ths);
      break;
    case ck_iamf_channel_sl5:
    case ck_iamf_channel_sr5:
      ret = dmx_s5(ths);
      break;
    case ck_iamf_channel_bl7:
    case ck_iamf_channel_br7:
      ret = dmx_s7(ths);
      break;
    case ck_iamf_channel_hl:
    case ck_iamf_channel_hr:
      ret = dmx_h2(ths);
      break;
    case ck_iamf_channel_hbl:
    case ck_iamf_channel_hbr:
      ret = dmx_h4(ths);
      break;
    default:
      break;
  }
  return ret;
}

static void dmx_gainup(Demixer *ths) {
  for (int c = 0; c < ths->chs_gain_list.count; ++c) {
    for (int i = 0; i < ths->frame_size; ++i) {
      if (ths->ch_data[ths->chs_gain_list.ch_gain[c].ch]) {
        ths->ch_data[ths->chs_gain_list.ch_gain[c].ch][i] *=
            ths->chs_gain_list.ch_gain[c].gain;
      }
    }
  }
}

static int dmx_demix(Demixer *ths) {
  int chcnt = iamf_loudspeaker_layout_get_info(ths->layout)->channels;

  for (int c = 0; c < chcnt; ++c) {
    if (dmx_channel(ths, ths->chs_out[c]) < 0) {
      return IAMF_ERR_INTERNAL;
    }
  }
  return IAMF_OK;
}

static void dmx_rms(Demixer *ths) {
  float N = 7;  // 7 frame
  float sf, sfavg;
  float filtBuf;
  float *out;
  iamf_channel_t ch;

  trace("---- demixer_equalizeRMS ----");

  for (int c = 0; c < ths->chs_recon_gain_list.count; c++) {
    ch = ths->chs_recon_gain_list.ch_recon_gain[c].ch;
    sf = ths->chs_recon_gain_list.ch_recon_gain[c].recon_gain;
    out = ths->ch_data[ch];

    if (N > 0) {
      sfavg = (2 / (N + 1)) * sf + (1 - 2 / (N + 1)) * ths->ch_last_sfavg[ch];
    } else {
      sfavg = sf;
    }

    debug("channel %s(%d) is smoothed within %f.", iamf_channel_name(ch), ch,
          sf);
    /* different scale factor in overapping area */
    for (int i = 0; i < ths->frame_size; i++) {
      filtBuf = ths->ch_last_sfavg[ch] * ths->stop_window[i] +
                sfavg * ths->start_window[i];
      out[i] *= filtBuf;
    }

    ths->ch_last_sf[ch] = sf;
    ths->ch_last_sfavg[ch] = sfavg;
  }
}

Demixer *demixer_open(uint32_t frame_size) {
  Demixer *ths = 0;
  int ec = IAMF_OK;
  ths = def_mallocz(Demixer, 1);
  if (ths) {
    int windowLen = frame_size / 8;

    ths->frame_size = frame_size;
    ths->layout = ck_iamf_loudspeaker_layout_none;

    ths->hanning_filter = def_malloc(float, windowLen);
    ths->start_window = def_malloc(float, frame_size);
    ths->stop_window = def_malloc(float, frame_size);
    ths->large_buffer = def_malloc(float, CH_MX_COUNT *frame_size);

    if (!ths->hanning_filter || !ths->start_window || !ths->stop_window ||
        !ths->large_buffer) {
      ec = IAMF_ERR_ALLOC_FAIL;
      error("%s : hanning window, start & stop windows.",
            iamf_error_code_string(ec));
      goto termination;
    }
    /**
     * init hanning window.
     * */
    for (int i = 0; i < windowLen; i++) {
      ths->hanning_filter[i] =
          (0.5 * (1.0 - cos(2.0 * M_PI * (double)i / (double)(windowLen - 1))));
    }

    for (int i = 0; i < frame_size; i++) {
      ths->start_window[i] = 1;
      ths->stop_window[i] = 0;
    }

    for (int i = 0; i < ck_iamf_channel_count; ++i) {
      ths->ch_last_sf[i] = 1.0;
      ths->ch_last_sfavg[i] = 1.0;
    }
  }

termination:

  if (ec != IAMF_OK) {
    demixer_close(ths);
    ths = 0;
  }
  return ths;
}

void demixer_close(Demixer *ths) {
  if (ths) {
    def_free(ths->hanning_filter);
    def_free(ths->start_window);
    def_free(ths->stop_window);
    def_free(ths->large_buffer);
    free(ths);
  }
}

int demixer_set_frame_offset(Demixer *ths, uint32_t offset) {
  int preskip = 0;
  int windowLen = ths->frame_size / 8;
  int overlapLen = windowLen / 2;

  preskip = offset % ths->frame_size;
  ths->skip = preskip;

  debug("demixer preskip %d, overlayLen %d", preskip, overlapLen);
  if (preskip + overlapLen > ths->frame_size) return IAMF_OK;

  for (int i = 0; i < preskip; i++) {
    ths->start_window[i] = 0;
    ths->stop_window[i] = 1;
  }

  for (int i = preskip, j = 0; j < overlapLen; i++, j++) {
    ths->start_window[i] = ths->hanning_filter[j];
    ths->stop_window[i] = ths->hanning_filter[j + overlapLen];
  }

  for (int i = preskip + overlapLen; i < ths->frame_size; i++) {
    ths->start_window[i] = 1;
    ths->stop_window[i] = 0;
  }
  return IAMF_OK;
}

int demixer_set_channel_layout(Demixer *ths, iamf_loudspeaker_layout_t layout) {
  if (iamf_loudspeaker_layout_get_rendering_channels(
          layout, ths->chs_out, def_max_audio_channels) > 0) {
    ths->layout = layout;
    return IAMF_OK;
  }
  return IAMF_ERR_BAD_ARG;
}

int demixer_set_channels_order(Demixer *ths, iamf_channel_t *chs, int count) {
  memcpy(ths->chs_in, chs, sizeof(iamf_channel_t) * count);
  ths->chs_count = count;
  return IAMF_OK;
}

int demixer_set_output_gain(Demixer *ths, iamf_channel_t *chs, float *gain,
                            int count) {
  for (int i = 0; i < count; ++i) {
    ths->chs_gain_list.ch_gain[i].ch = chs[i];
    ths->chs_gain_list.ch_gain[i].gain = gain[i];
    debug("channel %s(%d) gain is %f", iamf_channel_name(chs[i]), chs[i],
          gain[i]);
  }
  ths->chs_gain_list.count = count;
  return IAMF_OK;
}

int demixer_set_demixing_info(Demixer *ths, int mode, int w_idx) {
  if (mode < 0 || mode == 3 || mode > 6) return IAMF_ERR_BAD_ARG;

  if (w_idx < 0 || w_idx > 10) {
    debug("dmixtypenum: %d -> %d", ths->demixing_mode, mode);
    ths->last_dmixtypenum = ths->demixing_mode;
    ths->demixing_mode = mode;

    debug("last weight state index : %d -> %d", ths->last_weight_state_idx,
          ths->weight_state_idx);
    ths->last_weight_state_idx = ths->weight_state_idx;
    calc_w(demixing_type_mat[mode].w_idx_offset, ths->last_weight_state_idx,
           &ths->weight_state_idx);
    debug("weight state index : %d -> %d", ths->last_weight_state_idx,
          ths->weight_state_idx);
  } else {
    if (mode != ths->demixing_mode) {
      ths->last_dmixtypenum = ths->demixing_mode = mode;
      debug("set default demixing mode %d", mode);
    }
    if (ths->weight_state_idx != w_idx) {
      ths->last_weight_state_idx = ths->weight_state_idx = w_idx;
      debug("set default weight index %d", w_idx);
    }
  }

  return 0;
}

int demixer_set_recon_gain(Demixer *ths, int count, iamf_channel_t *chs,
                           float *recon_gain, uint32_t flags) {
  if (flags && flags ^ ths->chs_recon_gain_list.flags) {
    for (int i = 0; i < count; ++i) {
      ths->chs_recon_gain_list.ch_recon_gain[i].ch = chs[i];
    }
    ths->chs_recon_gain_list.count = count;
    ths->chs_recon_gain_list.flags = flags;
  }
  for (int i = 0; i < count; ++i) {
    ths->chs_recon_gain_list.ch_recon_gain[i].recon_gain = recon_gain[i];
  }
  return IAMF_OK;
}

int demixer_demixing(Demixer *ths, float *dst, float *src, uint32_t size) {
  iamf_channel_t ch;

  if (size != ths->frame_size) return IAMF_ERR_BAD_ARG;
  if (iamf_loudspeaker_layout_get_info(ths->layout)->channels != ths->chs_count)
    return IAMF_ERR_INTERNAL;

  memset(ths->ch_data, 0, sizeof(float *) * ck_iamf_channel_count);
  for (int c = 0; c < ths->chs_count; ++c) {
    ths->ch_data[ths->chs_in[c]] = src + size * c;
  }

  dmx_gainup(ths);
  if (dmx_demix(ths) < 0) return IAMF_ERR_INTERNAL;
  dmx_rms(ths);

  for (int c = 0; c < ths->chs_count; ++c) {
    ch = ths->chs_out[c];
    if (!ths->ch_data[ch]) {
      error("channel %s(%d) doesn't has data.", iamf_channel_name(ch), ch);
      continue;
    }
    trace("output channel %s(%d) at %p", iamf_channel_name(ch), ch,
          ths->ch_data[ch]);
    memcpy((void *)&dst[c * size], (void *)ths->ch_data[ch],
           sizeof(float) * size);
  }
  return IAMF_OK;
}
