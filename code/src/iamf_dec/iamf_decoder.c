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
 * @file IAMF_decoder.c
 * @brief IAMF decoder.
 * @version 2.0.0
 * @date Created 03/03/2023
 **/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <inttypes.h>
#include <math.h>
#include <stdbool.h>

#include "IAMF_decoder.h"
#include "clog.h"
#include "cvector.h"
#include "demixer.h"
#include "iamf_decoder_private.h"
#include "iamf_layout.h"
#include "iamf_obu_all.h"
#include "iamf_post_processor.h"
#include "iamf_private_definitions.h"
#include "iamf_renderer.h"
#include "iamf_string.h"
#include "iamf_utils.h"

#undef def_log_tag
#define def_log_tag "IAMF_DEC"

#define SR 0
#if SR
extern void iamf_rec_stream_log(int eid, int chs, float *in, int size);
extern void iamf_mix_stream_log(int chs, float *out, int size);
extern void iamf_stream_log_free();
#endif

// Helper function to check if audio element has binaural layout
static int _audio_element_binaural_layout_check(obu_sub_mix_t *sub_mix,
                                                iamf_database_t *database) {
  int n = array_size(sub_mix->audio_element_configs);

  for (int i = 0; i < n; i++) {
    obu_audio_element_config_t *aec = def_value_wrap_optional_ptr(
        array_at(sub_mix->audio_element_configs, i));
    iamf_audio_element_obu_t *e =
        iamf_database_get_audio_element_obu(database, aec->element_id);

    if (e && e->audio_element_type == ck_audio_element_type_channel_based) {
      channel_based_audio_element_obu_t *cae =
          def_channel_based_audio_element_obu_ptr(e);
      if (array_size(cae->channel_audio_layer_configs) == 1) {
        obu_channel_layer_config_t *clc = def_value_wrap_optional_ptr(
            array_at(cae->channel_audio_layer_configs, 0));
        if (clc->loudspeaker_layout == ck_iamf_loudspeaker_layout_binaural)
          return def_true;
      }
    }
  }
  return def_false;
}

static int _matching_loudess_layout_check(obu_sub_mix_t *sub,
                                          iamf_layout_t target_layout,
                                          void *data) {
  int n = array_size(sub->loudness_layouts);

  for (int i = 0; i < n; ++i) {
    iamf_layout_t *check_layout =
        def_value_wrap_optional_ptr(array_at(sub->loudness_layouts, i));
    if (!check_layout) continue;
    if (iamf_layout_is_equal(target_layout, *check_layout)) return def_true;
  }

  return def_false;
}

static int _stereo_layout_check(obu_sub_mix_t *sub, iamf_layout_t unused,
                                void *data) {
  return _matching_loudess_layout_check(
             sub, def_sound_system_layout_instance(SOUND_SYSTEM_A), data) &&
         (array_size(sub->loudness_layouts) == 1);
}

static int _binaural_layout_check(obu_sub_mix_t *sub,
                                  iamf_layout_t target_layout, void *data) {
  iamf_database_t *database = (iamf_database_t *)data;
  return target_layout.type == ck_iamf_layout_type_binaural
             ? _audio_element_binaural_layout_check(sub, database)
             : def_false;
}

static sample_format_t _get_sample_format(uint32_t bit_depth, int interleaved) {
  if (bit_depth == 16) return ck_sample_format_i16_interleaved;
  if (bit_depth == 24) return ck_sample_format_i24_interleaved;
  if (bit_depth == 32) return ck_sample_format_i32_interleaved;
  return ck_sample_format_i16_interleaved;
}

static float iamf_mix_presentation_get_best_loudness(
    iamf_mix_presentation_obu_t *obj, iamf_layout_t *layout) {
  obu_sub_mix_t *sub;
  float loudness_lkfs = def_default_loudness_lkfs;
  iamf_layout_t highest_layout =
      def_sound_system_layout_instance(SOUND_SYSTEM_NONE);

  int num_sub_mixes = array_size(obj->sub_mixes);

  for (int sub_idx = 0; sub_idx < num_sub_mixes; ++sub_idx) {
    sub = def_value_wrap_optional_ptr(array_at(obj->sub_mixes, sub_idx));
    if (!sub) continue;

    int n = array_size(sub->loudness_layouts);
    if (n <= 0) continue;

    for (int i = 0; i < n; ++i) {
      iamf_layout_t *loudness_layout =
          def_value_wrap_optional_ptr(array_at(sub->loudness_layouts, i));
      if (!loudness_layout) continue;

      if (iamf_layout_is_equal(*layout, *loudness_layout)) {
        obu_loudness_info_t *li =
            def_value_wrap_optional_ptr(array_at(sub->loudness, i));
        if (li) {
          loudness_lkfs =
              iamf_gain_q78_to_db(def_lsb_16bits(li->integrated_loudness));
          info(
              "selected loudness %f db from exact match layout in sub_mix[%d] "
              "<- 0x%x",
              loudness_lkfs, sub_idx, def_lsb_16bits(li->integrated_loudness));
          return loudness_lkfs;
        }
      }

      if (iamf_layout_is_equal(highest_layout, def_sound_system_layout_instance(
                                                   SOUND_SYSTEM_NONE)) ||
          iamf_layout_higher_check(*loudness_layout, highest_layout, 1)) {
        obu_loudness_info_t *li =
            def_value_wrap_optional_ptr(array_at(sub->loudness, i));
        if (li) {
          highest_layout = *loudness_layout;
          loudness_lkfs =
              iamf_gain_q78_to_db(def_lsb_16bits(li->integrated_loudness));
          debug(
              "selected loudness %f db from highest layout in sub_mix[%d] <- "
              "0x%x",
              loudness_lkfs, sub_idx, def_lsb_16bits(li->integrated_loudness));
        }
      }
    }
  }

  debug("selected loudness %f db from highest layout %s", loudness_lkfs,
        iamf_layout_string(highest_layout));

  return loudness_lkfs;
}

