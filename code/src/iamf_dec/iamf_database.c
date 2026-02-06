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
 * @file iamf_database.c
 * @brief IAMF database implementation.
 * @version 2.0.0
 * @date Created 12/11/2025
 **/

#include "iamf_database.h"

#include <math.h>
#include <stdlib.h>

#include "clog.h"
#include "iamf_layout.h"
#include "iamf_obu_parser.h"
#include "iamf_private_definitions.h"
#include "iamf_string.h"
#include "iamf_utils.h"
#include "obu/iamf_obu_all.h"

#undef def_log_tag
#define def_log_tag "IAMF_DB"

static int _iadb_polar_copy(polar_t *dst, polar_t *src) {
  if (!dst || !src) return IAMF_ERR_BAD_ARG;

  dst->azimuth = src->azimuth;
  dst->elevation = src->elevation;
  dst->distance = src->distance;

  return IAMF_OK;
}

static void _iadb_parameter_block_free(parameter_block_t *block) {
  if (block->subblocks)
    deque_free(block->subblocks,
               def_default_free_ptr(iamf_parameter_subblock_free));
  free(block);
}

static parameter_block_t *_iadb_parameter_block_new(parameter_base_t *base) {
  parameter_block_t *block = 0;
  if (base->type == ck_iamf_parameter_type_mix_gain) {
    mix_gain_parameter_block_t *pbk =
        def_mallocz(mix_gain_parameter_block_t, 1);
    mix_gain_parameter_base_t *mgpbs = (mix_gain_parameter_base_t *)base;
    if (pbk) {
      block = def_parameter_block_ptr(pbk);
      pbk->end_gain = mgpbs->default_mix_gain_db;
    }
  } else if (base->type == ck_iamf_parameter_type_polar ||
             base->type == ck_iamf_parameter_type_dual_polar) {
    polars_parameter_block_t *pbk = def_mallocz(polars_parameter_block_t, 1);
    polars_parameter_base_t *pbs = def_polars_parameter_base_ptr(base);
    if (pbk) {
      block = def_parameter_block_ptr(pbk);
      pbk->num_polars = pbs->num_polars;
      for (int i = 0; i < pbk->num_polars; ++i) {
        _iadb_polar_copy(&pbk->end_polars[i], &pbs->default_polars[i]);
      }
    }
  } else if (iamf_parameter_type_is_cartesian(base->type)) {
    cartesians_parameter_block_t *pbk =
        def_mallocz(cartesians_parameter_block_t, 1);
    cartesians_parameter_base_t *cbs = (cartesians_parameter_base_t *)base;
    if (pbk) {
      block = def_parameter_block_ptr(pbk);
      pbk->num_cartesians = cbs->num_cartesians;
      for (int i = 0; i < pbk->num_cartesians; ++i) {
        pbk->end_cartesians[i].x = cbs->default_cartesians[i].x;
        pbk->end_cartesians[i].y = cbs->default_cartesians[i].y;
        pbk->end_cartesians[i].z = cbs->default_cartesians[i].z;
      }
    }
  } else {
    block = def_mallocz(parameter_block_t, 1);
  }

  if (block) {
    block->base = base;
    block->subblocks = deque_new();
  }
  return block;
}

static int _iadb_parameter_block_find(value_wrap_t item, value_wrap_t key) {
  return def_value_wrap_type_ptr(parameter_block_t, &item)
             ->base->parameter_id == key.u32;
}

static int _iadb_parameter_block_manager_add(parameter_block_manager_t *manager,
                                             parameter_base_t *base) {
  value_wrap_t v;

  if (vector_find(manager->parameter_blocks,
                  def_value_wrap_instance_u32(base->parameter_id),
                  _iadb_parameter_block_find, &v) < 0) {
    parameter_block_t *block = _iadb_parameter_block_new(base);

    if (base->type == ck_iamf_parameter_type_demixing) {
      vector_push(manager->demixing_infos, def_value_wrap_instance_ptr(block));
    } else if (base->type == ck_iamf_parameter_type_recon_gain) {
      vector_push(manager->recon_gains, def_value_wrap_instance_ptr(block));
    } else if (base->type == ck_iamf_parameter_type_mix_gain) {
      vector_push(manager->mix_gains, def_value_wrap_instance_ptr(block));
    } else if (base->type == ck_iamf_parameter_type_momentary_loudness) {
      vector_push(manager->coordinates, def_value_wrap_instance_ptr(block));
    } else if (iamf_parameter_type_is_coordinate(base->type)) {
      vector_push(manager->coordinates, def_value_wrap_instance_ptr(block));
    } else {
      _iadb_parameter_block_free(block);
      return IAMF_ERR_BAD_ARG;
    }
    vector_push(manager->parameter_blocks, def_value_wrap_instance_ptr(block));
  }

  return IAMF_OK;
}

