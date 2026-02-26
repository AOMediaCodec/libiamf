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
 * @file iamf_element_reconstructor.c
 * @brief IAMF element reconstructor implementation.
 * @version 0.1
 * @date Created 12/11/2025
 **/

#include "iamf_element_reconstructor.h"

#include <stdlib.h>
#include <string.h>

#include "chashmap.h"
#include "clog.h"
#include "demixer.h"
#include "iamf_core_decoder.h"
#include "iamf_layout.h"
#include "iamf_private_definitions.h"
#include "iamf_string.h"
#include "iamf_types.h"

#undef def_log_tag
#define def_log_tag "IAMF_REC"

#define SRDBG 0
#if SRDBG
extern void iamf_rec_stream_log(int eid, int chs, float* in, int size);
extern void iamf_stream_log_free();
#endif

// =============================================================================
// Private Structure Definitions
// =============================================================================

typedef struct BaseReconstructor base_reconstructor_t;
typedef void (*fun_reconstructor_destroy_t)(base_reconstructor_t* self);
typedef iamf_audio_block_t* (*fun_reconstructor_process_frame_t)(
    base_reconstructor_t* self, int flush);

struct BaseReconstructor {
  const iamf_audio_element_obu_t* audio_element_obu;
  iamf_audio_element_type_t type;

  uint32_t num_samples_per_frame;

  uint32_t num_audio_streams;
  uint32_t num_audio_channels;
  uint32_t num_audio_frames;
  uint32_t delay;

  uint32_t audio_frame_ids[def_max_audio_streams];
  iamf_audio_frame_obu_t* audio_frame_obus[def_max_audio_streams];

  hash_map_t* audio_frames_map;  // substream_id(uint32_t) : audio_frame_obu_t*

  fun_reconstructor_destroy_t destroy;
  fun_reconstructor_process_frame_t process_frame;
};

typedef struct SceneBasedReconstructor {
  base_reconstructor_t base;
  iamf_core_decoder_t* core_decoder;
} scene_based_reconstructor_t;

typedef struct ChannelBasedReconstructor {
  base_reconstructor_t base;

  iamf_core_decoder_t* core_decoders[def_max_channel_groups];
  uint8_t num_groups;
  struct {
    iamf_loudspeaker_layout_t layout;
    int stream_start_index;
    int num_streams;
    int num_channels;
  } group_infos[def_max_channel_groups];

  int target_layout_index;
  Demixer* demixer;
} channel_based_reconstructor_t;

typedef struct ObjectBasedReconstructor {
  base_reconstructor_t base;
  iamf_core_decoder_t* core_decoder;
} object_based_reconstructor_t;

// =============================================================================
// Public Interface Implementation
// =============================================================================

// Full definition of the iamf_element_reconstructor_t structure.
typedef struct IamfElementReconstructor {
  hash_map_t* reconstructor_map;  // element_id(uint32_t) : base_reconstructor_t
  hash_map_t*
      audio_frame_obus;  // substream id(uint32_t) : element_id(uint32_t)
} iamf_element_reconstructor_t;

// =============================================================================
// Private Function Definitions
// =============================================================================

static iamf_core_decoder_t* _core_decoder_open(
    stream_mode_t mode, int channels, int nb_streams, int nb_coupled_streams,
    uint8_t* mapping, int mapping_size, const iamf_codec_config_obu_t* conf) {
  iamf_core_decoder_t* dec;
  int ret = 0;

  dec = iamf_core_decoder_open(conf->codec_id);

  if (dec) {
    iamf_core_decoder_set_codec_conf(dec, conf->decoder_config->data,
                                     conf->decoder_config->size);
    debug(
        "codec %s, mode %d, channels %d, streams %d, coupled streams %d, "
        "mapping size  %d",
        iamf_codec_type_string(iamf_codec_type_get(conf->codec_id)), mode,
        channels, nb_streams, nb_coupled_streams, mapping_size);
    iamf_core_decoder_set_streams_info(dec, mode, channels, nb_streams,
                                       nb_coupled_streams, mapping,
                                       mapping_size);
    ret = iamf_core_decoder_init(dec);
    if (ret != IAMF_OK) {
      error("Fail to initalize core decoder.");
      iamf_core_decoder_close(dec);
      dec = 0;
    }
  }

  return dec;
}

static int _core_decoder_decode(iamf_core_decoder_t* dec,
                                iamf_audio_frame_obu_t** audio_frame_obus,
                                uint32_t num_obus, iamf_audio_block_t* ablock,
                                int flush) {
  int samples = 0;
  uint8_t* buffers[def_max_audio_streams] = {0};
  uint32_t sizes[def_max_audio_streams] = {0};

  if (!dec || !ablock) return IAMF_ERR_BAD_ARG;

  if (!flush) {
    for (int i = 0; i < num_obus; ++i) {
      buffers[i] = audio_frame_obus[i]->audio_frame->data;
      sizes[i] = audio_frame_obus[i]->audio_frame->size;
    }
  }

  samples =
      iamf_core_decoder_decode(dec, buffers, sizes, num_obus, ablock->data,
                               ablock->capacity_per_channel);
  if (samples < 0) {
    error("Fail to decode core audio data: %d", samples);
    return samples;
  }

  ablock->num_samples_per_channel = samples;
  if (samples < ablock->capacity_per_channel) {
    iamf_audio_block_padding(ablock, ablock->capacity_per_channel - samples);
    samples = ablock->capacity_per_channel;
  }

  return samples;
}

static int _reconstructor_init(base_reconstructor_t* self,
                               const iamf_audio_element_obu_t* aeo,
                               const iamf_codec_config_obu_t* cco) {
  self->audio_element_obu = (iamf_audio_element_obu_t*)aeo;
  self->type = aeo->audio_element_type;
  self->num_samples_per_frame = cco->num_samples_per_frame;
  self->num_audio_frames = 0;
  self->num_audio_streams = array_size(aeo->audio_substream_ids);
  self->audio_frames_map = hash_map_new(def_hash_map_capacity_audio_frames);
  if (!self->audio_frames_map) return IAMF_ERR_ALLOC_FAIL;
  for (int i = 0; i < self->num_audio_streams; ++i) {
    self->audio_frame_ids[i] =
        def_value_wrap_u32(array_at(aeo->audio_substream_ids, i));
    hash_map_put(self->audio_frames_map, self->audio_frame_ids[i],
                 def_value_wrap_instance_ptr(&self->audio_frame_obus[i]));
  }

  if (aeo->audio_element_type == ck_audio_element_type_channel_based) {
    const channel_based_audio_element_obu_t* cbo =
        (const channel_based_audio_element_obu_t*)aeo;
    uint32_t channels = 0;
    for (int i = 0; i < cbo->max_valid_layers; ++i) {
      const obu_channel_layer_config_t* ctx =
          def_value_wrap_ptr(array_at(cbo->channel_audio_layer_configs, i));
      channels += (ctx->coupled_substream_count + ctx->substream_count);
    }
    self->num_audio_channels = channels;
  } else if (aeo->audio_element_type == ck_audio_element_type_scene_based) {
    const scene_based_audio_element_obu_t* sbo =
        (const scene_based_audio_element_obu_t*)aeo;
    self->num_audio_channels = sbo->output_channel_count;
  } else if (aeo->audio_element_type == ck_audio_element_type_object_based) {
    self->num_audio_channels = self->num_audio_streams;
  }

  return IAMF_OK;
}