static int iamf_decoder_priv_decode(iamf_decoder_t *self, const uint8_t *data,
                                    int32_t size, uint32_t *rsize, void *pcm) {
  iamf_decoder_context_t *ctx = &self->ctx;
  // iamf_database_t *database = &ctx->database;
  iamf_presentation_t *pst = self->presentation;
  iamf_audio_block_t *audio_block = 0;
  iamf_audio_block_t *out = 0;
  fraction_t frame_duration = self->ctx.frame_duration;
  int ret = 0;

  if (data && size) {
    *rsize = iamf_obu_parser_parse(self->parser, data, size);

    iamf_parser_state_t state = iamf_obu_parser_get_state(self->parser);
    if (state == ck_iamf_parser_state_run) return 0;
    if (state == ck_iamf_parser_state_ahead) {
      ctx->status = ck_iamf_decoder_status_reconfigure;
      return IAMF_ERR_INVALID_STATE;
    }

    ctx->status = ck_iamf_decoder_status_process;
    audio_block = iamf_presentation_process(pst);
  } else if (ctx->cache.decoder) {
    ctx->status = ck_iamf_decoder_status_process;
    while (1) {
      audio_block = iamf_element_reconstructor_flush(self->reconstructor,
                                                     def_i32_id_none);
      if (audio_block) {
        ret = 1;
        frame_duration.numerator = audio_block->num_samples_per_channel;
        iamf_presentation_add_audio_block(pst, audio_block);
#if SR
        iamf_rec_stream_log(audio_block->id, audio_block->num_channels,
                            audio_block->data,
                            audio_block->capacity_per_channel);
#endif
      } else
        break;
    }

    if (ret) {
      ctx->cache.decoder = 0;
      audio_block = iamf_presentation_process(pst);
    }
  }

  if (audio_block) {
    uint32_t _2nd_skip = audio_block->second_skip;
    frame_duration.numerator -= _2nd_skip;
    frame_duration.numerator -= audio_block->second_padding;
    iamf_database_time_elapse(&self->ctx.database, frame_duration);

    if (_2nd_skip > 0) ctx->cache.decoder = _2nd_skip;

    audio_block->padding += audio_block->second_padding;
    audio_block->second_padding = 0;

    if (audio_block->padding > 0) {
      ctx->cache.padding += audio_block->padding;
      audio_block->padding = 0;
      if (ctx->cache.decoder && ctx->cache.padding > ctx->cache.decoder) {
        audio_block->padding = ctx->cache.padding - ctx->cache.decoder;
        ctx->cache.padding = ctx->cache.decoder = 0;
      } else if (!ctx->cache.decoder) {
        audio_block->padding = ctx->cache.padding;
        ctx->cache.padding = 0;
      }
      debug("cache info: decoder %d, padding %d", ctx->cache.decoder,
            ctx->cache.padding);
    }
    iamf_audio_block_trim(audio_block);

    if (!audio_block->num_samples_per_channel) {
      iamf_audio_block_delete(audio_block);
      ctx->status = ck_iamf_decoder_status_parse_2;
      return 0;
    }

    iamf_post_processor_process(self->post_processor, audio_block, &out);
    iamf_audio_block_delete(audio_block);
  }

  if (!data || !size) {
    iamf_audio_block_t *ablocks[2] = {0};
    ablocks[0] = out;
    iamf_post_processor_process(self->post_processor, 0, &ablocks[1]);

    if (ablocks[0] && ablocks[1]) {
      out = iamf_audio_block_samples_concat(ablocks, 2);
      iamf_audio_block_delete(ablocks[0]);
      iamf_audio_block_delete(ablocks[1]);
    } else if (ablocks[0])
      out = ablocks[0];
    else if (ablocks[1])
      out = ablocks[1];
  }

  if (out) {
#if SR
    iamf_mix_stream_log(out->num_channels, out->data,
                        out->capacity_per_channel);
#endif
    sample_format_t format = _get_sample_format(ctx->bit_depth, 1);
    iamf_audio_block_copy_data(out, pcm, format);
    ret = out->num_samples_per_channel;
    iamf_audio_block_delete(out);
  }

  ctx->status = ck_iamf_decoder_status_parse_2;

  return ret;
}

static int iamf_decoder_priv_parse_descriptor_obus(iamf_decoder_t *self,
                                                   const uint8_t *data,
                                                   uint32_t size,
                                                   uint32_t *rsize) {
  int ret = IAMF_OK;
  uint32_t n = 0;
  iamf_decoder_context_t *ctx = &self->ctx;

  n = iamf_obu_parser_parse(self->parser, data, size);

  if (iamf_database_descriptors_get_state(&ctx->database) ==
      ck_descriptors_state_completed)
    self->ctx.status = ck_iamf_decoder_status_configure;
  else
    ret = IAMF_ERR_BUFFER_TOO_SMALL;

  if (rsize) *rsize = n;
  return ret;
}

static iamf_mix_presentation_obu_t *iamf_decoder_priv_get_best_mix_presentation(
    iamf_decoder_t *self) {
  iamf_decoder_context_t *ctx = &self->ctx;
  iamf_database_t *database = &ctx->database;
  iamf_layout_t target_layout = ctx->layout;
  iamf_mix_presentation_obu_t *obu = 0;

  if (ctx->layout.type == ck_iamf_layout_type_binaural) {
    obu = iamf_database_find_mix_presentation_obu(
        database, _binaural_layout_check, target_layout);
    if (obu) return obu;

    obu = iamf_database_find_mix_presentation_obu(
        database, _matching_loudess_layout_check, target_layout);
    if (obu) return obu;
  } else if (ctx->layout.type ==
             ck_iamf_layout_type_loudspeakers_ss_convention) {
    if (ctx->layout.sound_system == SOUND_SYSTEM_A) {
      obu = iamf_database_find_mix_presentation_obu(
          database, _stereo_layout_check, target_layout);
      if (obu) return obu;
    } else {
      obu = iamf_database_find_mix_presentation_obu(
          database, _matching_loudess_layout_check, target_layout);
      if (obu) return obu;
    }
  }

  obu = iamf_database_find_mix_presentation_obu_with_highest_layout(
      database, target_layout);
  return obu ? obu : iamf_database_get_mix_presentation_obu_default(database);
}

static int iamf_decoder_priv_enable_mix_presentation(
    iamf_decoder_t *self, iamf_mix_presentation_obu_t *mpo) {
  iamf_decoder_context_t *ctx = &self->ctx;
  iamf_database_t *database = &ctx->database;
  iamf_presentation_t *old = self->presentation;
  iamf_presentation_t *pst;

  pst = iamf_presentation_create(mpo->mix_presentation_id, database,
                                 self->reconstructor, ctx->layout);
  if (!pst) return IAMF_ERR_ALLOC_FAIL;

  debug("enable mix presentation id %u, %p", mpo->mix_presentation_id, mpo);

  iamf_database_disable_parameter_blocks(database);
  iamf_database_enable_mix_presentation_parameter_blocks(
      database, mpo->mix_presentation_id);
  iamf_database_disabled_parameter_blocks_clear(database);
  self->presentation = pst;

  if (old) iamf_presentation_destroy(old);

  return IAMF_OK;
}

int iamf_decoder_priv_update_frame_info(iamf_decoder_t *self) {
  iamf_decoder_context_t *ctx = &self->ctx;
  iamf_database_t *database = &ctx->database;
  iamf_mix_presentation_obu_t *mix_presentation_obu = 0;
  obu_sub_mix_t *sub_mix = 0;
  obu_audio_element_config_t *audio_element_config = 0;
  audio_element_t *audio_element = 0;
  codec_config_t *codec_config = 0;
  uint32_t id = iamf_presentation_get_id(self->presentation);

  int ret = IAMF_ERR_INTERNAL;

  uint32_t sample_rate = 0;
  uint32_t num_samples_per_frame = 0;

  mix_presentation_obu = iamf_database_get_mix_presentation_obu(database, id);
  int num_sub_mixes = array_size(mix_presentation_obu->sub_mixes);

  for (int i = 0; i < num_sub_mixes; ++i) {
    sub_mix = def_value_wrap_optional_ptr(
        array_at(mix_presentation_obu->sub_mixes, i));

    int n = array_size(sub_mix->audio_element_configs);
    for (int j = 0; j < n; ++j) {
      audio_element_config = def_value_wrap_optional_ptr(
          array_at(sub_mix->audio_element_configs, j));
      audio_element = iamf_database_get_audio_element(
          database, audio_element_config->element_id);
      codec_config = audio_element->codec_config;
      if (!sample_rate) {
        sample_rate = codec_config->codec_param.sample_rate;
        num_samples_per_frame = codec_config->codec_param.frame_size;
        ret = IAMF_OK;
      } else if (sample_rate != codec_config->codec_param.sample_rate ||
                 num_samples_per_frame !=
                     codec_config->codec_param.frame_size) {
        warning("Different sample rate or frame size found.");
        ret = IAMF_ERR_UNIMPLEMENTED;
      }
    }
  }

  if (ret == IAMF_OK) {
    ctx->frame_duration.numerator = num_samples_per_frame;
    ctx->frame_duration.denominator = sample_rate;
    info("Frame duration updated for presentation %u: %u/%u.", id,
         ctx->frame_duration.numerator, ctx->frame_duration.denominator);
  }

  return ret;
}