static int _iadb_parameter_block_manager_add_obu(
    parameter_block_manager_t *manager, iamf_parameter_block_obu_t *obu) {
  value_wrap_t v;

  if (vector_find(manager->parameter_blocks,
                  def_value_wrap_instance_u32(obu->base->parameter_id),
                  _iadb_parameter_block_find, &v) >= 0) {
    parameter_block_t *block = def_value_wrap_type_ptr(parameter_block_t, &v);
    int n = array_size(obu->subblocks);
    if (n <= 0) return IAMF_ERR_BAD_ARG;

    if (obu->obu.obu_flags & ck_iamf_obu_flag_redundant &&
        deque_length(block->subblocks) > 0) {
      debug("Ignore redundant parameter obu with id %u.",
            obu->base->parameter_id);
    } else {
      int idx = deque_length(block->subblocks);
      if (block->reset_start_time || block->elapsed < 0 ||
          block->elapsed > block->duration) {
        if (block->base->type == ck_iamf_parameter_type_mix_gain) {
          mix_gain_parameter_block_t *pblock =
              def_mix_gain_parameter_block_ptr(block);
          mix_gain_parameter_base_t *pbase =
              def_mix_gain_parameter_base_ptr(block->base);
          pblock->end_gain = pbase->default_mix_gain_db;
          debug("Use default mix gain (%d dB) as starting value for pid %u.",
                pblock->end_gain, block->base->parameter_id);
        } else if (iamf_parameter_type_is_polar(block->base->type)) {
          polars_parameter_block_t *pblock =
              def_polars_parameter_block_ptr(block);
          polars_parameter_base_t *pbase =
              def_polars_parameter_base_ptr(block->base);
          for (int i = 0; i < pblock->num_polars; ++i) {
            pblock->end_polars[i].azimuth = pbase->default_polars[i].azimuth;
            pblock->end_polars[i].elevation =
                pbase->default_polars[i].elevation;
            pblock->end_polars[i].distance = pbase->default_polars[i].distance;
            debug(
                "Use default polar coordinates (%f, %f, %f) as starting values "
                "for pid %u - %d.",
                pblock->end_polars[i].azimuth, pblock->end_polars[i].elevation,
                pblock->end_polars[i].distance, block->base->parameter_id, i);
          }
        } else if (iamf_parameter_type_is_cartesian(block->base->type)) {
          cartesians_parameter_block_t *pblock =
              def_cartesians_parameter_block_ptr(block);
          cartesians_parameter_base_t *pbase =
              def_cartesians_parameter_base_ptr(block->base);
          for (int i = 0; i < pblock->num_cartesians; ++i) {
            pblock->end_cartesians[i].x = pbase->default_cartesians[i].x;
            pblock->end_cartesians[i].y = pbase->default_cartesians[i].y;
            pblock->end_cartesians[i].z = pbase->default_cartesians[i].z;
            debug(
                "Use default cartesian coordinates (%f, %f, %f) as starting "
                "values for pid %u - %d.",
                pblock->end_cartesians[i].x, pblock->end_cartesians[i].y,
                pblock->end_cartesians[i].z, block->base->parameter_id, i);
          }
        }
        block->reset_start_time = 0;
      }

      for (int i = 0; i < n; ++i) {
        value_wrap_t *pv = array_at(obu->subblocks, i);
        parameter_subblock_t *psb = def_parameter_subblock_ptr(pv->ptr);
        block->duration += psb->subblock_duration;
        deque_push_back(block->subblocks, *pv);
        pv->ptr = 0;

        if (psb->type == ck_iamf_parameter_type_mix_gain) {
          mix_gain_parameter_block_t *pblock =
              def_mix_gain_parameter_block_ptr(block);
          mix_gain_parameter_subblock_t *sub =
              def_mix_gain_parameter_subblock_ptr(psb);

          if (sub->gain.animation_type == ck_iamf_animation_type_inter_linear ||
              sub->gain.animation_type == ck_iamf_animation_type_inter_bezier) {
            sub->gain_db.data.start = pblock->end_gain;
            if (sub->gain.animation_type == ck_iamf_animation_type_inter_linear)
              sub->gain_db.animation_type = ck_iamf_animation_type_linear;
            else
              sub->gain_db.animation_type = ck_iamf_animation_type_bezier;
          }

          if (sub->gain.animation_type == ck_iamf_animation_type_step)
            pblock->end_gain = sub->gain_db.data.start;
          else
            pblock->end_gain = sub->gain_db.data.end;

        } else if (iamf_parameter_type_is_polar(psb->type)) {
          polars_parameter_block_t *pblock =
              def_polars_parameter_block_ptr(block);
          polars_parameter_subblock_t *sub =
              def_polars_parameter_subblock_ptr(psb);

          for (int j = 0; j < pblock->num_polars; ++j) {
            if (sub->animation_type == ck_iamf_animation_type_inter_linear ||
                sub->animation_type == ck_iamf_animation_type_inter_bezier) {
              sub->polars[j].azimuth.start = pblock->end_polars[j].azimuth;
              sub->polars[j].elevation.start = pblock->end_polars[j].elevation;
              sub->polars[j].distance.start = pblock->end_polars[j].distance;
              if (sub->animation_type == ck_iamf_animation_type_inter_linear) {
                sub->polars[j].animation_type = ck_iamf_animation_type_linear;
              } else {
                sub->polars[j].animation_type = ck_iamf_animation_type_bezier;
              }
            }

            if (sub->animation_type == ck_iamf_animation_type_step) {
              pblock->end_polars[j].azimuth = sub->polars[j].azimuth.start;
              pblock->end_polars[j].elevation = sub->polars[j].elevation.start;
              pblock->end_polars[j].distance = sub->polars[j].distance.start;
            } else {
              pblock->end_polars[j].azimuth = sub->polars[j].azimuth.end;
              pblock->end_polars[j].elevation = sub->polars[j].elevation.end;
              pblock->end_polars[j].distance = sub->polars[j].distance.end;
            }
          }
        } else if (iamf_parameter_type_is_cartesian(psb->type)) {
          cartesians_parameter_block_t *pblock =
              def_cartesians_parameter_block_ptr(block);
          cartesians_parameter_subblock_t *sub =
              def_cartesians_parameter_subblock_ptr(psb);

          for (int j = 0; j < pblock->num_cartesians; ++j) {
            if (sub->animation_type == ck_iamf_animation_type_inter_linear ||
                sub->animation_type == ck_iamf_animation_type_inter_bezier) {
              sub->cartesians[j].x.start = pblock->end_cartesians[j].x;
              sub->cartesians[j].y.start = pblock->end_cartesians[j].y;
              sub->cartesians[j].z.start = pblock->end_cartesians[j].z;
              if (sub->animation_type == ck_iamf_animation_type_inter_linear) {
                sub->cartesians[j].animation_type =
                    ck_iamf_animation_type_linear;
              } else {
                sub->cartesians[j].animation_type =
                    ck_iamf_animation_type_bezier;
              }
            }

            if (sub->animation_type == ck_iamf_animation_type_step) {
              pblock->end_cartesians[j].x = sub->cartesians[j].x.start;
              pblock->end_cartesians[j].y = sub->cartesians[j].y.start;
              pblock->end_cartesians[j].z = sub->cartesians[j].z.start;
            } else {
              pblock->end_cartesians[j].x = sub->cartesians[j].x.end;
              pblock->end_cartesians[j].y = sub->cartesians[j].y.end;
              pblock->end_cartesians[j].z = sub->cartesians[j].z.end;
            }
          }
        }
      }

      if (!block->next_block_index) {
        block->next_block_index = -1;
      } else if (idx > 0) {
        block->next_block_index = idx;
      }
      debug(
          "Add parameter block obu with id %u. subblocks %u->%u, "
          "next_block_index %d",
          obu->base->parameter_id, idx, deque_length(block->subblocks),
          block->next_block_index);
    }

    return IAMF_OK;
  }

  return IAMF_ERR_BAD_ARG;
}