static void _reconstructor_uninit(base_reconstructor_t* self) {
  if (self->audio_frames_map) hash_map_delete(self->audio_frames_map, 0);
}

static void _reconstructor_and_audio_block_update_delay(
    base_reconstructor_t* self, iamf_audio_block_t* block, uint32_t delay) {
  debug("update delay: delay %u <- %u, audio block skip %u, padding %u", delay,
        self->delay, block->skip, block->padding);
  if (delay > 0 && delay != self->delay &&
      block->skip + block->padding + delay < block->num_samples_per_channel) {
    block->second_skip = delay;
    self->delay = delay;
  }
}

static void _reconstructor_delete_all_audio_frame_obus(
    base_reconstructor_t* self) {
  if (!self) return;
  int n = hash_map_size(self->audio_frames_map);
  for (int i = 0; i < n; ++i)
    if (self->audio_frame_obus[i])
      iamf_audio_frame_obu_free(self->audio_frame_obus[i]);
  memset(self->audio_frame_obus, 0, sizeof(self->audio_frame_obus));
  self->num_audio_frames = 0;
}

static int _get_new_channels(iamf_loudspeaker_layout_t last,
                             iamf_loudspeaker_layout_t cur,
                             iamf_channel_t* new_chs, uint32_t count) {
  uint32_t chs = 0;

  /**
   * In ChannelGroup for Channel audio: The order conforms to following rules:
   *
   * @ Coupled Substream(s) comes first and followed by non-coupled
   * Substream(s).
   * @ Coupled Substream(s) for surround channels comes first and followed by
   * one(s) for top channels.
   * @ Coupled Substream(s) for front channels comes first and followed by
   * one(s) for side, rear and back channels.
   * @ Coupled Substream(s) for side channels comes first and followed by one(s)
   * for rear channels.
   * @ Center channel comes first and followed by LFE and followed by the other
   * one.
   * */

  if (last == ck_iamf_loudspeaker_layout_none) {
    chs = iamf_loudspeaker_layout_get_decoding_channels(cur, new_chs, count);
  } else if (iamf_audio_layer_base_layout_check(last) &&
             iamf_audio_layer_base_layout_check(cur)) {
    const iamf_layout_info_t* info1 = iamf_loudspeaker_layout_get_info(last);
    const iamf_layout_info_t* info2 = iamf_loudspeaker_layout_get_info(cur);
    uint32_t s1 = info1->surround;
    uint32_t s2 = info2->surround;
    uint32_t t1 = info1->height;
    uint32_t t2 = info2->height;

    if (s1 < 5 && 5 <= s2) {
      new_chs[chs++] = ck_iamf_channel_l5;
      new_chs[chs++] = ck_iamf_channel_r5;
      debug("new channels : l5/r5(l7/r7)");
    }
    if (s1 < 7 && 7 <= s2) {
      new_chs[chs++] = ck_iamf_channel_sl7;
      new_chs[chs++] = ck_iamf_channel_sr7;
      debug("new channels : sl7/sr7");
    }
    if (t2 != t1 && t2 == 4) {
      new_chs[chs++] = ck_iamf_channel_hfl;
      new_chs[chs++] = ck_iamf_channel_hfr;
      debug("new channels : hfl/hfr");
    }
    if (t2 - t1 == 4) {
      new_chs[chs++] = ck_iamf_channel_hbl;
      new_chs[chs++] = ck_iamf_channel_hbr;
      debug("new channels : hbl/hbr");
    } else if (!t1 && t2 - t1 == 2) {
      if (s2 < 5) {
        new_chs[chs++] = ck_iamf_channel_tl;
        new_chs[chs++] = ck_iamf_channel_tr;
        debug("new channels : tl/tr");
      } else {
        new_chs[chs++] = ck_iamf_channel_hl;
        new_chs[chs++] = ck_iamf_channel_hr;
        debug("new channels : hl/hr");
      }
    }

    if (s1 < 3 && 3 <= s2) {
      new_chs[chs++] = ck_iamf_channel_c;
      new_chs[chs++] = ck_iamf_channel_lfe;
      debug("new channels : c/lfe");
    }
    if (s1 < 2 && 2 <= s2) {
      new_chs[chs++] = ck_iamf_channel_l2;
      debug("new channel : l2");
    }
  }

  if (chs > count) {
    error("too much new channels %d, we only need less than %d channels", chs,
          count);
    chs = IAMF_ERR_BUFFER_TOO_SMALL;
  }
  return chs;
}

static int _get_target_layout_channels_order(channel_based_reconstructor_t* cbr,
                                             iamf_channel_t* order, int max) {
  int chs = 0;
  iamf_loudspeaker_layout_t type = ck_iamf_loudspeaker_layout_none;

  for (int i = 0; i <= cbr->target_layout_index; ++i) {
    if (i) type = cbr->group_infos[i - 1].layout;
    chs += _get_new_channels(type, cbr->group_infos[i].layout, &order[chs],
                             max - chs);
  }
  return chs;
}

static iamf_channel_t _get_output_gain_channel(iamf_loudspeaker_layout_t type,
                                               iamf_output_gain_channel_t gch) {
  iamf_channel_t ch = ck_iamf_channel_none;
  const iamf_layout_info_t* info = iamf_loudspeaker_layout_get_info(type);
  switch (gch) {
    case ck_iamf_channel_gain_l: {
      switch (type) {
        case ck_iamf_loudspeaker_layout_mono:
          ch = ck_iamf_channel_mono;
          break;
        case ck_iamf_loudspeaker_layout_stereo:
          ch = ck_iamf_channel_l2;
          break;
        case ck_iamf_loudspeaker_layout_312:
          ch = ck_iamf_channel_l3;
          break;
        default:
          break;
      }
    } break;

    case ck_iamf_channel_gain_r: {
      switch (type) {
        case ck_iamf_loudspeaker_layout_stereo:
          ch = ck_iamf_channel_r2;
          break;
        case ck_iamf_loudspeaker_layout_312:
          ch = ck_iamf_channel_r3;
          break;
        default:
          break;
      }
    } break;

    case ck_iamf_channel_gain_ls:
      if (info->surround == 5) ch = ck_iamf_channel_sl5;
      break;

    case ck_iamf_channel_gain_rs:
      if (info->surround == 5) ch = ck_iamf_channel_sr5;
      break;

    case ck_iamf_channel_gain_ltf:
      ch = info->surround < 5 ? ck_iamf_channel_tl : ck_iamf_channel_hl;
      break;

    case ck_iamf_channel_gain_rtf:
      ch = info->surround < 5 ? ck_iamf_channel_tr : ck_iamf_channel_hr;
      break;

    default:
      break;
  }

  return ch;
}