int iamf_decoder_priv_configure(iamf_decoder_t *self, const uint8_t *data,
                                uint32_t size, uint32_t *rsize) {
  int ret = IAMF_OK;
  iamf_decoder_context_t *ctx;
  iamf_database_t *database;

  trace("self %p, data %p, size %d", self, data, size);

  if (!self) return IAMF_ERR_BAD_ARG;

  ctx = &self->ctx;
  database = &ctx->database;

  if (self->post_processor &&
      (ctx->configure_flags & def_iamf_decoder_config_output_layout)) {
    info("Initialize post processor.");
    iamf_post_processor_init(self->post_processor, self->ctx.sampling_rate,
                             iamf_layout_channels_count(&self->ctx.layout));
    if (ctx->enable_limiter)
      iamf_post_processor_enable_limiter(self->post_processor,
                                         self->ctx.limiter_threshold_db);
    ctx->configure_flags &= ~def_iamf_decoder_config_output_layout;
  }

  if (data && size > 0) {
    if (ctx->status == ck_iamf_decoder_status_init)
      ctx->status = ck_iamf_decoder_status_parse_1;
    else if (ctx->status == ck_iamf_decoder_status_parse_2)
      ctx->status = ck_iamf_decoder_status_reconfigure;

    if (ctx->status == ck_iamf_decoder_status_reconfigure) {
      info("Reconfigure decoder.");
      iamf_database_reset_descriptors(database);
      ctx->status = ck_iamf_decoder_status_parse_1;
    }

    ret = iamf_decoder_priv_parse_descriptor_obus(self, data, size, rsize);
  } else if (ctx->configure_flags) {
    info("configure flags 0x%x", ctx->configure_flags);
    if (ctx->status < ck_iamf_decoder_status_configure) {
      error("Decoder need configure with descriptor obus. status %d",
            ctx->status);
      return IAMF_ERR_BAD_ARG;
    }

    if (ctx->configure_flags & def_iamf_decoder_config_mix_presentation) {
      if (!iamf_database_get_mix_presentation_obu(database,
                                                  ctx->mix_presentation_id)) {
        warning("Invalid mix presentation id %" PRId64 ".",
                ctx->mix_presentation_id);
        return IAMF_ERR_INTERNAL;
      }
    }

  } else {
    error("Decoder need configure with descriptor obus.");
    return IAMF_ERR_BAD_ARG;
  }

  ctx->configure_flags = 0;

  if (ret == IAMF_OK) {
    iamf_mix_presentation_obu_t *mpo = 0;

    if (ctx->mix_presentation_id != def_i64_id_none)
      mpo = iamf_database_get_mix_presentation_obu(database,
                                                   ctx->mix_presentation_id);
    else {
      mpo = iamf_decoder_priv_get_best_mix_presentation(self);
      ctx->mix_presentation_id = mpo->mix_presentation_id;
    }

    if (mpo) {
      info("get mix presentation id %u",
           mpo ? mpo->mix_presentation_id : def_i32_id_none);
      ret = iamf_decoder_priv_enable_mix_presentation(self, mpo);
      if (ret != IAMF_OK) {
        error("Fail to enable mix presentation %u.", mpo->mix_presentation_id);
      } else {
        if (ctx->layout.type == ck_iamf_layout_type_binaural) {
          iamf_presentation_set_head_rotation(self->presentation,
                                              &ctx->head_rotation);
          iamf_presentation_enable_head_tracking(self->presentation,
                                                 ctx->head_tracking_enabled);
        }

        // Set element gain offset to presentation.
        if (ctx->element_gains && hash_map_size(ctx->element_gains) > 0) {
          hash_map_iterator_t *iter = hash_map_iterator_new(ctx->element_gains);
          if (iter) {
            do {
              uint32_t element_id = hash_map_iterator_get_key(iter);
              value_wrap_t *value = hash_map_iterator_get_value(iter);
              if (value) {
                float gain_offset = def_value_wrap_f32(value);
                int ret = iamf_presentation_set_element_gain_offset(
                    self->presentation, element_id, gain_offset);
                if (ret == IAMF_OK) {
                  info(
                      "Applied gain offset %f to element %u in presentation %u",
                      gain_offset, element_id, mpo->mix_presentation_id);
                } else {
                  warning(
                      "Failed to apply gain offset %f to element %u in "
                      "presentation %u: %d",
                      gain_offset, element_id, mpo->mix_presentation_id, ret);
                }
              }
            } while (!hash_map_iterator_next(iter));
            hash_map_iterator_delete(iter);
          }
          // Clear element gains after applying to presentation
          hash_map_delete(ctx->element_gains, 0);
          ctx->element_gains = 0;
          info("Cleared element gains after applying to presentation %u",
               mpo->mix_presentation_id);
        }

        // Check if the sampling rate of the stream in presentation matches the
        // configured sampling rate
        int in_sampling_rate =
            iamf_presentation_get_sampling_rate(self->presentation);
        if (in_sampling_rate == ctx->sampling_rate) {
          iamf_post_processor_disable_resampler(self->post_processor);
        } else {
          ret = iamf_post_processor_enable_resampler(self->post_processor,
                                                     in_sampling_rate);
          if (ret != IAMF_OK) {
            error("Fail to enable resampler for presentation %u.",
                  mpo->mix_presentation_id);
            return ret;
          }
        }

        ret = iamf_decoder_priv_update_frame_info(self);
        if (ret == IAMF_OK) {
          self->ctx.status = ck_iamf_decoder_status_parse_2;
          self->ctx.loudness_lkfs =
              iamf_mix_presentation_get_best_loudness(mpo, &self->ctx.layout);
          if (self->ctx.normalized_loudness_lkfs != def_default_loudness_lkfs) {
            iamf_presentation_set_loudness_gain(
                self->presentation,
                f32_db_to_linear(self->ctx.normalized_loudness_lkfs -
                                 self->ctx.loudness_lkfs));
          }
        }
      }
    } else {
      ret = IAMF_ERR_INTERNAL;
      if (ctx->mix_presentation_id != def_i64_id_none)
        warning("Fail to find the mix presentation %u obu.",
                ctx->mix_presentation_id);
      else
        warning("Fail to find the valid mix presentation obu, try again.");
    }
  }

  return ret;
}