static int _iadb_parameter_block_manager_init(
    parameter_block_manager_t *manager) {
  manager->parameter_blocks = vector_new();
  manager->demixing_infos = vector_new();
  manager->recon_gains = vector_new();
  manager->mix_gains = vector_new();
  manager->coordinates = vector_new();
  if (!manager->parameter_blocks || !manager->demixing_infos ||
      !manager->recon_gains || !manager->mix_gains || !manager->coordinates) {
    error("Failed to allocate memory for parameter blocks.");
    return IAMF_ERR_ALLOC_FAIL;
  }
  return IAMF_OK;
}

static void _iadb_parameter_block_manager_uninit(
    parameter_block_manager_t *manager) {
  if (manager->parameter_blocks)
    vector_free(manager->parameter_blocks,
                def_default_free_ptr(_iadb_parameter_block_free));
  if (manager->demixing_infos) vector_free(manager->demixing_infos, 0);
  if (manager->recon_gains) vector_free(manager->recon_gains, 0);
  if (manager->mix_gains) vector_free(manager->mix_gains, 0);
  if (manager->coordinates) vector_free(manager->coordinates, 0);
  memset(manager, 0, sizeof(parameter_block_manager_t));
}

static int _iadb_parameter_block_manager_time_elapse(
    parameter_block_manager_t *pbm, fraction_t time) {
  parameter_subblock_t *psb = 0;
  int n = vector_size(pbm->parameter_blocks);

  for (int i = 0; i < n; ++i) {
    parameter_block_t *block = def_value_wrap_type_ptr(
        parameter_block_t, vector_at(pbm->parameter_blocks, i));

    debug("S: pid %u, duration %u, elapsed %d, rate %u, elapse %u/%u",
          block->base->parameter_id, block->duration, block->elapsed,
          block->base->parameter_rate, time.numerator, time.denominator);

    if (deque_length(block->subblocks) > 0) {
      if (block->elapsed < 0) {
        // the elapsed time returns to the normal value
        block->elapsed = 0 - block->elapsed;
      } else {
        block->elapsed +=
            iamf_fraction_transform(time, block->base->parameter_rate);

        // Calculate the number of subblocks covered by elapsed
        int elapsed_duration = block->elapsed;
        int elapsed_subblock_count = 0;

        for (int j = 0; j < deque_length(block->subblocks); ++j) {
          psb = def_parameter_subblock_ptr(
              def_value_wrap_optional_ptr(deque_at(block->subblocks, j)));
          if (psb && elapsed_duration > 0 &&
              elapsed_duration >= psb->subblock_duration) {
            elapsed_duration -= psb->subblock_duration;
            elapsed_subblock_count++;
          } else {
            break;
          }
        }

        // Check if subblocks before next_block_index need to be deleted
        if (block->next_block_index > 0 &&
            elapsed_subblock_count >= block->next_block_index) {
          // Delete subblocks before next_block_index
          for (int k = 0; k < block->next_block_index; ++k) {
            value_wrap_t v;
            psb = def_parameter_subblock_ptr(
                def_value_wrap_optional_ptr(deque_at(block->subblocks, 0)));
            if (psb) {
              block->duration -= psb->subblock_duration;
              deque_pop_front(block->subblocks, &v);
              iamf_parameter_subblock_free(psb);
            }
          }
          block->elapsed = elapsed_duration;
          // Set next_block_index to -1
          block->next_block_index = -1;
        } else if (block->next_block_index == -1 && block->elapsed > 0 &&
                   block->elapsed >= block->duration) {
          // Delete all remaining subblocks
          while (deque_length(block->subblocks) > 0) {
            value_wrap_t v;
            psb = def_parameter_subblock_ptr(
                def_value_wrap_optional_ptr(deque_at(block->subblocks, 0)));
            if (psb) {
              deque_pop_front(block->subblocks, &v);
              iamf_parameter_subblock_free(psb);
            }
          }

          // The elapsed time is a negative value, which means that there is a
          // gaps in the parameters. When new parameters are added, the elapsed
          // time will return to the normal value.
          // By default, the elapsed time is constant in the process of 1~n-1
          // times.
          block->elapsed = block->duration - block->elapsed;
          block->duration = 0;
        }
      }
    } else {
      block->reset_start_time = 1;
    }

    debug("E: pid %u, duration %u, elapsed %d", block->base->parameter_id,
          block->duration, block->elapsed,
          block->elapsed < 0 || block->elapsed > block->duration
              ? "."
              : ", in gaps.");
  }
  return IAMF_OK;
}

static int _iadb_mix_presentation_obu_find(value_wrap_t item,
                                           value_wrap_t key) {
  return def_value_wrap_type_ptr(iamf_mix_presentation_obu_t, &item)
             ->mix_presentation_id == key.u32;
}
static int _iadb_audio_element_obu_find(value_wrap_t item, value_wrap_t key) {
  return def_value_wrap_type_ptr(iamf_audio_element_obu_t, &item)
             ->audio_element_id == key.u32;
}

static int _iadb_descriptor_init(iamf_database_t *database) {
  database->descriptors.codec_config_obus = vector_new();
  database->descriptors.audio_element_obus = vector_new();
  database->descriptors.mix_presentation_obus = vector_new();

  database->codec_configs = vector_new();
  database->audio_elements = vector_new();

  if (!database->descriptors.codec_config_obus ||
      !database->descriptors.audio_element_obus ||
      !database->descriptors.mix_presentation_obus ||
      !database->codec_configs || !database->audio_elements) {
    def_err_msg_enomem("descriptors", "Database");
    return IAMF_ERR_ALLOC_FAIL;
  }
  return IAMF_OK;
}

static void _iadb_descriptor_uninit(iamf_database_t *database) {
  if (database->descriptors.ia_sequence_header_obu)
    iamf_sequence_header_obu_free(database->descriptors.ia_sequence_header_obu);
  if (database->descriptors.codec_config_obus)
    vector_free(database->descriptors.codec_config_obus,
                def_default_free_ptr(iamf_codec_config_obu_free));
  if (database->descriptors.audio_element_obus)
    vector_free(database->descriptors.audio_element_obus,
                def_default_free_ptr(iamf_audio_element_obu_free));
  if (database->descriptors.mix_presentation_obus)
    vector_free(database->descriptors.mix_presentation_obus,
                def_default_free_ptr(iamf_mix_presentation_obu_free));
  memset(&database->descriptors, 0, sizeof(database->descriptors));

  if (database->codec_configs)
    vector_free(database->codec_configs, def_default_free_ptr(free));
  if (database->audio_elements)
    vector_free(database->audio_elements, def_default_free_ptr(free));
  debug("uninit descriptors obus.");
}