static int _get_all_channels_output_gains(channel_based_reconstructor_t* cbr,
                                          int channels_map[], float gains[],
                                          int max) {
  const channel_based_audio_element_obu_t* cbo =
      (const channel_based_audio_element_obu_t*)cbr->base.audio_element_obu;
  int count = 0;
  for (int l = 0; l < cbr->num_groups; ++l) {
    obu_channel_layer_config_t* config =
        def_value_wrap_ptr(array_at(cbo->channel_audio_layer_configs, l));
    if (config->output_gain_is_present_flag) {
      uint32_t flags = config->output_gain_info.output_gain_flag;
      for (int c = 0; c < ck_iamf_channel_gain_count; ++c) {
        if (flags & def_rshift(c)) {
          channels_map[count] =
              _get_output_gain_channel(config->loudspeaker_layout, c);
          if (channels_map[count] != ck_iamf_channel_none) {
            gains[count++] = config->output_gain_info.output_gain_linear;
          }
        }
      }
    }
  }

  return count;
}

static int _set_default_demixing_info(channel_based_reconstructor_t* cbr) {
  const iamf_audio_element_obu_t* aeo = cbr->base.audio_element_obu;
  int mode, weight_idx;
  int n = array_size(aeo->parameters);
  int i = 0;
  for (; i < n; ++i) {
    parameter_subblock_t* pb =
        def_value_wrap_optional_ptr(array_at(aeo->parameters, i));
    if (pb && pb->type == ck_iamf_parameter_type_demixing) {
      demixing_info_parameter_base_t* dp = (demixing_info_parameter_base_t*)pb;
      mode = dp->dmixp_mode;
      weight_idx = dp->default_w;
      break;
    }
  }
  return i < n ? demixer_set_demixing_info(cbr->demixer, mode, weight_idx)
               : IAMF_ERR_INTERNAL;
}

static uint32_t _get_recon_channels_flags(iamf_loudspeaker_layout_t l1,
                                          iamf_loudspeaker_layout_t l2) {
  uint32_t s1, s2, t1, t2, flags;
  const iamf_layout_info_t *info1, *info2;

  if (l1 == l2) return 0;

  info1 = iamf_loudspeaker_layout_get_info(l1);
  info2 = iamf_loudspeaker_layout_get_info(l2);

  s1 = info1->surround;
  s2 = info2->surround;
  t1 = info1->height;
  t2 = info2->height;
  flags = 0;

  if (s1 != s2) {
    if (s2 <= 3) {
      flags |= def_rshift(ck_iamf_channel_recon_l);
      flags |= def_rshift(ck_iamf_channel_recon_r);
    } else if (s2 == 5) {
      flags |= def_rshift(ck_iamf_channel_recon_ls);
      flags |= def_rshift(ck_iamf_channel_recon_rs);
    } else if (s2 == 7) {
      flags |= def_rshift(ck_iamf_channel_recon_lb);
      flags |= def_rshift(ck_iamf_channel_recon_rb);
    }
  }

  if (t2 != t1 && t2 == 4) {
    flags |= def_rshift(ck_iamf_channel_recon_ltb);
    flags |= def_rshift(ck_iamf_channel_recon_rtb);
  }

  if (s2 == 5 && t1 && t2 == t1) {
    flags |= def_rshift(ck_iamf_channel_recon_ltf);
    flags |= def_rshift(ck_iamf_channel_recon_rtf);
  }

  return flags;
}

static int _get_recon_channels_order(iamf_loudspeaker_layout_t layout,
                                     uint32_t flags, iamf_channel_t* order,
                                     int channels) {
  int chs = 0;
  static iamf_recon_channel_t recon_channel_order[] = {
      ck_iamf_channel_recon_l,   ck_iamf_channel_recon_c,
      ck_iamf_channel_recon_r,   ck_iamf_channel_recon_ls,
      ck_iamf_channel_recon_rs,  ck_iamf_channel_recon_ltf,
      ck_iamf_channel_recon_rtf, ck_iamf_channel_recon_lb,
      ck_iamf_channel_recon_rb,  ck_iamf_channel_recon_ltb,
      ck_iamf_channel_recon_rtb, ck_iamf_channel_recon_lfe};

  static iamf_channel_t channel_layout_map
      [ck_iamf_loudspeaker_layout_count][ck_iamf_channel_recon_count] = {
          {ck_iamf_channel_mono, ck_iamf_channel_none, ck_iamf_channel_none,
           ck_iamf_channel_none, ck_iamf_channel_none, ck_iamf_channel_none,
           ck_iamf_channel_none, ck_iamf_channel_none, ck_iamf_channel_none,
           ck_iamf_channel_none, ck_iamf_channel_none, ck_iamf_channel_none},
          {ck_iamf_channel_l2, ck_iamf_channel_none, ck_iamf_channel_r2,
           ck_iamf_channel_none, ck_iamf_channel_none, ck_iamf_channel_none,
           ck_iamf_channel_none, ck_iamf_channel_none, ck_iamf_channel_none,
           ck_iamf_channel_none, ck_iamf_channel_none, ck_iamf_channel_none},
          {ck_iamf_channel_l5, ck_iamf_channel_c, ck_iamf_channel_r5,
           ck_iamf_channel_sl5, ck_iamf_channel_sr5, ck_iamf_channel_none,
           ck_iamf_channel_none, ck_iamf_channel_none, ck_iamf_channel_none,
           ck_iamf_channel_none, ck_iamf_channel_none, ck_iamf_channel_lfe},
          {ck_iamf_channel_l5, ck_iamf_channel_c, ck_iamf_channel_r5,
           ck_iamf_channel_sl5, ck_iamf_channel_sr5, ck_iamf_channel_hl,
           ck_iamf_channel_hr, ck_iamf_channel_none, ck_iamf_channel_none,
           ck_iamf_channel_none, ck_iamf_channel_none, ck_iamf_channel_lfe},
          {ck_iamf_channel_l5, ck_iamf_channel_c, ck_iamf_channel_r5,
           ck_iamf_channel_sl5, ck_iamf_channel_sr5, ck_iamf_channel_hfl,
           ck_iamf_channel_hfr, ck_iamf_channel_none, ck_iamf_channel_none,
           ck_iamf_channel_hbl, ck_iamf_channel_hbr, ck_iamf_channel_lfe},
          {ck_iamf_channel_l7, ck_iamf_channel_c, ck_iamf_channel_r7,
           ck_iamf_channel_sl7, ck_iamf_channel_sr7, ck_iamf_channel_none,
           ck_iamf_channel_none, ck_iamf_channel_bl7, ck_iamf_channel_br7,
           ck_iamf_channel_none, ck_iamf_channel_none, ck_iamf_channel_lfe},
          {ck_iamf_channel_l7, ck_iamf_channel_c, ck_iamf_channel_r7,
           ck_iamf_channel_sl7, ck_iamf_channel_sr7, ck_iamf_channel_hl,
           ck_iamf_channel_hr, ck_iamf_channel_bl7, ck_iamf_channel_br7,
           ck_iamf_channel_none, ck_iamf_channel_none, ck_iamf_channel_lfe},
          {ck_iamf_channel_l7, ck_iamf_channel_c, ck_iamf_channel_r7,
           ck_iamf_channel_sl7, ck_iamf_channel_sr7, ck_iamf_channel_hfl,
           ck_iamf_channel_hfr, ck_iamf_channel_bl7, ck_iamf_channel_br7,
           ck_iamf_channel_hbl, ck_iamf_channel_hbr, ck_iamf_channel_lfe},
          {ck_iamf_channel_l3, ck_iamf_channel_c, ck_iamf_channel_r3,
           ck_iamf_channel_none, ck_iamf_channel_none, ck_iamf_channel_tl,
           ck_iamf_channel_tr, ck_iamf_channel_none, ck_iamf_channel_none,
           ck_iamf_channel_none, ck_iamf_channel_none, ck_iamf_channel_lfe}};

#define def_recon_channel_flag(c) def_rshift(c)

  for (int c = 0; c < ck_iamf_channel_recon_count; ++c) {
    if (flags & def_recon_channel_flag(recon_channel_order[c])) {
      order[chs++] = channel_layout_map[layout][recon_channel_order[c]];
      if (chs >= channels) break;
    }
  }

  return IAMF_OK;
}