static iamf_parser_state_t iamf_decoder_priv_process_descriptor_obus(
    iamf_decoder_t *self, iamf_obu_t *obu) {
  iamf_database_t *database = &self->ctx.database;

  if (obu)
    debug("Processing OBU type: %s", iamf_obu_type_string(obu->obu_type));

  if ((!obu || !iamf_obu_is_descriptor(obu)) &&
      iamf_database_descriptors_complete(database)) {
    if (obu) iamf_obu_free(obu);
    info("All descriptor OBUs are received.");
    return ck_iamf_parser_state_ahead;
  }

  iamf_database_add_obu(database, obu);
  return ck_iamf_parser_state_run;
}

static iamf_parser_state_t iamf_decoder_priv_process_data_obus(
    iamf_decoder_t *self, iamf_obu_t *obu) {
  iamf_decoder_context_t *ctx = &self->ctx;
  iamf_presentation_t *presentation = self->presentation;
  iamf_database_t *database = &self->ctx.database;
  iamf_parser_state_t state = ck_iamf_parser_state_run;

  if (!obu) return ck_iamf_parser_state_run;

  debug("Processing OBU type: %s", iamf_obu_type_string(obu->obu_type));

  if ((!ctx->key_frame && ctx->delimiter &&
       obu->obu_type != ck_iamf_obu_temporal_delimiter) ||
      iamf_obu_is_redundant_copy(obu)) {
    iamf_obu_free(obu);
    return ck_iamf_parser_state_run;
  }

  if (!ctx->key_frame && !ctx->delimiter &&
      obu->obu_type != ck_iamf_obu_temporal_delimiter)
    ctx->key_frame = 1;

  switch (obu->obu_type) {
    case ck_iamf_obu_parameter_block: {
      info("Processing parameter block OBU");

      iamf_parameter_block_obu_t *pbo = def_parameter_block_obu_ptr(obu);

      if (pbo->base->type == ck_iamf_parameter_type_demixing ||
          pbo->base->type == ck_iamf_parameter_type_recon_gain) {
        iamf_audio_element_obu_t *aeo =
            iamf_database_get_audio_element_obu_with_pid(
                database, pbo->base->parameter_id);
        parameter_subblock_t *subblock = def_value_wrap_type_ptr(
            parameter_subblock_t, array_at(pbo->subblocks, 0));
        if (subblock) {
          if (subblock->type == ck_iamf_parameter_type_demixing) {
            demixing_info_parameter_subblock_t *demixing_info =
                def_demixing_info_parameter_subblock_ptr(subblock);
            iamf_element_reconstructor_update_demixing_mode(
                self->reconstructor, aeo->audio_element_id,
                demixing_info->demixing_mode);
            trace("update demix mode %d", demixing_info->demixing_mode);
          } else {
            recon_gain_parameter_subblock_t *recon_gain =
                def_recon_gain_parameter_subblock_ptr(subblock);
            int index =
                iamf_element_reconstructor_get_channel_based_reconstructed_layout_index(
                    self->reconstructor, aeo->audio_element_id);

            int n = array_size(recon_gain->recon_gains);
            for (int i = 0; i < n; ++i) {
              recon_gain_t *recon_info = def_value_wrap_type_ptr(
                  recon_gain_t, array_at(recon_gain->recon_gains, i));
              if (recon_info && recon_info->layer == index) {
                iamf_element_reconstructor_update_recon_gain(
                    self->reconstructor, aeo->audio_element_id, recon_info);
                break;
              }
            }
          }
        }
      }

      // Provide parameter block OBU to iamf_database for processing
      int ret = iamf_database_add_obu(database, obu);
      if (ret != IAMF_OK) {
        error("Failed to add parameter block OBU to database");
        iamf_obu_free(obu);
      }
    } break;

    case ck_iamf_obu_temporal_delimiter: {
      if (!ctx->delimiter) ctx->delimiter = 1;
      if (!ctx->key_frame) {
        iamf_temporal_delimiter_obu_t *tdo =
            def_temporal_delimiter_obu_ptr(obu);
        if (tdo->key_frame) ctx->key_frame = 1;
      }
      iamf_obu_free(obu);
    } break;

    case ck_iamf_obu_audio_frame:
    case ck_iamf_obu_audio_frame_id0:
    case ck_iamf_obu_audio_frame_id1:
    case ck_iamf_obu_audio_frame_id2:
    case ck_iamf_obu_audio_frame_id3:
    case ck_iamf_obu_audio_frame_id4:
    case ck_iamf_obu_audio_frame_id5:
    case ck_iamf_obu_audio_frame_id6:
    case ck_iamf_obu_audio_frame_id7:
    case ck_iamf_obu_audio_frame_id8:
    case ck_iamf_obu_audio_frame_id9:
    case ck_iamf_obu_audio_frame_id10:
    case ck_iamf_obu_audio_frame_id11:
    case ck_iamf_obu_audio_frame_id12:
    case ck_iamf_obu_audio_frame_id13:
    case ck_iamf_obu_audio_frame_id14:
    case ck_iamf_obu_audio_frame_id15:
    case ck_iamf_obu_audio_frame_id16:
    case ck_iamf_obu_audio_frame_id17: {
      iamf_audio_frame_obu_t *afo = def_audio_frame_obu_ptr(obu);
      int ret = iamf_element_reconstructor_add_audio_frame_obu(
          self->reconstructor, afo);
      if (ret == IAMF_ERR_INVALID_PACKET) {
        iamf_database_time_elapse(&self->ctx.database,
                                  self->ctx.frame_duration);
      } else if (ret > 0) {
        iamf_audio_block_t *audio_block =
            iamf_element_reconstructor_process(self->reconstructor);
        if (audio_block) {
          uint32_t _2nd_skip = audio_block->second_skip;
          fraction_t frame_duration = self->ctx.frame_duration;
#if SR
          iamf_rec_stream_log(audio_block->id, audio_block->num_channels,
                              audio_block->data,
                              audio_block->capacity_per_channel);
#endif
          frame_duration.numerator -= _2nd_skip;
          ret = iamf_presentation_add_audio_block(presentation, audio_block);
          if (ret > 0) {
            state = ck_iamf_parser_state_stop;
          } else if (ret == IAMF_ERR_INVALID_PACKET) {
            iamf_database_time_elapse(&self->ctx.database, frame_duration);
          }
        } else {
          warning("No audio block available.");
        }
      }
    } break;

    case ck_iamf_obu_sequence_header: {
      info("*********** FOUND NEW MAGIC CODE **********");
      state = ck_iamf_parser_state_ahead;
    }

    default: {
      iamf_obu_free(obu);
    } break;
  }

  return state;
}