static int _iadb_descriptors_codec_config_obu_check_profile(
    iamf_descriptors_t *descriptors, iamf_codec_config_obu_t *cco,
    iamf_profile_t profile) {
  int ret = def_pass;
  switch (profile) {
    case ck_iamf_profile_base_advanced:
    case ck_iamf_profile_advanced_1:
    case ck_iamf_profile_advanced_2: {
      int n = vector_size(descriptors->codec_config_obus);
      trace(
          "Codec config count %d, profile %s, lpcm codec count %d, codec type "
          "%s(0x%08x)",
          n, iamf_profile_type_string(profile), descriptors->num_lpcm_codec,
          iamf_codec_type_string(iamf_codec_type_get(cco->codec_id)),
          cco->codec_id);
      if ((n > def_max_codec_configs) ||
          (n && !descriptors->num_lpcm_codec &&
           cco->codec_id != ck_iamf_codec_id_lpcm)) {
        warning("Too many codec configs or no LPCM codec config.");
        ret = def_error;
      }

      // TODO: The frame sizes and the sample rates identified (implicitly or
      // explicitly) by the two Codec Config OBUs SHALL be the same.
    } break;
    default:
      break;
  }
  return ret;
}

static int _iadb_descriptors_mix_presentation_obu_check_profile(
    iamf_descriptors_t *descriptors, iamf_mix_presentation_obu_t *mpo,
    iamf_profile_t profile) {
  int ret = def_pass;

  // TODO: If num_sub_mixes = 1 in all Mix Presentation OBUs, there SHALL be
  // only one unique Codec Config OBU.
  // TODO: Every Audio Substreams used in the first sub-mix of all Mix
  // Presentation OBUs SHALL be coded using the same Codec Config OBU.

  switch (profile) {
    case ck_iamf_profile_base_advanced: {
      uint32_t element_flags = 0;
      uint32_t k = array_size(mpo->sub_mixes);
      uint32_t m = vector_size(descriptors->mix_presentation_obus);

      for (int s = 0; s < k; ++s) {
        obu_sub_mix_t *sub =
            def_value_wrap_type_ptr(obu_sub_mix_t, array_at(mpo->sub_mixes, s));
        int n = array_size(sub->audio_element_configs);

        for (int i = 0; i < n; ++i) {
          value_wrap_t v;
          obu_audio_element_config_t *aec =
              def_value_wrap_type_ptr(obu_audio_element_config_t,
                                      array_at(sub->audio_element_configs, i));

          int idx = vector_find(descriptors->audio_element_obus,
                                def_value_wrap_instance_u32(aec->element_id),
                                _iadb_audio_element_obu_find, &v);
          iamf_audio_element_obu_t *aeo = def_value_wrap_type_ptr(
              iamf_audio_element_obu_t,
              vector_at(descriptors->audio_element_obus, idx));
          if (aeo->audio_element_type == ck_audio_element_type_object_based)
            element_flags |= 1;
          else
            element_flags |= 2;
        }
      }

      if (!m && element_flags == 3) {
        error(
            "Object-based can't with channel-based or secen-based elements in "
            "Mix Presentation OBU %u.",
            mpo->mix_presentation_id);
        ret = def_error;
      } else if (m && element_flags & 1) {
        error("Mix Presentation OBU %u with object-based must be first.",
              mpo->mix_presentation_id);
        ret = def_error;
      }

      if (ret != def_pass)
        warning(
            "Mix Presentation OBU %u, element flags 0x%x(b01-obj, b10-chn), "
            "mix count %u",
            mpo->mix_presentation_id, element_flags, m);

    } break;
    default:
      break;
  }
  return ret;
}

static int _iadb_descriptor_obu_check_profile(iamf_descriptors_t *descriptors,
                                              iamf_obu_t *obu,
                                              iamf_profile_t profile) {
  int ret = def_pass;
  switch (obu->obu_type) {
    case ck_iamf_obu_codec_config: {
      iamf_codec_config_obu_t *cco = def_codec_config_obu_ptr(obu);
      ret = _iadb_descriptors_codec_config_obu_check_profile(descriptors, cco,
                                                             profile);
    } break;
    case ck_iamf_obu_audio_element: {
      iamf_audio_element_obu_t *aeo = def_audio_element_obu_ptr(obu);
      ret = iamf_audio_element_obu_check_profile(aeo, profile);
    } break;

    case ck_iamf_obu_mix_presentation: {
      iamf_mix_presentation_obu_t *mpo = def_mix_presentation_obu_ptr(obu);
      ret = iamf_mix_presentation_obu_check_profile(
          mpo, descriptors->audio_element_obus, profile);
      if (ret == def_pass)
        ret = _iadb_descriptors_mix_presentation_obu_check_profile(
            descriptors, mpo, profile);

    } break;

    default:
      break;
  }

  return ret;
}

static codec_config_t *_iadb_codec_config_new(iamf_codec_config_obu_t *obu) {
  codec_config_t *cc = def_mallocz(codec_config_t, 1);
  if (!cc) return 0;

  cc->id = obu->codec_config_id;
  cc->codec_config_obu = obu;
  iamf_codec_config_obu_get_parameter(obu, &cc->codec_param);
  return cc;
}

#define _iadb_codec_config_free free

static int _iadb_codec_config_obu_add(iamf_database_t *database,
                                      iamf_codec_config_obu_t *obu) {
  int ret = IAMF_OK;
  codec_config_t *cc = _iadb_codec_config_new(obu);
  if (!cc) return IAMF_ERR_ALLOC_FAIL;

  vector_push(database->codec_configs, def_value_wrap_instance_ptr(cc));
  if (vector_push(database->descriptors.codec_config_obus,
                  def_value_wrap_instance_ptr(obu)) < 0) {
    ret = IAMF_ERR_ALLOC_FAIL;
  }

  if (obu->codec_id == ck_iamf_codec_id_lpcm)
    ++database->descriptors.num_lpcm_codec;

  return ret;
}

int _iadb_codec_config_find(value_wrap_t item, value_wrap_t key) {
  return def_value_wrap_type_ptr(codec_config_t, &item)->id == key.u32;
}

int _iadb_audio_element_find(value_wrap_t item, value_wrap_t key) {
  return def_value_wrap_type_ptr(audio_element_t, &item)->id == key.u32;
}