static int _set_default_recon_gain(channel_based_reconstructor_t* cbr) {
  if (cbr->target_layout_index > 0) {
    iamf_loudspeaker_layout_t layout = cbr->group_infos[0].layout;
    iamf_loudspeaker_layout_t target_layout =
        cbr->group_infos[cbr->target_layout_index].layout;
    uint32_t flags = _get_recon_channels_flags(layout, target_layout);
    debug("first layer %d, target layout %d", layout, target_layout);
    debug("default recon gain flags 0x%04x", flags);
    int nb_channels = bit1_count(flags);
    debug("default channel count %d", nb_channels);
    iamf_channel_t order[ck_iamf_channel_recon_count];
    float recon_gain[ck_iamf_channel_recon_count];
    _get_recon_channels_order(target_layout, flags, order,
                              ck_iamf_channel_recon_count);
    for (int i = 0; i < nb_channels; ++i) {
      recon_gain[i] = def_default_recon_gain;
      debug("channel %s(%d) : default recon gain is 1.",
            iamf_channel_name(order[i]), order[i]);
    }
    demixer_set_recon_gain(cbr->demixer, nb_channels, order, recon_gain, flags);
  }

  return IAMF_OK;
}

static int _set_channel_based_target_layout(channel_based_reconstructor_t* cbr,
                                            iamf_loudspeaker_layout_t layout) {
  iamf_channel_t channels_map[def_max_audio_channels];
  int channels = 0;

  // Only configure demixer if it exists (num_groups > 1)
  if (cbr->demixer) {
    demixer_set_channel_layout(
        cbr->demixer, cbr->group_infos[cbr->target_layout_index].layout);
    channels = _get_target_layout_channels_order(cbr, channels_map,
                                                 def_max_audio_channels);
    demixer_set_channels_order(cbr->demixer, channels_map, channels);

    _set_default_recon_gain(cbr);
  }

  return IAMF_OK;
}

static void _iamf_audio_block_trimming_info_update(
    iamf_audio_block_t* ablock, iamf_audio_frame_obu_t* afo) {
  if (!ablock || !afo) return;
  ablock->skip += afo->num_samples_to_trim_at_start;
  ablock->padding += afo->num_samples_to_trim_at_end;
}

static int _demixer_config(Demixer* demixer,
                           channel_based_reconstructor_t* cbr) {
  iamf_channel_t channels_map[def_max_audio_channels];
  int channels = 0;
  float gains[def_max_audio_channels];

  demixer_set_frame_offset(demixer, 0);

  channels = _get_all_channels_output_gains(cbr, (int*)channels_map, gains,
                                            def_max_audio_channels);
  demixer_set_output_gain(demixer, channels_map, gains, channels);

  _set_default_demixing_info(cbr);
  return IAMF_OK;
}

static iamf_audio_block_t* _demixer_reconstruct_stream(
    Demixer* demixer, iamf_audio_block_t* ablock) {
  iamf_audio_block_t* abk = iamf_audio_block_new(
      ablock->id, ablock->capacity_per_channel, ablock->num_channels);
  if (!abk) return 0;
  demixer_demixing(demixer, abk->data, ablock->data,
                   ablock->num_samples_per_channel);
  iamf_audio_block_samples_info_copy(abk, ablock);
  return abk;
}

static iamf_audio_block_t*
_channel_based_reconstructor_reconstruct_element_stream(
    channel_based_reconstructor_t* self, int flush) {
  const iamf_layout_info_t* info = iamf_loudspeaker_layout_get_info(
      self->group_infos[self->target_layout_index].layout);
  uint32_t channels = info->channels;
  iamf_audio_block_t* ablock = 0;
  int num_obus = 0;

  if (info->flags & ck_iamf_layout_flag_subset) {
    const iamf_layout_info_t* reference_info =
        iamf_loudspeaker_layout_get_info(info->reference_layout);
    channels = reference_info->channels;
  }

  num_obus = 1 == self->num_groups ? self->group_infos[0].num_streams
                                   : self->group_infos[1].stream_start_index;

  ablock = iamf_audio_block_new(self->base.audio_element_obu->audio_element_id,
                                self->base.num_samples_per_frame, channels);
  if (!ablock) {
    error("Failed to allocate audio block in channel-based reconstructor %u.",
          self->base.audio_element_obu->audio_element_id);
    return 0;
  }

  if (_core_decoder_decode(self->core_decoders[0], self->base.audio_frame_obus,
                           num_obus, ablock, flush) < 0) {
    error("Failed to decode audio data in channel-based reconstructor %u.",
          self->base.audio_element_obu->audio_element_id);
    iamf_audio_block_delete(ablock);
    ablock = 0;
  } else {
    _iamf_audio_block_trimming_info_update(ablock,
                                           self->base.audio_frame_obus[0]);
    _reconstructor_and_audio_block_update_delay(
        &self->base, ablock,
        iamf_core_decoder_get_delay(self->core_decoders[0]));
    iamf_audio_block_reorder(ablock, info->decoding_map, info->channels);
  }

  return ablock;
}