static iamf_parser_state_t iamf_decoder_priv_process_obus(iamf_obu_t *obu,
                                                          void *user_data) {
  iamf_decoder_t *self = (iamf_decoder_t *)user_data;
  iamf_decoder_context_t *ctx = &self->ctx;

  if (ctx->status == ck_iamf_decoder_status_parse_1)
    return iamf_decoder_priv_process_descriptor_obus(self, obu);
  else if (ctx->status == ck_iamf_decoder_status_parse_2)
    return iamf_decoder_priv_process_data_obus(self, obu);

  warning("Unexpected state %d of decoder.", ctx->status);
  return ck_iamf_parser_state_stop;
}

static int iamf_decoder_priv_create_parser(iamf_decoder_t *self) {
  iamf_database_t *database = &self->ctx.database;
  iamf_obu_extra_parameters_t eparam;

  memset(&eparam, 0, sizeof(iamf_obu_extra_parameters_t));
  eparam.pbo_interfaces.this = database;
  eparam.pbo_interfaces.get_parameter_base =
      (fun_get_parameter_base_t)iamf_database_get_parameter_base;
  eparam.pbo_interfaces.get_recon_gain_present_flags =
      (fun_get_recon_gain_present_flags_t)
          iamf_database_get_recon_gain_present_flags;

  self->parser =
      iamf_obu_parser_create(iamf_decoder_priv_process_obus, self, &eparam);
  return self->parser ? IAMF_OK : IAMF_ERR_ALLOC_FAIL;
}

static void iamf_decoder_priv_display_stream_info(iamf_stream_info_t *info) {
  if (!info) {
    error("Stream info is null");
    return;
  }

  info("=== IAMF Stream Information ===");
  info("Max frame size: %u samples", info->max_frame_size);

  // Display profile information
  info("Primary profile: %d", info->iamf_stream_info.primary_profile);
  info("Additional profile: %d", info->iamf_stream_info.additional_profile);

  // Display codec information
  info("Codec ID 0: %d", info->iamf_stream_info.codec_ids[0]);
  info("Codec ID 1: %d", info->iamf_stream_info.codec_ids[1]);
  info("Sampling rate: %u Hz", info->iamf_stream_info.sampling_rate);
  info("Samples per channel in frame: %u",
       info->iamf_stream_info.samples_per_channel_in_frame);

  // Display mix presentation information
  info("Mix presentation count: %u",
       info->iamf_stream_info.mix_presentation_count);

  if (info->iamf_stream_info.mix_presentations) {
    for (uint32_t i = 0; i < info->iamf_stream_info.mix_presentation_count;
         ++i) {
      iamf_mix_presentation_info_t *mix_presentation =
          &info->iamf_stream_info.mix_presentations[i];

      info("  Mix Presentation[%u]:", i);
      info("    ID: %u", mix_presentation->id);
      info("    Number of audio elements: %u",
           mix_presentation->num_audio_elements);

      if (mix_presentation->elements) {
        for (uint32_t j = 0; j < mix_presentation->num_audio_elements; ++j) {
          iamf_element_presentation_info_t *element =
              &mix_presentation->elements[j];
          info("    Element[%u]: ID=%u, Mode=%d, Profile=%d", j, element->eid,
               element->mode, element->profile);

          if (element->gain_offset_range) {
            info("      Gain Offset Range: Min=%.2f dB, Max=%.2f dB",
                 element->gain_offset_range->min,
                 element->gain_offset_range->max);
          }
        }
      }
    }
  }

  info("=== End of Stream Information ===");
}

/* ----------------------------- APIs ----------------------------- */

IAMF_DecoderHandle IAMF_decoder_open(void) {
  iamf_decoder_t *self = 0;
  info("open iamf decoder.");
  self = def_mallocz(iamf_decoder_t, 1);
  if (self) {
    iamf_decoder_context_t *ctx = &self->ctx;

    ctx->status = ck_iamf_decoder_status_init;

    ctx->sampling_rate = def_default_sampling_rate;
    ctx->mix_presentation_id = def_i64_id_none;
    ctx->loudness_lkfs = def_default_loudness_lkfs;
    ctx->normalized_loudness_lkfs = def_default_loudness_lkfs;
    ctx->limiter_threshold_db = def_limiter_max_true_peak;
    ctx->enable_limiter = 1;

    ctx->head_rotation = def_quaternion_instance(1.0, 0.0, 0.0, 0.0);

    self->reconstructor = iamf_element_reconstructor_create();
    self->post_processor = iamf_post_processor_create();

    if (iamf_decoder_priv_create_parser(self) != IAMF_OK ||
        !self->reconstructor || !self->post_processor ||
        iamf_database_init(&ctx->database) != IAMF_OK) {
      IAMF_decoder_close(self);
      return 0;
    }
    iamf_database_set_profile(&ctx->database, def_iamf_profile_default);
  }
  return self;
}

int IAMF_decoder_close(IAMF_DecoderHandle handle) {
  iamf_decoder_t *self = (iamf_decoder_t *)handle;
  info("close iamf decoder.");
#if SR
  iamf_stream_log_free();
#endif
  if (self) {
    iamf_decoder_context_t *ctx = &self->ctx;
    iamf_database_uninit(&ctx->database);
    if (ctx->element_gains) hash_map_delete(ctx->element_gains, 0);

    if (self->parser) iamf_obu_parser_destroy(self->parser);
    if (self->reconstructor)
      iamf_element_reconstructor_destroy(self->reconstructor);
    if (self->presentation) iamf_presentation_destroy(self->presentation);
    if (self->post_processor) iamf_post_processor_destroy(self->post_processor);
  }
  def_free(self);
  return 0;
}

int IAMF_decoder_set_profile(IAMF_DecoderHandle handle, IA_Profile profile) {
  iamf_decoder_t *self = (iamf_decoder_t *)handle;
  iamf_profile_t _profile = def_iamf_profile_cast(profile);
  if (!self) return IAMF_ERR_BAD_ARG;

  if (_profile < ck_iamf_profile_none ||
      _profile > ck_iamf_profile_advanced_2) {
    return IAMF_ERR_BAD_ARG;
  }

  info("set profile %s (%d)", iamf_profile_type_string(_profile), _profile);

  return iamf_database_set_profile(&self->ctx.database, _profile);
}

IA_Profile IAMF_decoder_get_profile(IAMF_DecoderHandle handle) {
  iamf_decoder_t *self = (iamf_decoder_t *)handle;
  if (!self) return def_ia_profile_cast(ck_iamf_profile_none);

  return def_ia_profile_cast(iamf_database_get_profile(&self->ctx.database));
}

int IAMF_decoder_configure(IAMF_DecoderHandle handle, const uint8_t *data,
                           uint32_t size, uint32_t *rsize) {
  iamf_decoder_t *self = (iamf_decoder_t *)handle;
  uint32_t read = 0;
  int ret = iamf_decoder_priv_configure(self, data, size, &read);

  if (rsize) {
    *rsize = read;
    if ((ret != IAMF_OK && ret != IAMF_ERR_BUFFER_TOO_SMALL) ||
        (ret == IAMF_ERR_BUFFER_TOO_SMALL &&
         self->ctx.status != ck_iamf_decoder_status_parse_2))
      error("fail to configure decoder.");
    return ret;
  }

  if (ret == IAMF_ERR_BUFFER_TOO_SMALL &&
      iamf_database_descriptors_complete(&self->ctx.database)) {
    self->ctx.configure_flags |= def_iamf_decoder_config_presentation;
    self->ctx.status = ck_iamf_decoder_status_parse_2;
    info("configure with complete descriptor OBUs.");
    ret = iamf_decoder_priv_configure(self, 0, 0, 0);
  }

  if (ret != IAMF_OK) error("fail to configure decoder.");

  return ret;
}