static int _iadb_audio_element_obu_add(iamf_database_t *database,
                                       iamf_audio_element_obu_t *obu) {
  int ret = IAMF_OK;
  audio_element_t *ae = 0;
  codec_config_t *cc = 0;
  parameter_base_t *pb = 0;
  value_wrap_t v;

  if (vector_find(database->codec_configs,
                  def_value_wrap_instance_u32(obu->codec_config_id),
                  _iadb_codec_config_find, &v) < 0) {
    warning("Can not find codec config with id %u for audio element %u",
            obu->codec_config_id, obu->audio_element_id);
    return IAMF_ERR_BAD_ARG;
  }
  cc = v.ptr;

  if (vector_find(database->audio_elements,
                  def_value_wrap_instance_u32(obu->audio_element_id),
                  _iadb_audio_element_find, &v) >= 0) {
    debug("audio element %u is already in database.", obu->audio_element_id);
    return IAMF_OK;
  }

  ae = def_mallocz(audio_element_t, 1);
  if (!ae) return IAMF_ERR_ALLOC_FAIL;

  ae->id = obu->audio_element_id;
  ae->codec_config = cc;
  ae->audio_element_obu = obu;
  pb = iamf_audio_element_obu_get_parameter(obu,
                                            ck_iamf_parameter_type_demixing);
  ae->demixing_info_id = pb ? pb->parameter_id : def_i32_id_none;
  if (pb)
    _iadb_parameter_block_manager_add(&database->parameter_block_manager, pb);
  pb = iamf_audio_element_obu_get_parameter(obu,
                                            ck_iamf_parameter_type_recon_gain);
  ae->recon_gain_id = pb ? pb->parameter_id : def_i32_id_none;
  if (pb)
    _iadb_parameter_block_manager_add(&database->parameter_block_manager, pb);

  vector_push(database->descriptors.audio_element_obus,
              def_value_wrap_instance_ptr(obu));
  vector_push(database->audio_elements, def_value_wrap_instance_ptr(ae));

  if (obu->audio_element_type == ck_audio_element_type_object_based)
    ++database->descriptors.num_object_elements;

  return ret;
}

static int _iadb_mix_presentation_obu_add(iamf_database_t *database,
                                          iamf_mix_presentation_obu_t *obu) {
  parameter_block_manager_t *pm = &database->parameter_block_manager;
  uint32_t k = array_size(obu->sub_mixes);
  for (int s = 0; s < k; ++s) {
    obu_sub_mix_t *sub =
        def_value_wrap_type_ptr(obu_sub_mix_t, array_at(obu->sub_mixes, s));
    int n = array_size(sub->audio_element_configs);

    for (int i = 0; i < n; ++i) {
      obu_audio_element_config_t *aec = def_value_wrap_type_ptr(
          obu_audio_element_config_t, array_at(sub->audio_element_configs, i));
      int m = array_size(aec->rendering_config.parameters);
      for (int j = 0; j < m; ++j) {
        parameter_base_t *pb = def_param_base_ptr(
            def_value_wrap_ptr(array_at(aec->rendering_config.parameters, j)));
        if (pb) _iadb_parameter_block_manager_add(pm, pb);
      }
      _iadb_parameter_block_manager_add(
          pm, def_param_base_ptr(&aec->element_mix_gain));
    }
    _iadb_parameter_block_manager_add(
        pm, def_param_base_ptr(&sub->output_mix_gain));

    int loudness_count = array_size(sub->loudness);
    for (int i = 0; i < loudness_count; ++i) {
      obu_loudness_info_t *loudness_info = def_value_wrap_type_ptr(
          obu_loudness_info_t, array_at(sub->loudness, i));

      if (loudness_info->info_type & def_loudness_info_type_momentary) {
        _iadb_parameter_block_manager_add(
            pm,
            def_param_base_ptr(
                &loudness_info->momentary_loudness.momentary_loudness_param));
      }
    }
  }

  vector_push(database->descriptors.mix_presentation_obus,
              def_value_wrap_instance_ptr(obu));

  return IAMF_OK;
}

static int _iadb_parameter_block_obu_add(iamf_database_t *database,
                                         iamf_parameter_block_obu_t *obu) {
  parameter_block_manager_t *pm = &database->parameter_block_manager;
  return _iadb_parameter_block_manager_add_obu(pm, obu);
}

static int _iadb_audio_element_find_with_pid(value_wrap_t item,
                                             value_wrap_t key) {
  audio_element_t *ae = def_value_wrap_type_ptr(audio_element_t, &item);
  return ae->demixing_info_id == key.u32 || ae->recon_gain_id == key.u32;
}

static int _iadb_audio_element_config_find(value_wrap_t item,
                                           value_wrap_t key) {
  return def_value_wrap_type_ptr(obu_audio_element_config_t, &item)
             ->element_id == key.u32;
}

static parameter_subblock_t *_iadb_parameter_block_get_subblock(
    iamf_database_t *database, uint32_t pid, uint32_t offset) {
  parameter_block_t *pbk = iamf_database_get_parameter_block(database, pid);
  parameter_subblock_t *subblock = 0, *s = 0;
  uint32_t start;
  int count;

  if (!pbk) return 0;

  start = pbk->elapsed + offset;
  count = deque_length(pbk->subblocks);

  for (int i = 0; i < count; ++i) {
    s = def_parameter_subblock_ptr(
        def_value_wrap_optional_ptr(deque_at(pbk->subblocks, i)));
    if (start < s->subblock_duration) {
      subblock = s;
      break;
    } else {
      start -= s->subblock_duration;
    }
  }
  return subblock;
}

int iamf_database_init(iamf_database_t *database) {
  memset(database, 0, sizeof(iamf_database_t));

  if (_iadb_descriptor_init(database) != IAMF_OK ||
      _iadb_parameter_block_manager_init(&database->parameter_block_manager) !=
          IAMF_OK) {
    iamf_database_uninit(database);
    return IAMF_ERR_ALLOC_FAIL;
  }

  return IAMF_OK;
}

void iamf_database_uninit(iamf_database_t *database) {
  trace("uninit database...");
  if (!database) return;

  _iadb_descriptor_uninit(database);
  _iadb_parameter_block_manager_uninit(&database->parameter_block_manager);
  memset(database, 0, sizeof(iamf_database_t));
}

int iamf_database_reset_descriptors(iamf_database_t *database) {
  iamf_profile_t profile = database->profile;
  iamf_database_uninit(database);
  iamf_database_init(database);
  database->profile = profile;
  return IAMF_OK;
}