static iamf_audio_block_t*
_channel_based_reconstructor_reconstruct_element_scalable_stream(
    channel_based_reconstructor_t* self, int flush) {
  const iamf_layout_info_t* info = iamf_loudspeaker_layout_get_info(
      self->group_infos[self->target_layout_index].layout);
  uint32_t channels = info->channels;
  iamf_audio_block_t* ablock = 0;
  iamf_audio_block_t* ablocks[def_max_audio_streams] = {0};
  int ret = IAMF_OK;
  int start_index = 0, end_next_index = 0;

  ablock = iamf_audio_block_new(self->base.audio_element_obu->audio_element_id,
                                self->base.num_samples_per_frame, channels);
  if (!ablock) {
    error("Failed to allocate audio block in channel-based reconstructor %u.",
          self->base.audio_element_obu->audio_element_id);
    return 0;
  }

  for (int i = 0; i <= self->target_layout_index; ++i) {
    start_index = end_next_index;
    end_next_index = i == (self->num_groups - 1)
                         ? self->base.num_audio_streams
                         : self->group_infos[i + 1].stream_start_index;

    ablocks[i] = iamf_audio_block_new(
        self->base.audio_element_obu->audio_element_id,
        self->base.num_samples_per_frame, self->group_infos[i].num_channels);
    if (!ablocks[i]) {
      error("Failed to allocate audio block in channel-based reconstructor %u.",
            self->base.audio_element_obu->audio_element_id);
      ret = IAMF_ERR_ALLOC_FAIL;
      break;
    }

    ret = _core_decoder_decode(self->core_decoders[i],
                               &self->base.audio_frame_obus[start_index],
                               end_next_index - start_index, ablocks[i], flush);

    if (ret < 0) {
      error("Failed to decode audio data in channel-based reconstructor %u.",
            self->base.audio_element_obu->audio_element_id);
      break;
    }
  }

  if (ret > 0) {
    iamf_audio_block_t* tmp = ablock;

    iamf_audio_block_channels_concat(tmp, ablocks,
                                     self->target_layout_index + 1);
#if SRDBG
    // iamf_rec_stream_log(tmp->id, tmp->num_channels, tmp->data,
    //                     tmp->capacity_per_channel);
#endif

    _iamf_audio_block_trimming_info_update(ablock,
                                           self->base.audio_frame_obus[0]);
    _reconstructor_and_audio_block_update_delay(
        &self->base, ablock,
        iamf_core_decoder_get_delay(self->core_decoders[0]));

    if (ablock->second_skip)
      demixer_set_frame_offset(self->demixer, ablock->second_skip);
    ablock = _demixer_reconstruct_stream(self->demixer, tmp);
    iamf_audio_block_delete(tmp);

#if SRDBG
    iamf_rec_stream_log(ablock->id, ablock->num_channels, ablock->data,
                        ablock->capacity_per_channel);
#endif

  } else {
    if (ablock) iamf_audio_block_delete(ablock);
    ablock = 0;
  }

  for (int i = 0; i <= self->target_layout_index; ++i)
    iamf_audio_block_delete(ablocks[i]);

  return ablock;
}

static void _channel_based_reconstructor_delete(
    channel_based_reconstructor_t* self) {
  if (!self) return;

  _reconstructor_uninit(&self->base);

  for (int i = 0; i < self->num_groups; ++i) {
    if (self->core_decoders[i]) iamf_core_decoder_close(self->core_decoders[i]);
  }
  if (self->demixer) demixer_close(self->demixer);

  def_free(self);
}

static iamf_audio_block_t* _channel_based_reconstructor_process_frame(
    channel_based_reconstructor_t* self, int flush) {
  return self->num_groups == 1
             ? _channel_based_reconstructor_reconstruct_element_stream(self,
                                                                       flush)
             : _channel_based_reconstructor_reconstruct_element_scalable_stream(
                   self, flush);
}

static channel_based_reconstructor_t* _channel_based_reconstructor_new(
    const iamf_audio_element_obu_t* aeo, const iamf_codec_config_obu_t* cco) {
  const channel_based_audio_element_obu_t* cbo =
      (const channel_based_audio_element_obu_t*)aeo;
  int stream_start_index = 0;
  channel_based_reconstructor_t* cbr =
      def_mallocz(channel_based_reconstructor_t, 1);
  if (!cbr) {
    def_err_msg_enomem("Channel Based Reconstructor", "Reconstructor");
    return 0;
  }

  if (_reconstructor_init(&cbr->base, aeo, cco) != IAMF_OK) {
    _channel_based_reconstructor_delete(cbr);
    return 0;
  }

  cbr->base.destroy =
      (fun_reconstructor_destroy_t)_channel_based_reconstructor_delete;
  cbr->base.process_frame = (fun_reconstructor_process_frame_t)
      _channel_based_reconstructor_process_frame;

  cbr->num_groups = cbo->max_valid_layers;
  cbr->target_layout_index = cbo->max_valid_layers - 1;

  for (int i = 0; i < cbr->num_groups; ++i) {
    obu_channel_layer_config_t* ctx =
        def_value_wrap_ptr(array_at(cbo->channel_audio_layer_configs, i));
    cbr->group_infos[i].layout = iamf_audio_layer_layout_get(
        ctx->loudspeaker_layout, ctx->expanded_loudspeaker_layout);
    cbr->group_infos[i].stream_start_index = stream_start_index;
    cbr->group_infos[i].num_streams = ctx->substream_count;
    cbr->group_infos[i].num_channels =
        ctx->substream_count + ctx->coupled_substream_count;

    stream_start_index += ctx->substream_count;

    cbr->core_decoders[i] = _core_decoder_open(
        ck_stream_mode_ambisonics_none,
        ctx->coupled_substream_count + ctx->substream_count,
        ctx->substream_count, ctx->coupled_substream_count, 0, 0, cco);
    if (!cbr->core_decoders[i]) {
      _channel_based_reconstructor_delete(cbr);
      return 0;
    }
  }

  if (cbr->num_groups > 1) {
    cbr->demixer = demixer_open(cco->num_samples_per_frame);
    if (!cbr->demixer) {
      def_err_msg_enomem("Demixer", "Reconstructor");
      _channel_based_reconstructor_delete(cbr);
      return 0;
    }

    info("open demixer.");
    _demixer_config(cbr->demixer, cbr);
  }

  info("make channel based reconstructor for element %u",
       aeo->audio_element_id);

  return cbr;
}