int IAMF_decoder_decode(IAMF_DecoderHandle handle, const uint8_t *data,
                        int32_t size, uint32_t *rsize, void *pcm) {
  iamf_decoder_t *self = (iamf_decoder_t *)handle;
  uint32_t read = 0;
  int ret = IAMF_OK;

  if (!self) return IAMF_ERR_BAD_ARG;
  trace("decode iamf decoder. data %p, size %d, statue %d", data, size,
        self->ctx.status);
  if (self->ctx.status != ck_iamf_decoder_status_parse_2)
    return IAMF_ERR_INVALID_STATE;
  ret = iamf_decoder_priv_decode(self, data, size, &read, pcm);
  if (rsize) *rsize = read;
  return ret;
}

int IAMF_decoder_output_layout_set_sound_system(IAMF_DecoderHandle handle,
                                                IAMF_SoundSystem ss) {
  iamf_decoder_t *self = (iamf_decoder_t *)handle;
  iamf_decoder_context_t *ctx;
  if (!self) return IAMF_ERR_BAD_ARG;
  if (!iamf_sound_system_check(ss)) return IAMF_ERR_BAD_ARG;

  ctx = &self->ctx;
  if (ctx->layout.type == ck_iamf_layout_type_loudspeakers_ss_convention &&
      ctx->layout.sound_system == ss)
    return IAMF_OK;

  info("sound system %d, channels %d", ss,
       IAMF_layout_sound_system_channels_count(ss));

  ctx->layout.type = ck_iamf_layout_type_loudspeakers_ss_convention;
  ctx->layout.sound_system = ss;
  ctx->configure_flags |= def_iamf_decoder_config_output_layout;
  return IAMF_OK;
}

int IAMF_decoder_output_layout_set_binaural(IAMF_DecoderHandle handle) {
  iamf_decoder_t *self = (iamf_decoder_t *)handle;
  iamf_decoder_context_t *ctx;

  if (!self) return IAMF_ERR_BAD_ARG;

  ctx = &self->ctx;
  if (ctx->layout.type == ck_iamf_layout_type_binaural) return IAMF_OK;

  info("binaural channels %d", IAMF_layout_binaural_channels_count());
  ctx->layout.type = ck_iamf_layout_type_binaural;
  ctx->configure_flags |= def_iamf_decoder_config_output_layout;
  return IAMF_OK;
}

int IAMF_decoder_set_mix_presentation_id(IAMF_DecoderHandle handle,
                                         uint64_t id64) {
  iamf_decoder_t *self = (iamf_decoder_t *)handle;
  iamf_decoder_context_t *ctx;
  uint32_t id = def_lsb_32bits(id64);

  if (!self || id64 > UINT32_MAX) return IAMF_ERR_BAD_ARG;

  ctx = &self->ctx;

  if (ctx->mix_presentation_id == id) {
    debug("mix presentation id %u is same as current setting. skip it. ",
          ctx->mix_presentation_id);
    return IAMF_OK;
  }

  ctx->mix_presentation_id = id;
  if (!self->presentation ||
      ctx->mix_presentation_id != iamf_presentation_get_id(self->presentation))
    ctx->configure_flags |= def_iamf_decoder_config_mix_presentation;
  info("set new mix presentation id %" PRId64 ".", ctx->mix_presentation_id);

  if (ctx->status > ck_iamf_decoder_status_configure)
    iamf_decoder_priv_configure(self, 0, 0, 0);

  return IAMF_OK;
}

int64_t IAMF_decoder_get_mix_presentation_id(IAMF_DecoderHandle handle) {
  iamf_decoder_t *self = (iamf_decoder_t *)handle;
  return self ? self->ctx.mix_presentation_id : def_i64_id_none;
}

int IAMF_layout_sound_system_channels_count(IAMF_SoundSystem ss) {
  const iamf_layout_info_t *info =
      iamf_layout_get_info(def_sound_system_layout_instance(ss));
  if (!info) {
    error("invalid sound system %x", ss);
    return IAMF_ERR_BAD_ARG;
  }
  debug("sound system %x, channels %d", ss, info->channels);
  return info->channels;
}

int IAMF_layout_binaural_channels_count() { return 2; }

char *IAMF_decoder_get_codec_capability() {
  char *ccs_str = def_mallocz(char, def_ccs_str_size);
  char cc_str[def_cc_str_size];

  if (!ccs_str) return 0;

  snprintf(cc_str, def_cc_str_size, "iamf.%.03d.%.03d.ipcm",
           def_iamf_profile_default, def_iamf_profile_default);
  strncat(ccs_str, cc_str, def_cc_str_size);

#ifdef CONFIG_OPUS_CODEC
  snprintf(cc_str, def_cc_str_size, ";iamf.%.03d.%.03d.Opus",
           def_iamf_profile_default, def_iamf_profile_default);
  strncat(ccs_str, cc_str, def_cc_str_size);
#endif

#ifdef CONFIG_AAC_CODEC
  snprintf(cc_str, def_cc_str_size, ";iamf.%.03d.%.03d.mp4a.40.2",
           def_iamf_profile_default, def_iamf_profile_default);
  strncat(ccs_str, cc_str, def_cc_str_size);
#endif

#ifdef CONFIG_FLAC_CODEC
  snprintf(cc_str, def_cc_str_size, ";iamf.%.03d.%.03d.fLaC",
           def_iamf_profile_default, def_iamf_profile_default);
  strncat(ccs_str, cc_str, def_cc_str_size);
#endif

  return ccs_str;
}

int IAMF_decoder_set_normalization_loudness(IAMF_DecoderHandle handle,
                                            float loudness) {
  iamf_decoder_t *self = (iamf_decoder_t *)handle;
  if (!self) return IAMF_ERR_BAD_ARG;
  if (self->ctx.normalized_loudness_lkfs != loudness) {
    self->ctx.normalized_loudness_lkfs = loudness;
    if (self->ctx.status > ck_iamf_decoder_status_configure) {
      iamf_presentation_set_loudness_gain(
          self->presentation,
          f32_db_to_linear(self->ctx.normalized_loudness_lkfs -
                           self->ctx.loudness_lkfs));
    }
  }
  return IAMF_OK;
}

int IAMF_decoder_set_bit_depth(IAMF_DecoderHandle handle, uint32_t bit_depth) {
  iamf_decoder_t *self = (iamf_decoder_t *)handle;
  iamf_sample_bit_depth_t valid_bit_depths[] = {
      SAMPLE_BIT_DEPTH_16, SAMPLE_BIT_DEPTH_24, SAMPLE_BIT_DEPTH_32};
  int ret = IAMF_ERR_BAD_ARG;

  if (!self) return IAMF_ERR_BAD_ARG;

  for (int i = 0;
       i < sizeof(valid_bit_depths) / sizeof(iamf_sample_bit_depth_t); ++i) {
    if (bit_depth == valid_bit_depths[i]) {
      self->ctx.bit_depth = bit_depth;
      ret = IAMF_OK;
      break;
    }
  }
  return ret;
}