int iamf_database_set_profile(iamf_database_t *database,
                              iamf_profile_t profile) {
  if (profile == ck_iamf_profile_none || def_iamf_profile_count <= profile)
    return IAMF_ERR_BAD_ARG;
  database->profile = profile;
  return IAMF_OK;
}

iamf_profile_t iamf_database_get_profile(iamf_database_t *database) {
  if (!database) return ck_iamf_profile_none;
  return database->profile;
}

int iamf_database_add_obu(iamf_database_t *database, iamf_obu_t *obu) {
  int ret;

  if (!obu) return IAMF_ERR_BAD_ARG;

  if (database->descriptors.state == ck_descriptors_state_none) {
    debug("Processing OBU type: %s", iamf_obu_type_string(obu->obu_type));
    if (obu->obu_type != ck_iamf_obu_sequence_header) {
      iamf_obu_free(obu);
      return IAMF_ERR_INVALID_STATE;
    } else {
      info("Get IA Sequence Header OBU.");
    }
  }

  ret = _iadb_descriptor_obu_check_profile(&database->descriptors, obu,
                                           database->profile);

  if (ret < def_pass) {
    debug("Profile check failed for OBU type: %s",
          iamf_obu_type_string(obu->obu_type));
    iamf_obu_free(obu);
    return ret == def_error ? IAMF_ERR_INTERNAL : IAMF_ERR_INVALID_PACKET;
  }

  ret = IAMF_ERR_INTERNAL;

  switch (obu->obu_type) {
    case ck_iamf_obu_sequence_header: {
      iamf_sequence_header_obu_t *sho = def_iamf_sequence_header_obu_ptr(obu);

      if (sho->primary_profile > database->profile) {
        error("Not support profile %u ia sequence.", sho->primary_profile);
        ret = IAMF_ERR_UNIMPLEMENTED;
        break;
      }

      if (database->profile > sho->additional_profile)
        database->profile = sho->additional_profile;

      if (database->descriptors.ia_sequence_header_obu) {
        warning("WARNING : Receive IAMF Sequence Header OBU again !!!");
        iamf_sequence_header_obu_free(
            database->descriptors.ia_sequence_header_obu);
      }

      database->descriptors.ia_sequence_header_obu = sho;
      database->descriptors.state = ck_descriptors_state_processing;
      ret = IAMF_OK;
      break;
    }
    case ck_iamf_obu_codec_config:
      ret = _iadb_codec_config_obu_add(database, def_codec_config_obu_ptr(obu));
      break;
    case ck_iamf_obu_audio_element:
      ret =
          _iadb_audio_element_obu_add(database, def_audio_element_obu_ptr(obu));
      break;
    case ck_iamf_obu_mix_presentation:
      ret = _iadb_mix_presentation_obu_add(database,
                                           def_mix_presentation_obu_ptr(obu));
      break;
    case ck_iamf_obu_parameter_block:
      ret = _iadb_parameter_block_obu_add(database,
                                          def_parameter_block_obu_ptr(obu));
      iamf_obu_free(obu);
      obu = 0;
      break;
    default:
      debug("Iamf %s Obu(%d) is not needed in database.",
            iamf_obu_type_string(obu->obu_type), obu->obu_type);
      ret = IAMF_ERR_UNIMPLEMENTED;
  }

  if (ret < 0 && obu) iamf_obu_free(obu);
  return ret;
}

audio_element_t *iamf_database_get_audio_element(iamf_database_t *database,
                                                 uint32_t id) {
  value_wrap_t v;
  if (vector_find(database->audio_elements, def_value_wrap_instance_u32(id),
                  _iadb_audio_element_find, &v) >= 0)
    return def_value_wrap_type_ptr(audio_element_t, &v);
  return 0;
}

iamf_codec_config_obu_t *iamf_database_get_codec_config_obu(
    iamf_database_t *database, uint32_t id) {
  value_wrap_t v;
  if (vector_find(database->codec_configs, def_value_wrap_instance_u32(id),
                  _iadb_codec_config_find, &v) >= 0) {
    codec_config_t *cc = def_value_wrap_type_ptr(codec_config_t, &v);
    return cc ? cc->codec_config_obu : 0;
  }
  return 0;
}

audio_element_t *iamf_database_get_audio_element_with_pid(
    iamf_database_t *database, uint32_t pid) {
  value_wrap_t v;
  if (vector_find(database->audio_elements, def_value_wrap_instance_u32(pid),
                  _iadb_audio_element_find_with_pid, &v) >= 0) {
    return def_value_wrap_type_ptr(audio_element_t, &v);
  }
  return 0;
}

iamf_audio_element_obu_t *iamf_database_get_audio_element_obu(
    iamf_database_t *database, uint32_t eid) {
  audio_element_t *ae = iamf_database_get_audio_element(database, eid);
  return ae ? ae->audio_element_obu : 0;
}

iamf_mix_presentation_obu_t *iamf_database_get_mix_presentation_obu(
    iamf_database_t *database, uint32_t id) {
  value_wrap_t v;

  return vector_find(database->descriptors.mix_presentation_obus,
                     def_value_wrap_instance_u32(id),
                     _iadb_mix_presentation_obu_find, &v) < 0
             ? 0
             : def_value_wrap_type_ptr(iamf_mix_presentation_obu_t, &v);
}

iamf_mix_presentation_obu_t *iamf_database_get_mix_presentation_obu_default(
    iamf_database_t *database) {
  if (!database) return 0;

  int n = vector_size(database->descriptors.mix_presentation_obus);
  if (n <= 0) return 0;

  // Return the first mix presentation OBU in the database
  return def_value_wrap_type_ptr(
      iamf_mix_presentation_obu_t,
      vector_at(database->descriptors.mix_presentation_obus, 0));
}

iamf_mix_presentation_obu_t *iamf_database_find_mix_presentation_obu(
    iamf_database_t *database, func_mix_presentation_obu_check_t check_func,
    iamf_layout_t target_layout) {
  if (!database || !check_func) return 0;

  int n = vector_size(database->descriptors.mix_presentation_obus);

  for (int i = 0; i < n; ++i) {
    iamf_mix_presentation_obu_t *obu = def_value_wrap_type_ptr(
        iamf_mix_presentation_obu_t,
        vector_at(database->descriptors.mix_presentation_obus, i));

    if (obu && check_func(obu, target_layout, database)) return obu;
  }
  return 0;
}