stream_mode_t _get_stream_mode_ambisonics(
    iamf_ambisonics_mode_t ambisonics_mode) {
  if (ambisonics_mode == ck_ambisonics_mode_mono)
    return ck_stream_mode_ambisonics_mono;
  else if (ambisonics_mode == ck_ambisonics_mode_projection)
    return ck_stream_mode_ambisonics_projection;
  return ck_stream_mode_ambisonics_none;
}

static void _scene_based_reconstructor_delete(
    scene_based_reconstructor_t* self) {
  if (!self) return;
  _reconstructor_uninit(&self->base);
  if (self->core_decoder) iamf_core_decoder_close(self->core_decoder);
  def_free(self);
}

static iamf_audio_block_t* _scene_based_reconstructor_process_frame(
    scene_based_reconstructor_t* self, int flush) {
  iamf_audio_block_t* ablock = iamf_audio_block_new(
      self->base.audio_element_obu->audio_element_id,
      self->base.num_samples_per_frame, self->base.num_audio_channels);
  debug("samples %u, channels %u, block %p", self->base.num_samples_per_frame,
        self->base.num_audio_channels, ablock);
  int ret =
      _core_decoder_decode(self->core_decoder, self->base.audio_frame_obus,
                           self->base.num_audio_streams, ablock, flush);
  if (ret < 0) {
    error("Failed to decode audio data in scene-based reconstructor %u.",
          self->base.audio_element_obu->audio_element_id);
    iamf_audio_block_delete(ablock);
    ablock = 0;
  } else {
    _iamf_audio_block_trimming_info_update(ablock,
                                           self->base.audio_frame_obus[0]);
    _reconstructor_and_audio_block_update_delay(
        &self->base, ablock, iamf_core_decoder_get_delay(self->core_decoder));
  }

  return ablock;
}

static scene_based_reconstructor_t* _scene_based_reconstructor_new(
    const iamf_audio_element_obu_t* aeo, const iamf_codec_config_obu_t* cco) {
  const scene_based_audio_element_obu_t* sbo =
      (const scene_based_audio_element_obu_t*)aeo;
  scene_based_reconstructor_t* sbr =
      def_mallocz(scene_based_reconstructor_t, 1);
  if (!sbr) {
    def_err_msg_enomem("Scene Based Reconstructor", "Reconstructor");
    return 0;
  }

  if (_reconstructor_init(&sbr->base, aeo, cco) != IAMF_OK) {
    _scene_based_reconstructor_delete(sbr);
    return 0;
  }

  sbr->base.destroy =
      (fun_reconstructor_destroy_t)_scene_based_reconstructor_delete;
  sbr->base.process_frame = (fun_reconstructor_process_frame_t)
      _scene_based_reconstructor_process_frame;

  if (sbo->ambisonics_mode == ck_ambisonics_mode_mono ||
      sbo->ambisonics_mode == ck_ambisonics_mode_projection)
    sbr->core_decoder = _core_decoder_open(
        _get_stream_mode_ambisonics(sbo->ambisonics_mode),
        sbo->output_channel_count, sbo->substream_count,
        sbo->coupled_substream_count, sbo->channel_mapping,
        sbo->mapping_size * (sbo->ambisonics_mode == ck_ambisonics_mode_mono
                                 ? 1
                                 : sizeof(short)),
        cco);

  if (!sbr->core_decoder) {
    _scene_based_reconstructor_delete(sbr);
    return 0;
  }

  info("make scene based reconstructor for element %u", aeo->audio_element_id);
  return sbr;
}

static void _object_based_reconstructor_delete(
    object_based_reconstructor_t* self) {
  if (!self) return;
  _reconstructor_uninit(&self->base);
  if (self->core_decoder) iamf_core_decoder_close(self->core_decoder);
  def_free(self);
}

static iamf_audio_block_t* _object_based_reconstructor_process_frame(
    object_based_reconstructor_t* self, int flush) {
  iamf_audio_block_t* ablock = iamf_audio_block_new(
      self->base.audio_element_obu->audio_element_id,
      self->base.num_samples_per_frame, self->base.num_audio_channels);
  debug("samples %u, channels %u, block %p", self->base.num_samples_per_frame,
        self->base.num_audio_channels, ablock);
  int ret =
      _core_decoder_decode(self->core_decoder, self->base.audio_frame_obus,
                           self->base.num_audio_streams, ablock, flush);
  if (ret < 0) {
    error("Failed to decode audio data in object-based reconstructor %u.",
          self->base.audio_element_obu->audio_element_id);
    iamf_audio_block_delete(ablock);
    ablock = 0;
  } else {
    _iamf_audio_block_trimming_info_update(ablock,
                                           self->base.audio_frame_obus[0]);
    _reconstructor_and_audio_block_update_delay(
        &self->base, ablock, iamf_core_decoder_get_delay(self->core_decoder));
  }

  return ablock;
}

static object_based_reconstructor_t* _object_based_reconstructor_new(
    const iamf_audio_element_obu_t* aeo, const iamf_codec_config_obu_t* cco) {
  const object_based_audio_element_obu_t* obo =
      (const object_based_audio_element_obu_t*)aeo;
  object_based_reconstructor_t* obr =
      def_mallocz(object_based_reconstructor_t, 1);
  if (!obr) {
    def_err_msg_enomem("Object Based Reconstructor", "Reconstructor");
    return 0;
  }

  if (_reconstructor_init(&obr->base, aeo, cco) != IAMF_OK) {
    _object_based_reconstructor_delete(obr);
    return 0;
  }

  obr->base.destroy =
      (fun_reconstructor_destroy_t)_object_based_reconstructor_delete;
  obr->base.process_frame = (fun_reconstructor_process_frame_t)
      _object_based_reconstructor_process_frame;

  obr->base.num_audio_channels = obo->num_objects;

  obr->core_decoder = _core_decoder_open(
      ck_stream_mode_ambisonics_none, obr->base.num_audio_channels,
      obr->base.num_audio_streams,
      obr->base.num_audio_channels != obr->base.num_audio_streams, 0, 0, cco);

  if (!obr->core_decoder) {
    _object_based_reconstructor_delete(obr);
    return 0;
  }

  info("make object based reconstructor for element %u with %u objects",
       aeo->audio_element_id, obo->num_objects);
  return obr;
}