int IAMF_decoder_peak_limiter_enable(IAMF_DecoderHandle handle,
                                     uint32_t enable) {
  iamf_decoder_t *self = (iamf_decoder_t *)handle;
  if (!self) return IAMF_ERR_BAD_ARG;
  self->ctx.enable_limiter = enable;
  return IAMF_OK;
}

int IAMF_decoder_peak_limiter_set_threshold(IAMF_DecoderHandle handle,
                                            float db) {
  iamf_decoder_t *self = (iamf_decoder_t *)handle;
  if (!self) return IAMF_ERR_BAD_ARG;
  self->ctx.limiter_threshold_db = db;
  return IAMF_OK;
}

float IAMF_decoder_peak_limiter_get_threshold(IAMF_DecoderHandle handle) {
  iamf_decoder_t *self = (iamf_decoder_t *)handle;
  if (!self) return def_limiter_max_true_peak;
  return self->ctx.limiter_threshold_db;
}

int IAMF_decoder_set_sampling_rate(IAMF_DecoderHandle handle, uint32_t rate) {
  iamf_decoder_t *self = (iamf_decoder_t *)handle;
  IAMF_SampleBitDepth valid_sampling_rates[] = {
      SAMPLING_RATE_8000,  SAMPLING_RATE_11025, SAMPLING_RATE_16000,
      SAMPLING_RATE_22050, SAMPLING_RATE_24000, SAMPLING_RATE_32000,
      SAMPLING_RATE_44100, SAMPLING_RATE_48000, SAMPLING_RATE_64000,
      SAMPLING_RATE_88200, SAMPLING_RATE_96000, SAMPLING_RATE_176400,
      SAMPLING_RATE_192000};
  int ret = IAMF_ERR_BAD_ARG;

  if (!self) return IAMF_ERR_BAD_ARG;

  for (int i = 0;
       i < sizeof(valid_sampling_rates) / sizeof(IAMF_SampleBitDepth); ++i) {
    if (rate == valid_sampling_rates[i]) {
      if (rate != self->ctx.sampling_rate) self->ctx.sampling_rate = rate;
      ret = IAMF_OK;
      break;
    }
  }
  return ret;
}

int IAMF_decoder_set_audio_element_gain_offset(IAMF_DecoderHandle handle,
                                               uint32_t id, float gain) {
  iamf_decoder_t *self = (iamf_decoder_t *)handle;
  iamf_decoder_context_t *ctx;

  if (!self) return IAMF_ERR_BAD_ARG;
  if (self->presentation)
    return iamf_presentation_set_element_gain_offset(self->presentation, id,
                                                     gain);

  ctx = &self->ctx;
  if (!ctx->element_gains)
    ctx->element_gains = hash_map_new(def_hash_map_capacity_elements);
  if (!ctx->element_gains) return IAMF_ERR_ALLOC_FAIL;

  return hash_map_put(ctx->element_gains, id,
                      def_value_wrap_instance_f32(gain)) < 0
             ? IAMF_ERR_INTERNAL
             : IAMF_ERR_PENDING;
}

int IAMF_decoder_get_audio_element_gain_offset(IAMF_DecoderHandle handle,
                                               uint32_t id, float *gain) {
  iamf_decoder_t *self = (iamf_decoder_t *)handle;
  iamf_decoder_context_t *ctx;
  value_wrap_t *value;

  if (!self || !gain) return IAMF_ERR_BAD_ARG;
  if (self->presentation)
    return iamf_presentation_get_element_gain_offset(self->presentation, id,
                                                     gain);

  ctx = &self->ctx;
  if (!ctx->element_gains) return IAMF_ERR_INVALID_STATE;

  value = hash_map_get(ctx->element_gains, id);
  if (!value) return IAMF_ERR_BAD_ARG;

  *gain = def_value_wrap_f32(value);
  return IAMF_OK;
}

int IAMF_decoder_set_head_rotation(IAMF_DecoderHandle handle, float w, float x,
                                   float y, float z) {
  iamf_decoder_t *self = (iamf_decoder_t *)handle;
  iamf_decoder_context_t *ctx;
  int ret = IAMF_OK;

  if (!self) return IAMF_ERR_BAD_ARG;

  ctx = &self->ctx;

  // Validate quaternion (should be approximately unit length)
  float quaternion_length = sqrtf(w * w + x * x + y * y + z * z);
  if (quaternion_length < 0.001f) {
    warning("Head rotation quaternion is too close to zero, ignoring");
    return IAMF_ERR_BAD_ARG;
  }

  // Normalize quaternion to ensure it's unit length
  if (fabsf(quaternion_length - 1.0f) > 0.001f) {
    w /= quaternion_length;
    x /= quaternion_length;
    y /= quaternion_length;
    z /= quaternion_length;
    debug("Normalized head rotation quaternion: w=%f, x=%f, y=%f, z=%f", w, x,
          y, z);
  }

  ctx->head_rotation.w = w;
  ctx->head_rotation.x = x;
  ctx->head_rotation.y = y;
  ctx->head_rotation.z = z;

  debug("Set head rotation: w=%f, x=%f, y=%f, z=%f", w, x, y, z);

  if (self->presentation) {
    ret = iamf_presentation_set_head_rotation(self->presentation,
                                              &ctx->head_rotation);
  } else {
    debug(
        "Presentation not active, head rotation stored for later application");
    ret = IAMF_ERR_PENDING;
  }

  return ret;
}

int IAMF_decoder_enable_head_tracking(IAMF_DecoderHandle handle,
                                      uint32_t enable) {
  iamf_decoder_t *self = (iamf_decoder_t *)handle;
  iamf_decoder_context_t *ctx;
  int ret = IAMF_OK;

  if (!self) return IAMF_ERR_BAD_ARG;

  ctx = &self->ctx;

  if (ctx->head_tracking_enabled == enable) return IAMF_OK;

  ctx->head_tracking_enabled = enable;

  debug("Head tracking %s in decoder", enable ? "enabled" : "disabled");

  if (self->presentation) {
    ret = iamf_presentation_enable_head_tracking(self->presentation, enable);
  } else {
    debug(
        "Presentation not active, head tracking setting stored for later "
        "application");
    ret = IAMF_ERR_PENDING;
  }

  return ret;
}