iamf_mix_presentation_obu_t *
iamf_database_find_mix_presentation_obu_with_highest_layout(
    iamf_database_t *database, iamf_layout_t reference_layout) {
  iamf_mix_presentation_obu_t *selected_mpo = 0;
  iamf_layout_t highest_layout =
      def_sound_system_layout_instance(SOUND_SYSTEM_NONE);

  int n = vector_size(database->descriptors.mix_presentation_obus);

  for (int i = 0; i < n; ++i) {
    iamf_mix_presentation_obu_t *obu = def_value_wrap_type_ptr(
        iamf_mix_presentation_obu_t,
        vector_at(database->descriptors.mix_presentation_obus, i));

    if (obu && array_size(obu->sub_mixes) > 0) {
      // Traverse all sub_mixes in the mix presentation
      int m = array_size(obu->sub_mixes);
      for (int j = 0; j < m; ++j) {
        obu_sub_mix_t *sub =
            def_value_wrap_optional_ptr(array_at(obu->sub_mixes, j));

        if (sub && array_size(sub->loudness_layouts) > 0) {
          int layout_count = array_size(sub->loudness_layouts);
          for (int k = 0; k < layout_count; ++k) {
            iamf_layout_t *layout =
                def_value_wrap_optional_ptr(array_at(sub->loudness_layouts, k));
            if (!layout) continue;

            if ((iamf_layout_higher_check(*layout, reference_layout, 0) ||
                 (iamf_layout_is_equal(
                      highest_layout,
                      def_sound_system_layout_instance(SOUND_SYSTEM_NONE)) &&
                  iamf_layout_higher_check(*layout, reference_layout, 1))) &&
                iamf_layout_higher_check(*layout, highest_layout, 0)) {
              highest_layout = *layout;
              selected_mpo = obu;
              debug("Selected MPO: %u, Layout: %s", obu->mix_presentation_id,
                    iamf_layout_string(highest_layout));
            }
          }
        }
      }
    }
  }

  return selected_mpo;
}

iamf_audio_element_obu_t *iamf_database_get_audio_element_obu_with_pid(
    iamf_database_t *database, uint32_t pid) {
  audio_element_t *ae = iamf_database_get_audio_element_with_pid(database, pid);
  return ae ? ae->audio_element_obu : 0;
}

parameter_base_t *iamf_database_get_parameter_base(iamf_database_t *database,
                                                   uint32_t pid) {
  parameter_block_t *pbk = iamf_database_get_parameter_block(database, pid);
  return pbk ? pbk->base : 0;
}

parameter_block_t *iamf_database_get_parameter_block(iamf_database_t *database,
                                                     uint32_t parameter_id) {
  value_wrap_t v;
  return vector_find(database->parameter_block_manager.parameter_blocks,
                     def_value_wrap_instance_u32(parameter_id),
                     _iadb_parameter_block_find, &v) >= 0
             ? def_value_wrap_type_ptr(parameter_block_t, &v)
             : 0;
}

int iamf_database_enable_parameter_block(iamf_database_t *database,
                                         uint32_t pid) {
  value_wrap_t v;
  if (vector_find(database->parameter_block_manager.parameter_blocks,
                  def_value_wrap_instance_u32(pid), _iadb_parameter_block_find,
                  &v) >= 0) {
    parameter_block_t *pbk = def_value_wrap_type_ptr(parameter_block_t, &v);
    pbk->enabled = 1;
    debug("enable parameter block with id %u", pid);
    return 1;
  }
  return 0;
}

int iamf_database_enable_mix_presentation_parameter_blocks(
    iamf_database_t *database, uint32_t pid) {
  iamf_mix_presentation_obu_t *mpo =
      iamf_database_get_mix_presentation_obu(database, pid);
  int num_sub_mixes = 0;
  if (!mpo) return IAMF_ERR_BAD_ARG;

  num_sub_mixes = array_size(mpo->sub_mixes);

  for (int i = 0; i < num_sub_mixes; i++) {
    obu_sub_mix_t *sub =
        def_value_wrap_optional_ptr(array_at(mpo->sub_mixes, i));
    int num_configs = 0;
    if (!sub) continue;

    num_configs = array_size(sub->audio_element_configs);
    for (int j = 0; j < num_configs; j++) {
      obu_audio_element_config_t *config =
          def_value_wrap_optional_ptr(array_at(sub->audio_element_configs, j));
      audio_element_t *ae = 0;
      if (!config) continue;

      ae = iamf_database_get_audio_element(database, config->element_id);
      if (ae->demixing_info_id != def_i32_id_none)
        iamf_database_enable_parameter_block(database, ae->demixing_info_id);
      if (ae->recon_gain_id != def_i32_id_none)
        iamf_database_enable_parameter_block(database, ae->recon_gain_id);

      iamf_database_enable_parameter_block(
          database, config->element_mix_gain.base.parameter_id);
    }
    iamf_database_enable_parameter_block(
        database, sub->output_mix_gain.base.parameter_id);
  }

  return IAMF_OK;
}

int iamf_database_disable_parameter_blocks(iamf_database_t *database) {
  int n = vector_size(database->parameter_block_manager.parameter_blocks);
  for (int i = 0; i < n; ++i) {
    parameter_block_t *pbk = def_value_wrap_type_ptr(
        parameter_block_t,
        vector_at(database->parameter_block_manager.parameter_blocks, i));
    if (pbk) pbk->enabled = 0;
  }
  return IAMF_OK;
}

int iamf_database_disabled_parameter_blocks_clear(iamf_database_t *database) {
  int n = vector_size(database->parameter_block_manager.parameter_blocks);
  for (int i = 0; i < n; ++i) {
    parameter_block_t *pbk = def_value_wrap_type_ptr(
        parameter_block_t,
        vector_at(database->parameter_block_manager.parameter_blocks, i));

    if (pbk && !pbk->enabled && pbk->subblocks) {
      deque_free(pbk->subblocks,
                 def_default_free_ptr(iamf_parameter_subblock_free));
      pbk->subblocks = deque_new();
      pbk->duration = 0;
      pbk->elapsed = 0;
    }
  }
  return IAMF_OK;
}

int iamf_database_get_demix_mode(iamf_database_t *database, uint32_t pid,
                                 uint32_t offset) {
  demixing_info_parameter_subblock_t *subblock =
      (demixing_info_parameter_subblock_t *)_iadb_parameter_block_get_subblock(
          database, pid, offset);
  return subblock ? subblock->demixing_mode : def_dmx_mode_none;
}