static void _reconstructor_delete(base_reconstructor_t* self) {
  if (!self) return;
  if (self->destroy) self->destroy(self);
}

static void iamf_element_reconstructor_priv_delete_all_audio_frame_obus(
    iamf_element_reconstructor_t* self) {
  if (!self || !hash_map_size(self->reconstructor_map)) return;

  hash_map_iterator_t* iter = hash_map_iterator_new(self->reconstructor_map);
  if (iter) {
    do {
      _reconstructor_delete_all_audio_frame_obus(def_value_wrap_type_ptr(
          base_reconstructor_t, hash_map_iterator_get_value(iter)));
    } while (!hash_map_iterator_next(iter));
    hash_map_iterator_delete(iter);
  }
}

// =============================================================================
// Public Interface Implementation
// =============================================================================

iamf_element_reconstructor_t* iamf_element_reconstructor_create() {
  iamf_element_reconstructor_t* reconstructor =
      def_mallocz(iamf_element_reconstructor_t, 1);
  if (!reconstructor) {
    def_err_msg_enomem("instance", "Reconstructor");
    return 0;
  }

  reconstructor->reconstructor_map =
      hash_map_new(def_hash_map_capacity_elements);
  if (!reconstructor->reconstructor_map) {
    def_err_msg_enomem("reconstructors map", "Reconstructor");
    iamf_element_reconstructor_destroy(reconstructor);
    return 0;
  }

  reconstructor->audio_frame_obus =
      hash_map_new(def_hash_map_capacity_audio_frames);
  if (!reconstructor->audio_frame_obus) {
    def_err_msg_enomem("audio frames map", "Reconstructor");
    iamf_element_reconstructor_destroy(reconstructor);
    return 0;
  }

  return reconstructor;
}

void iamf_element_reconstructor_destroy(
    iamf_element_reconstructor_t* reconstructor) {
  if (!reconstructor) return;

  if (reconstructor->reconstructor_map)
    hash_map_delete(reconstructor->reconstructor_map,
                    def_default_free_ptr(_reconstructor_delete));

  if (reconstructor->audio_frame_obus)
    hash_map_delete(reconstructor->audio_frame_obus, 0);

  def_free(reconstructor);
}

int iamf_element_reconstructor_add_element(
    iamf_element_reconstructor_t* reconstructor,
    const iamf_audio_element_obu_t* aeo, const iamf_codec_config_obu_t* cco) {
  base_reconstructor_t* br = 0;
  if (!reconstructor || !aeo || !cco) return IAMF_ERR_BAD_ARG;

  switch (aeo->audio_element_type) {
    case ck_audio_element_type_channel_based: {
      channel_based_reconstructor_t* cbr =
          _channel_based_reconstructor_new(aeo, cco);
      if (!cbr) {
        error("Failed to create channel-based reconstructor.");
        return IAMF_ERR_ALLOC_FAIL;
      }
      br = (base_reconstructor_t*)cbr;
    } break;
    case ck_audio_element_type_scene_based: {
      scene_based_reconstructor_t* sbr =
          _scene_based_reconstructor_new(aeo, cco);
      if (!sbr) {
        error("Failed to create scene-based reconstructor.");
        return IAMF_ERR_ALLOC_FAIL;
      }
      br = (base_reconstructor_t*)sbr;
    } break;
    case ck_audio_element_type_object_based: {
      object_based_reconstructor_t* obr =
          _object_based_reconstructor_new(aeo, cco);
      if (!obr) {
        error("Failed to create object-based reconstructor.");
        return IAMF_ERR_ALLOC_FAIL;
      }
      br = (base_reconstructor_t*)obr;
      break;
    }
    default: {
      warning("Unsupported audio element type %d.", aeo->audio_element_type);
      return IAMF_ERR_UNIMPLEMENTED;
    }
  }

  if (br) {
    int n = array_size(aeo->audio_substream_ids);
    trace("add element %u, substream ids %d to reconstructor.",
          aeo->audio_element_id, n);
    hash_map_put(reconstructor->reconstructor_map, aeo->audio_element_id,
                 def_value_wrap_instance_ptr(br));
    for (int i = 0; i < n; ++i) {
      hash_map_put(reconstructor->audio_frame_obus,
                   def_value_wrap_u32(array_at(aeo->audio_substream_ids, i)),
                   def_value_wrap_instance_u32(aeo->audio_element_id));
    }
  }

  return IAMF_OK;  // Success
}

int iamf_element_reconstructor_add_audio_frame_obu(
    iamf_element_reconstructor_t* self, iamf_audio_frame_obu_t* obu) {
  value_wrap_t* val = 0;
  uint32_t eid = 0;
  base_reconstructor_t* recon = 0;
  iamf_audio_frame_obu_t** afo = 0;
  int ret = IAMF_OK;

  if (!self || !obu) {
    error("Invalid arguments: self or obu is NULL.");
    return IAMF_ERR_BAD_ARG;
  }

  val = hash_map_get(self->audio_frame_obus, obu->audio_substream_id);
  if (!val) {
    debug("Audio element ID not found for substream ID %u.",
          obu->audio_substream_id);
    iamf_audio_frame_obu_free(obu);
    return IAMF_ERR_BAD_ARG;
  }
  eid = def_value_wrap_u32(val);

  val = hash_map_get(self->reconstructor_map, eid);
  if (!val) {
    warning("Reconstructor not found for audio element ID %u.", eid);
    iamf_audio_frame_obu_free(obu);
    return IAMF_ERR_INTERNAL;
  }
  recon = def_value_wrap_ptr(val);

  val = hash_map_get(recon->audio_frames_map, obu->audio_substream_id);
  if (!val) {
    warning("Audio frame node not found for substream ID %u in self.",
            obu->audio_substream_id);
    iamf_audio_frame_obu_free(obu);
    return IAMF_ERR_INTERNAL;
  }
  afo = def_value_wrap_ptr(val);

  if (*afo) {
    warning("Substream ID %u already has an audio frame OBU.",
            obu->audio_substream_id);
    iamf_element_reconstructor_priv_delete_all_audio_frame_obus(self);
    ret = IAMF_ERR_INVALID_PACKET;
  }

  *afo = obu;
  recon->num_audio_frames++;

  if (recon->num_audio_frames != recon->num_audio_streams) return ret;
  debug("element %u all audio frames %d are ready.", eid,
        recon->num_audio_frames);

  return 1;
}