IAMF_StreamInfo *IAMF_decoder_get_stream_info(IAMF_DecoderHandle handle) {
  iamf_decoder_t *self = (iamf_decoder_t *)handle;
  if (!self) return 0;

  iamf_decoder_context_t *ctx = &self->ctx;

  if (ctx->status <= ck_iamf_decoder_status_parse_1) return 0;

  IAMF_StreamInfo *info = def_mallocz(IAMF_StreamInfo, 1);
  if (!info) {
    error("Failed to allocate memory for IAMF_StreamInfo");
    return 0;
  }

  iamf_database_t *database = &ctx->database;
  obu_audio_element_config_t *audio_element_config = 0;

  info->max_frame_size =
      iamf_fraction_transform(ctx->frame_duration, ctx->sampling_rate);
  info->max_frame_size += iamf_post_processor_get_delay(self->post_processor);

  if (database->descriptors.ia_sequence_header_obu) {
    info->iamf_stream_info.primary_profile = def_ia_profile_cast(
        database->descriptors.ia_sequence_header_obu->primary_profile);
    info->iamf_stream_info.additional_profile = def_ia_profile_cast(
        database->descriptors.ia_sequence_header_obu->additional_profile);
  } else {
    info->iamf_stream_info.primary_profile =
        info->iamf_stream_info.additional_profile =
            def_ia_profile_cast(iamf_database_get_profile(database));
  }

  codec_config_t *db_codec_config = def_value_wrap_type_ptr(
      codec_config_t, vector_at(database->codec_configs, 0));

  info->iamf_stream_info.codec_ids[0] =
      def_iamf_codec_id_cast(db_codec_config->codec_param.type);
  info->iamf_stream_info.codec_ids[1] =
      def_iamf_codec_id_cast(ck_iamf_codec_type_none);
  info->iamf_stream_info.sampling_rate =
      db_codec_config->codec_param.sample_rate;
  info->iamf_stream_info.samples_per_channel_in_frame =
      db_codec_config->codec_param.frame_size;

  int num_codec_configs = vector_size(database->codec_configs);
  if (num_codec_configs > 1) {
    db_codec_config = def_value_wrap_type_ptr(
        codec_config_t, vector_at(database->codec_configs, 1));
    info->iamf_stream_info.codec_ids[1] = db_codec_config->codec_param.type;
  }

  if (info->iamf_stream_info.codec_ids[0] ==
          def_iamf_codec_id_cast(ck_iamf_codec_type_opus) ||
      info->iamf_stream_info.codec_ids[1] ==
          def_iamf_codec_id_cast(ck_iamf_codec_type_opus))
    info->iamf_stream_info.sampling_rate = SAMPLING_RATE_48000;

  info->iamf_stream_info.mix_presentation_count =
      vector_size(database->descriptors.mix_presentation_obus);

  if (info->iamf_stream_info.mix_presentation_count > 0) {
    info->iamf_stream_info.mix_presentations =
        def_mallocz(iamf_mix_presentation_info_t,
                    info->iamf_stream_info.mix_presentation_count);
    if (info->iamf_stream_info.mix_presentations) {
      for (int i = 0; i < info->iamf_stream_info.mix_presentation_count; ++i) {
        iamf_mix_presentation_obu_t *mpo = def_value_wrap_type_ptr(
            iamf_mix_presentation_obu_t,
            vector_at(database->descriptors.mix_presentation_obus, i));

        if (mpo) {
          info->iamf_stream_info.mix_presentations[i].id =
              mpo->mix_presentation_id;
          int num_sub_mixes = array_size(mpo->sub_mixes);

          // Count total audio elements across all sub-mixes
          uint32_t total_audio_elements = 0;
          for (int sub_idx = 0; sub_idx < num_sub_mixes; ++sub_idx) {
            obu_sub_mix_t *sub =
                def_value_wrap_optional_ptr(array_at(mpo->sub_mixes, sub_idx));
            if (sub) {
              total_audio_elements += array_size(sub->audio_element_configs);
            }
          }

          // Allocate elements array
          if (total_audio_elements > 0) {
            info->iamf_stream_info.mix_presentations[i].elements = def_mallocz(
                iamf_element_presentation_info_t, total_audio_elements);
            info->iamf_stream_info.mix_presentations[i].num_audio_elements =
                total_audio_elements;

            uint32_t element_idx = 0;
            for (int sub_idx = 0; sub_idx < num_sub_mixes; ++sub_idx) {
              obu_sub_mix_t *sub = def_value_wrap_optional_ptr(
                  array_at(mpo->sub_mixes, sub_idx));

              if (sub) {
                int num_audio_elements = array_size(sub->audio_element_configs);

                for (int elem_idx = 0; elem_idx < num_audio_elements;
                     ++elem_idx) {
                  audio_element_config = def_value_wrap_optional_ptr(
                      array_at(sub->audio_element_configs, elem_idx));

                  if (audio_element_config &&
                      element_idx < total_audio_elements) {
                    // Set element ID
                    info->iamf_stream_info.mix_presentations[i]
                        .elements[element_idx]
                        .eid = audio_element_config->element_id;

                    info->iamf_stream_info.mix_presentations[i]
                        .elements[element_idx]
                        .mode = audio_element_config->rendering_config
                                    .headphones_rendering_mode;
                    info->iamf_stream_info.mix_presentations[i]
                        .elements[element_idx]
                        .profile = audio_element_config->rendering_config
                                       .binaural_filter_profile;

                    // Check for gain offset range
                    if ((audio_element_config->rendering_config.flags &
                         def_rendering_config_flag_element_gain_offset) &&
                        (audio_element_config->rendering_config
                             .element_gain_offset_type ==
                         ck_element_gain_offset_type_range)) {
                      info->iamf_stream_info.mix_presentations[i]
                          .elements[element_idx]
                          .gain_offset_range =
                          def_mallocz(iamf_element_gain_offset_range_t, 1);
                      if (info->iamf_stream_info.mix_presentations[i]
                              .elements[element_idx]
                              .gain_offset_range) {
                        info->iamf_stream_info.mix_presentations[i]
                            .elements[element_idx]
                            .gain_offset_range->min =
                            audio_element_config->rendering_config
                                .element_gain_offset_db.min;
                        info->iamf_stream_info.mix_presentations[i]
                            .elements[element_idx]
                            .gain_offset_range->max =
                            audio_element_config->rendering_config
                                .element_gain_offset_db.max;
                      }
                    } else {
                      info->iamf_stream_info.mix_presentations[i]
                          .elements[element_idx]
                          .gain_offset_range = 0;
                    }

                    element_idx++;
                  }
                }
              }
            }
          } else {
            info->iamf_stream_info.mix_presentations[i].elements = 0;
            info->iamf_stream_info.mix_presentations[i].num_audio_elements = 0;
          }
        }
      }
    }
  }

  // Display stream information for debugging
  iamf_decoder_priv_display_stream_info(info);

  return info;
}

void IAMF_decoder_free_stream_info(iamf_stream_info_t *info) {
  if (!info) return;

  if (info->iamf_stream_info.mix_presentations) {
    for (uint32_t i = 0; i < info->iamf_stream_info.mix_presentation_count;
         ++i) {
      if (info->iamf_stream_info.mix_presentations[i].elements) {
        for (uint32_t j = 0;
             j < info->iamf_stream_info.mix_presentations[i].num_audio_elements;
             ++j) {
          def_free(info->iamf_stream_info.mix_presentations[i]
                       .elements[j]
                       .gain_offset_range);
        }
        def_free(info->iamf_stream_info.mix_presentations[i].elements);
      }
    }
    def_free(info->iamf_stream_info.mix_presentations);
  }

  def_free(info);
}