array_t *iamf_database_get_recon_gain_present_flags(iamf_database_t *database,
                                                    uint32_t pid) {
  array_t *flags = 0;
  audio_element_t *ae = iamf_database_get_audio_element_with_pid(database, pid);
  if (!ae) return 0;

  if (ae->audio_element_obu->audio_element_type ==
      ck_audio_element_type_channel_based) {
    channel_based_audio_element_obu_t *cae =
        def_channel_based_audio_element_obu_ptr(ae->audio_element_obu);
    int n = array_size(cae->channel_audio_layer_configs);
    flags = array_new(n);
    if (!flags) return 0;
    for (int i = 0; i < n; ++i) {
      value_wrap_t *v;
      obu_channel_layer_config_t *clc = def_value_wrap_optional_ptr(
          array_at(cae->channel_audio_layer_configs, i));
      v = array_at(flags, i);
      if (v) {
        v->u32 = clc->recon_gain_is_present_flag;
      } else {
        continue;
      }
    }
  }

  return flags;
}

int iamf_database_get_subblocks(iamf_database_t *database, uint32_t pid,
                                fraction_t number,
                                parameter_subblock_t ***subblocks) {
  parameter_block_t *pbk = iamf_database_get_parameter_block(database, pid);
  uint32_t requested_count = number.numerator;
  uint32_t requested_rate = number.denominator;
  parameter_subblock_t *subblock = 0;
  int count = 0;

  if (!pbk || !subblocks || !deque_length(pbk->subblocks))
    return IAMF_ERR_BAD_ARG;

  uint32_t converted_count;
  if (requested_rate == pbk->base->parameter_rate) {
    converted_count = requested_count;
  } else {
    converted_count =
        iamf_fraction_transform(number, pbk->base->parameter_rate);
  }

  debug(
      "pid %u, converted_count %u, elapsed %d, duration %u, next_block_index "
      "%d",
      pid, converted_count, pbk->elapsed, pbk->duration, pbk->next_block_index);

  // for gaps in delay case.
  if (pbk->elapsed > 0 && pbk->elapsed + converted_count > pbk->duration) {
    warning("required subblocks exceed the duration of parameter block %u.",
            pid);
    return IAMF_ERR_BUFFER_TOO_SMALL;
  }

  if (pbk->elapsed <= 0) {
    if (!pbk->next_block_index)
      return IAMF_ERR_BAD_ARG;
    else if (pbk->next_block_index == -1) {
      count = deque_length(pbk->subblocks);
      *subblocks = def_mallocz(parameter_subblock_t *, count);
      if (!*subblocks) return IAMF_ERR_ALLOC_FAIL;
      for (int i = 0; i < count; ++i) {
        subblock = (parameter_subblock_t *)def_value_wrap_optional_ptr(
            deque_at(pbk->subblocks, i));
        if (subblock) (*subblocks)[i] = subblock;
      }
      return count;
    }
  } else {
    uint32_t next_block_duration = 0;
    if (pbk->next_block_index > 0) {
      for (int i = 0;
           i < pbk->next_block_index && i < deque_length(pbk->subblocks); ++i) {
        subblock = (parameter_subblock_t *)def_value_wrap_optional_ptr(
            deque_at(pbk->subblocks, i));
        next_block_duration += subblock->subblock_duration;
      }
    }

    if (pbk->elapsed + converted_count > next_block_duration &&
        pbk->next_block_index > 0) {
      count = deque_length(pbk->subblocks) - pbk->next_block_index;
      if (count > 0) {
        *subblocks = def_mallocz(parameter_subblock_t *, count);
        if (!*subblocks) return IAMF_ERR_ALLOC_FAIL;
        for (int i = pbk->next_block_index; i < deque_length(pbk->subblocks);
             ++i) {
          subblock = (parameter_subblock_t *)def_value_wrap_optional_ptr(
              deque_at(pbk->subblocks, i));
          if (subblock) (*subblocks)[i - pbk->next_block_index] = subblock;
        }
        return count;
      }
    }
  }

  *subblocks = 0;
  debug("No subblocks found for pid %u", pid);
  return 0;
}

uint32_t iamf_database_get_parameter_rate(iamf_database_t *database,
                                          uint32_t pid) {
  parameter_block_t *pbk = iamf_database_get_parameter_block(database, pid);
  return pbk ? pbk->base->parameter_rate : 0;
}

int iamf_database_time_elapse(iamf_database_t *database, fraction_t time) {
  return _iadb_parameter_block_manager_time_elapse(
      &database->parameter_block_manager, time);
}

int iamf_database_get_audio_element_default_demixing_info(
    iamf_database_t *database, uint32_t eid, int *mode, int *weight_index) {
  audio_element_t *ae = iamf_database_get_audio_element(database, eid);
  if (!ae || ae->demixing_info_id == def_i32_id_none) return IAMF_ERR_BAD_ARG;
  parameter_base_t *base =
      iamf_database_get_parameter_base(database, ae->demixing_info_id);
  if (!base) return IAMF_ERR_BAD_ARG;
  demixing_info_parameter_base_t *dmx = def_demixing_info_param_base_ptr(base);
  *mode = dmx->dmixp_mode;
  *weight_index = dmx->default_w;

  return IAMF_OK;
}

int iamf_database_get_audio_element_demix_mode(iamf_database_t *database,
                                               uint32_t eid,
                                               fraction_t offset) {
  audio_element_t *ae = iamf_database_get_audio_element(database, eid);
  if (!ae || ae->demixing_info_id == def_i32_id_none) {
    return def_dmx_mode_none;
  }

  // Convert fraction_t offset to uint32_t for the existing function
  uint32_t offset_samples = iamf_fraction_transform(offset, 1);

  return iamf_database_get_demix_mode(database, ae->demixing_info_id,
                                      offset_samples);
}

descriptors_state_t iamf_database_descriptors_get_state(
    iamf_database_t *database) {
  return database->descriptors.state;
}

int iamf_database_descriptors_complete(iamf_database_t *database) {
  if (database->descriptors.state == ck_descriptors_state_processing &&
      (vector_size(database->descriptors.codec_config_obus) &&
       vector_size(database->descriptors.audio_element_obus) &&
       vector_size(database->descriptors.mix_presentation_obus))) {
    database->descriptors.state = ck_descriptors_state_completed;
  }

  return database->descriptors.state == ck_descriptors_state_completed;
}