iamf_audio_block_t* iamf_element_reconstructor_process(
    iamf_element_reconstructor_t* self) {
  iamf_audio_block_t* ablock = 0;
  if (!self || !hash_map_size(self->reconstructor_map)) {
    debug(
        "iamf_element_reconstructor_process: self is NULL or reconstructor_map "
        "is empty");
    return 0;
  }

  debug("iamf_element_reconstructor_process: reconstructor_map size = %d",
        hash_map_size(self->reconstructor_map));

  hash_map_iterator_t* iter = hash_map_iterator_new(self->reconstructor_map);
  if (iter) {
    do {
      base_reconstructor_t* br = def_value_wrap_type_ptr(
          base_reconstructor_t, hash_map_iterator_get_value(iter));
      if (br) {
        if (br->num_audio_streams == br->num_audio_frames) {
          if (br->process_frame) ablock = br->process_frame(br, 0);
          _reconstructor_delete_all_audio_frame_obus(br);
          break;
        } else {
          debug(
              "audio element %u: Waiting for more audio "
              "frames (%d/%d)",
              hash_map_iterator_get_key(iter), br->num_audio_frames,
              br->num_audio_streams);
        }
      }
    } while (!hash_map_iterator_next(iter));
    hash_map_iterator_delete(iter);
  }
  return ablock;
}

iamf_audio_block_t* iamf_element_reconstructor_flush(
    iamf_element_reconstructor_t* self, uint32_t id) {
  iamf_audio_block_t* ablock = 0;
  if (!self) return 0;

  if (id == def_i32_id_none) {
    hash_map_iterator_t* iter = hash_map_iterator_new(self->reconstructor_map);
    if (iter) {
      do {
        base_reconstructor_t* br = def_value_wrap_type_ptr(
            base_reconstructor_t, hash_map_iterator_get_value(iter));
        if (br->delay) {
          ablock = br->process_frame(br, 1);
          ablock->second_padding = ablock->num_samples_per_channel - br->delay;
          br->delay = 0;
          break;
        }
      } while (!hash_map_iterator_next(iter));
      hash_map_iterator_delete(iter);
    }
  } else {
    base_reconstructor_t* br =
        def_value_wrap_optional_ptr(hash_map_get(self->reconstructor_map, id));
    if (br && br->delay > 0) {
      ablock = br->process_frame(br, 1);
      ablock->second_padding = ablock->num_samples_per_channel - br->delay;
      br->delay = 0;
    }
  }

  return ablock;
}

int iamf_element_reconstructor_update_demixing_mode(
    iamf_element_reconstructor_t* reconstructor, int audio_element_id,
    int demixing_mode) {
  if (!reconstructor) return IAMF_ERR_BAD_ARG;
  base_reconstructor_t* br = def_value_wrap_optional_ptr(
      hash_map_get(reconstructor->reconstructor_map, audio_element_id));

  if (!br) return IAMF_ERR_BAD_ARG;
  if (br->type == ck_audio_element_type_channel_based) {
    channel_based_reconstructor_t* cbr = (channel_based_reconstructor_t*)br;
    if (cbr->demixer)
      return demixer_set_demixing_info(cbr->demixer, demixing_mode,
                                       def_dmx_weight_index_none);
  }
  return IAMF_ERR_BAD_ARG;
}

int iamf_element_reconstructor_update_recon_gain(
    iamf_element_reconstructor_t* reconstructor, int audio_element_id,
    const recon_gain_t* recon_gain) {
  if (!reconstructor || !recon_gain) return IAMF_ERR_BAD_ARG;
  base_reconstructor_t* br = def_value_wrap_optional_ptr(
      hash_map_get(reconstructor->reconstructor_map, audio_element_id));
  if (!br) return IAMF_ERR_BAD_ARG;
  if (br->type == ck_audio_element_type_channel_based) {
    channel_based_reconstructor_t* cbr = (channel_based_reconstructor_t*)br;
    if (cbr->target_layout_index == recon_gain->layer) {
      int channels = array_size(recon_gain->recon_gain_linear);
      iamf_channel_t order[ck_iamf_channel_recon_count] = {0};
      float recon_gain_linear[ck_iamf_channel_recon_count] = {0};
      for (int i = 0; i < channels; ++i)
        recon_gain_linear[i] =
            def_value_wrap_f32(array_at(recon_gain->recon_gain_linear, i));
      _get_recon_channels_order(
          cbr->group_infos[cbr->target_layout_index].layout,
          recon_gain->recon_gain_flags, order, ck_iamf_channel_recon_count);
      return demixer_set_recon_gain(cbr->demixer, channels, order,
                                    recon_gain_linear,
                                    recon_gain->recon_gain_flags);
    }
  }
  return IAMF_ERR_BAD_ARG;
}

int iamf_element_reconstructor_set_channel_based_target_layout(
    iamf_element_reconstructor_t* reconstructor, int audio_element_id,
    iamf_loudspeaker_layout_t target_layout) {
  base_reconstructor_t* br = 0;
  if (!reconstructor) {
    error("Invalid reconstructor instance.");
    return IAMF_ERR_BAD_ARG;
  }

  if (!iamf_audio_layer_base_layout_check(target_layout)) {
    debug("Invalid target layout (%d) specified.", target_layout);
    return IAMF_ERR_BAD_ARG;
  }

  br = def_value_wrap_optional_ptr(
      hash_map_get(reconstructor->reconstructor_map, audio_element_id));
  if (!br) {
    error("Audio element with ID %d not found.", audio_element_id);
    return IAMF_ERR_INTERNAL;
  }

  if (br->type == ck_audio_element_type_channel_based) {
    channel_based_reconstructor_t* cbr = (channel_based_reconstructor_t*)br;
    int i = 0;
    for (; i < cbr->num_groups; ++i) {
      if (cbr->group_infos[i].layout == target_layout) {
        cbr->target_layout_index = i;
        break;
      }
    }

    if (i < cbr->num_groups)
      return _set_channel_based_target_layout(cbr, target_layout);
  }

  return IAMF_ERR_BAD_ARG;
}

int iamf_element_reconstructor_get_channel_based_reconstructed_layout_index(
    iamf_element_reconstructor_t* self, int audio_element_id) {
  if (!self) return IAMF_ERR_BAD_ARG;
  base_reconstructor_t* br = def_value_wrap_optional_ptr(
      hash_map_get(self->reconstructor_map, audio_element_id));
  if (!br) return IAMF_ERR_BAD_ARG;
  if (br->type == ck_audio_element_type_channel_based) {
    channel_based_reconstructor_t* cbr = (channel_based_reconstructor_t*)br;
    // Add bounds checking for target_layout_index
    if (cbr->target_layout_index >= 0 &&
        cbr->target_layout_index < cbr->num_groups) {
      return cbr->target_layout_index;
    }
  }

  return IAMF_ERR_BAD_ARG;
}

int iamf_element_reconstructor_activate_element(
    iamf_element_reconstructor_t* reconstructor, int audio_element_id) {
  return IAMF_OK;  // Success
}
