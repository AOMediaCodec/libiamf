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
 * @file iamf_presentation.c
 * @brief IAMF presentation implementation.
 * @version 2.0.0
 * @date Created 05/09/2025
 **/

#include "iamf_presentation.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "chashmap.h"
#include "clog.h"
#include "iamf_element_reconstructor.h"
#include "iamf_layout.h"
#include "iamf_private_definitions.h"
#include "iamf_renderer.h"
#include "iamf_string.h"
#include "iamf_synchronizer.h"
#include "iamf_types.h"
#include "obu/iamf_obu_all.h"

#undef def_log_tag
#define def_log_tag "IAMF_PST"

#define def_pst_str "IAMF Presentation"

/**
 * @brief IAMF element presentation structure
 */
typedef struct IamfPresentationElement {
  uint32_t element_id;
  uint32_t sub_mix_index;

  int sampling_rate;
  uint32_t number_samples_per_frame;
  int num_channels;

  uint32_t scheme;

  iamf_rendering_param_t rendering_config;
  iamf_layout_t out_layout;

  union {
    iamf_loudspeaker_layout_t layout;
    uint32_t position_id;
  } in;

  uint32_t mix_gain_id;

  int enable_gain_offset;
  struct {
    float offset;
    float min;
    float max;
  } gain_offset;
  uint32_t gain_offset_pid;  // Unique gain offset parameter ID generated to
                             // avoid conflicts

  iamf_audio_block_t* block;
} iamf_presentation_element_t;

/**
 * @brief IAMF presentation structure
 */
typedef struct IamfPresentation {
  uint32_t id;

  iamf_layout_t layout;

  vector_t* output_mix_gain_ids;
  uint32_t num_audio_blocks;

  iamf_database_t* database;
  iamf_element_reconstructor_t* reconstructor;
  hash_map_t* elements;  // iamf_presentation_element_t
  iamf_synchronizer_t* synchronizer;
  iamf_renderer_t renderer;

  int out_sampling_rate;

  float loudnesses[def_max_sub_mixes];
  int num_loudnesses;
} iamf_presentation_t;

/**
 * @brief Check if a parameter ID conflicts with existing parameter IDs
 *
 * This function checks if the given pid conflicts with any existing
 * parameter IDs in the presentation, including mix_gain_id and
 * position_param_id. It prevents parameter ID conflicts when generating
 * unique gain offset parameter IDs.
 *
 * @param self Pointer to the presentation instance
 * @param element_id The element ID being checked
 * @param pid The parameter ID to test for conflicts
 * @return 1 if there is a conflict, 0 otherwise
 */
static int iamf_presentation_priv_pid_conflicts(iamf_presentation_t* self,
                                                uint32_t element_id,
                                                uint32_t pid) {
  // Check conflict with mix_gain_id for the same element
  value_wrap_t* val = hash_map_get(self->elements, element_id);
  if (val) {
    iamf_presentation_element_t* element =
        def_value_wrap_type_ptr(iamf_presentation_element_t, val);
    if (element->mix_gain_id == pid) {
      debug("Parameter ID %u conflicts with mix_gain_id for element %u", pid,
            element_id);
      return 1;
    }
  }

  // Check conflict with position_param_id across all elements
  if (self->elements) {
    hash_map_iterator_t* it = hash_map_iterator_new(self->elements);
    if (it) {
      do {
        val = hash_map_iterator_get_value(it);
        iamf_presentation_element_t* elem =
            def_value_wrap_type_ptr(iamf_presentation_element_t, val);
        if (elem->in.position_id == pid) {
          debug(
              "Parameter ID %u conflicts with position_param_id for element %u",
              pid, elem->element_id);
          hash_map_iterator_delete(it);
          return 1;
        }
      } while (!hash_map_iterator_next(it));
      hash_map_iterator_delete(it);
    }
  }

  return 0;
}

/**
 * @brief Generate a unique gain offset parameter ID
 *
 * This function generates a unique parameter ID for gain offset that doesn't
 * conflict with any existing parameter IDs. It uses two strategies:
 * 1. Try direct pid
 * 2. Try linear offset (pid + offset)
 *
 * @param self Pointer to the presentation instance
 * @param element_id The element ID
 * @return A unique parameter ID for gain offset
 */
static uint32_t iamf_presentation_priv_generate_unique_gain_offset_id(
    iamf_presentation_t* self, uint32_t element_id) {
  // First, try using pid directly
  uint32_t candidate_id = self->id;
  if (!iamf_presentation_priv_pid_conflicts(self, element_id, candidate_id)) {
    debug("Using direct id %u as gain offset parameter ID", candidate_id);
    return candidate_id;
  }

  // If conflict, try different offsets
  for (uint32_t offset = 1; offset < 0xFFFF; offset++) {
    candidate_id += offset;
    if (!iamf_presentation_priv_pid_conflicts(self, element_id, candidate_id)) {
      debug(
          "Generated unique gain offset ID: original_id=%u, unique_id=%u, "
          "offset=%u",
          self->id, candidate_id, offset);
      return candidate_id;
    }
  }

  // If all offsets conflict, use id as fallback
  warning("All offsets conflict, using pid as fallback: id=%u", self->id);
  return self->id;
}

static animated_float32_t _create_step_animated_gain(float start_value) {
  animated_float32_t animated_gain;

  memset(&animated_gain, 0, sizeof(animated_gain));
  animated_gain.animation_type = ck_iamf_animation_type_step;
  animated_gain.data.start = start_value;

  return animated_gain;
}

static animated_polar_t _create_step_animated_polar(float azimuth,
                                                    float elevation,
                                                    float distance) {
  animated_polar_t animated_polar;
  memset(&animated_polar, 0, sizeof(animated_polar));
  animated_polar.animation_type = ck_iamf_animation_type_step;
  animated_polar.azimuth =
      def_animated_data_step_instance(animated_data_float32_t, azimuth);
  animated_polar.elevation =
      def_animated_data_step_instance(animated_data_float32_t, elevation);
  animated_polar.distance =
      def_animated_data_step_instance(animated_data_float32_t, distance);
  return animated_polar;
}

static animated_cartesian_t _create_step_animated_cartesian(float x, float y,
                                                            float z) {
  animated_cartesian_t animated_cartesian;
  memset(&animated_cartesian, 0, sizeof(animated_cartesian));
  animated_cartesian.x =
      def_animated_data_step_instance(animated_data_float32_t, x);
  animated_cartesian.y =
      def_animated_data_step_instance(animated_data_float32_t, y);
  animated_cartesian.z =
      def_animated_data_step_instance(animated_data_float32_t, z);
  return animated_cartesian;
}

// Helper function to free all element presentations in a hash_map_t
static void iamf_presentation_priv_delete_all_elements(hash_map_t* elements) {
  if (!elements || !hash_map_size(elements)) return;

  hash_map_iterator_t* iter = hash_map_iterator_new(elements);
  if (iter) {
    do {
      iamf_presentation_element_t* element = def_value_wrap_type_ptr(
          iamf_presentation_element_t, hash_map_iterator_get_value(iter));
      if (element && element->block) iamf_audio_block_delete(element->block);
      def_free(element);
    } while (!hash_map_iterator_next(iter));
    hash_map_iterator_delete(iter);
  }
}

iamf_loudspeaker_layout_t
iamf_presentation_priv_channel_based_element_get_out_layout(
    iamf_presentation_t* self, channel_based_audio_element_obu_t* cbo,
    obu_audio_element_config_t* audio_config) {
  iamf_layout_t* layout = &self->layout;

  if (!cbo) return ck_iamf_loudspeaker_layout_none;
  if (cbo->max_valid_layers == 1) {
    obu_channel_layer_config_t* config =
        def_value_wrap_type_ptr(obu_channel_layer_config_t,
                                array_at(cbo->channel_audio_layer_configs, 0));
    iamf_loudspeaker_layout_t layout = iamf_audio_layer_layout_get(
        config->loudspeaker_layout, config->expanded_loudspeaker_layout);
    const iamf_layout_info_t* info = iamf_loudspeaker_layout_get_info(layout);
    return info->flags & ck_iamf_layout_flag_subset ? info->reference_layout
                                                    : info->layout;
  }

  if (layout->type == ck_iamf_layout_type_binaural) {
    int headphones_rendering_mode =
        audio_config->rendering_config.headphones_rendering_mode;
    if (headphones_rendering_mode > 0) {
      obu_channel_layer_config_t* config = def_value_wrap_type_ptr(
          obu_channel_layer_config_t, array_at(cbo->channel_audio_layer_configs,
                                               cbo->max_valid_layers - 1));
      debug("use the highest layout downmix to binaural.");
      return config->loudspeaker_layout;
    }
  }

  uint8_t out_sound_system = layout->type == ck_iamf_layout_type_binaural
                                 ? SOUND_SYSTEM_A
                                 : layout->sound_system;

  for (int i = 0; i < cbo->max_valid_layers; ++i) {
    obu_channel_layer_config_t* config =
        def_value_wrap_type_ptr(obu_channel_layer_config_t,
                                array_at(cbo->channel_audio_layer_configs, i));
    const iamf_layout_info_t* info =
        iamf_loudspeaker_layout_get_info(config->loudspeaker_layout);
    if ((info->flags & ck_iamf_layout_flag_out) &&
        info->sound_system == out_sound_system) {
      info("scalabel channels layer is %d", config->loudspeaker_layout);
      return config->loudspeaker_layout;
    }
  }

  // select next highest available layout
  int playback_channels = iamf_layout_get_info(*layout)->channels;
  for (int i = 0; i < cbo->max_valid_layers; ++i) {
    obu_channel_layer_config_t* config =
        def_value_wrap_type_ptr(obu_channel_layer_config_t,
                                array_at(cbo->channel_audio_layer_configs, i));
    int channels =
        iamf_loudspeaker_layout_get_info(config->loudspeaker_layout)->channels;
    if (channels > playback_channels) {
      info("scalabel channels layer is %d", config->loudspeaker_layout);
      return config->loudspeaker_layout;
    }
  }

  if (cbo->max_valid_layers) {
    obu_channel_layer_config_t* config = def_value_wrap_type_ptr(
        obu_channel_layer_config_t,
        array_at(cbo->channel_audio_layer_configs, cbo->max_valid_layers - 1));
    return config->loudspeaker_layout;  // use the highest layout
  }

  return ck_iamf_loudspeaker_layout_none;
}

static iamf_presentation_element_t* iamf_presentation_priv_make_element(
    iamf_presentation_t* self, uint32_t sub_mix_index,
    obu_audio_element_config_t* audio_config) {
  iamf_presentation_element_t* element = 0;
  iamf_audio_element_obu_t* aeo = 0;
  iamf_codec_config_obu_t* cco = 0;
  uint32_t eid = audio_config->element_id;
  audio_codec_parameter_t acparam = {0};

  debug("make presentation element %u", eid);
  aeo = iamf_database_get_audio_element_obu(self->database, eid);
  if (!aeo) {
    warning("no audio element found for id %u", eid);
    return 0;
  }

  cco =
      iamf_database_get_codec_config_obu(self->database, aeo->codec_config_id);
  if (!cco) {
    warning("no codec config found for id %u", aeo->codec_config_id);
    return 0;
  }

  iamf_codec_config_obu_get_parameter(cco, &acparam);

  element = def_mallocz(iamf_presentation_element_t, 1);
  if (!element) return 0;

  element->element_id = eid;
  element->sub_mix_index = sub_mix_index;
  element->scheme = aeo->audio_element_type;
  element->sampling_rate = acparam.sample_rate;
  element->rendering_config = def_rendering_param_instance(
      audio_config->rendering_config.headphones_rendering_mode,
      audio_config->rendering_config.binaural_filter_profile);
  element->number_samples_per_frame = cco->num_samples_per_frame;
  element->mix_gain_id = audio_config->element_mix_gain.base.parameter_id;

  if (audio_config->rendering_config.flags &
      def_rendering_config_flag_element_gain_offset) {
    element->enable_gain_offset = 1;
    // Initialize gain offset PID when gain offset is enabled
    element->gain_offset_pid =
        iamf_presentation_priv_generate_unique_gain_offset_id(
            self, element->element_id);
    debug(
        "Initialized gain offset PID during element creation: element_id=%u, "
        "gain_offset_pid=%u",
        element->element_id, element->gain_offset_pid);
    element->gain_offset.offset =
        audio_config->rendering_config.element_gain_offset_db.value;
    element->gain_offset.min =
        audio_config->rendering_config.element_gain_offset_db.min;
    element->gain_offset.max =
        audio_config->rendering_config.element_gain_offset_db.max;
  }

  debug("codec conf id %u, codec id 0x%x (%s), sampling rate is %u",
        cco->codec_config_id, cco->codec_id,
        iamf_codec_type_string(iamf_codec_type_get(cco->codec_id)),
        element->sampling_rate);

  if (element->scheme == ck_audio_element_type_channel_based) {
    channel_based_audio_element_obu_t* cbo =
        def_channel_based_audio_element_obu_ptr(aeo);
    element->in.layout =
        iamf_presentation_priv_channel_based_element_get_out_layout(
            self, cbo, audio_config);
    info("get decoded channel based layout %d", element->in.layout);
    const iamf_layout_info_t* info =
        iamf_loudspeaker_layout_get_info(element->in.layout);
    element->num_channels = info->channels;
  } else if (element->scheme == ck_audio_element_type_scene_based) {
    scene_based_audio_element_obu_t* sbo =
        def_scene_based_audio_element_obu_ptr(aeo);
    element->num_channels = sbo->output_channel_count;
  } else if (element->scheme == ck_audio_element_type_object_based) {
    object_based_audio_element_obu_t* obo =
        def_object_based_audio_element_obu_ptr(aeo);
    element->num_channels = obo->num_objects;

    // Get position parameter ID from rendering_config for object-based elements
    element->in.position_id = def_i32_id_none;  // Initialize to invalid value
    if (audio_config->rendering_config.parameters) {
      for (int i = 0; i < array_size(audio_config->rendering_config.parameters);
           i++) {
        parameter_base_t* param_base = def_value_wrap_type_ptr(
            parameter_base_t,
            array_at(audio_config->rendering_config.parameters, i));
        if (param_base && iamf_parameter_type_is_coordinate(param_base->type)) {
          element->in.position_id = param_base->parameter_id;
          debug(
              "Found position_id %u for object-based element %u from "
              "rendering_config",
              element->in.position_id, element->element_id);
          break;
        }
      }
    }

    if (element->in.position_id == def_i32_id_none) {
      warning(
          "No position parameter found in rendering_config for object-based "
          "element %u",
          element->element_id);
    }
  } else {
    warning("unknown scheme %d", element->scheme);
  }

  return element;
}

static int iamf_presentation_priv_make_elements(iamf_presentation_t* self) {
  iamf_mix_presentation_obu_t* mix_presentation_obu =
      iamf_database_get_mix_presentation_obu(self->database, self->id);

  if (mix_presentation_obu->sub_mixes) {
    for (size_t i = 0; i < array_size(mix_presentation_obu->sub_mixes); i++) {
      obu_sub_mix_t* sub_mix = def_value_wrap_type_ptr(
          obu_sub_mix_t, array_at(mix_presentation_obu->sub_mixes, i));
      if (sub_mix) {
        vector_push(self->output_mix_gain_ids,
                    def_value_wrap_instance_u32(
                        sub_mix->output_mix_gain.base.parameter_id));
        if (sub_mix && sub_mix->audio_element_configs) {
          for (size_t j = 0; j < array_size(sub_mix->audio_element_configs);
               j++) {
            obu_audio_element_config_t* audio_config = def_value_wrap_type_ptr(
                obu_audio_element_config_t,
                array_at(sub_mix->audio_element_configs, j));
            if (audio_config) {
              iamf_presentation_element_t* element =
                  iamf_presentation_priv_make_element(self, i, audio_config);
              if (!element ||
                  hash_map_put(self->elements, element->element_id,
                               def_value_wrap_instance_ptr(element)) < 0) {
                def_free(element);
                warning(
                    "Failed to insert element presentation for element_id: %u",
                    audio_config->element_id);
                return IAMF_ERR_INTERNAL;
              }
            }
          }
        }
      }
    }
  }

  return IAMF_OK;
}

static int iamf_presentation_priv_add_elements_to_reconstructor(
    iamf_presentation_t* self) {
  if (!self) return IAMF_ERR_BAD_ARG;
  if (!self->elements) return IAMF_ERR_INTERNAL;

  hash_map_iterator_t* iter = hash_map_iterator_new(self->elements);
  int ret = IAMF_OK;

  debug("elements size %d", hash_map_size(self->elements));
  if (iter) {
    do {
      uint32_t eid = hash_map_iterator_get_key(iter);
      debug("add element %u to reconstructor", eid);
      iamf_presentation_element_t* element = def_value_wrap_type_ptr(
          iamf_presentation_element_t, hash_map_iterator_get_value(iter));
      if (!element) {
        error("Invalid presentation element for element id: %u", eid);
        ret = IAMF_ERR_INTERNAL;
        break;
      }

      iamf_audio_element_obu_t* audio_element_obu =
          iamf_database_get_audio_element_obu(self->database, eid);
      if (!audio_element_obu) {
        error("Failed to get audio_element_obu for element id: %u", eid);
        ret = IAMF_ERR_INTERNAL;
        break;
      }

      iamf_codec_config_obu_t* codec_config_obu =
          iamf_database_get_codec_config_obu(
              self->database, audio_element_obu->codec_config_id);
      if (!codec_config_obu) {
        error(
            "Failed to get codec_config_obu for codec config id: %u (element "
            "id: %u)",
            audio_element_obu->codec_config_id, eid);
        ret = IAMF_ERR_INTERNAL;
        break;
      }

      ret = iamf_element_reconstructor_add_element(
          self->reconstructor, audio_element_obu, codec_config_obu);
      if (ret != IAMF_OK) {
        error("Failed to add reconstructor for element_id: %u", eid);
        break;
      }

      info("Successfully added reconstructor for element_id: %u", eid);
    } while (!hash_map_iterator_next(iter));
    hash_map_iterator_delete(iter);
  }

  return ret;
}

static int iamf_presentation_priv_activate_elements_and_parameters(
    iamf_presentation_t* self) {
  if (!self || !self->elements) return IAMF_ERR_BAD_ARG;
  if (!hash_map_size(self->elements)) return IAMF_ERR_INTERNAL;

  hash_map_iterator_t* iter = hash_map_iterator_new(self->elements);
  int ret = IAMF_OK;

  if (iter) {
    do {
      uint32_t eid = hash_map_iterator_get_key(iter);
      iamf_presentation_element_t* element = def_value_wrap_type_ptr(
          iamf_presentation_element_t, hash_map_iterator_get_value(iter));

      if (!element) {
        error("Invalid element self for element_id: %u", eid);
        ret = IAMF_ERR_INTERNAL;
        break;
      }

      if (element->scheme == ck_audio_element_type_channel_based)
        iamf_element_reconstructor_set_channel_based_target_layout(
            self->reconstructor, eid, element->in.layout);

      iamf_element_reconstructor_activate_element(self->reconstructor, eid);
      iamf_synchronizer_add_audio_cache(self->synchronizer, eid);

      iamf_audio_element_obu_t* aeo =
          iamf_database_get_audio_element_obu(self->database, eid);
      if (aeo) {
        int n = array_size(aeo->parameters);
        for (int i = 0; i < n; i++) {
          parameter_base_t* pb = def_value_wrap_type_ptr(
              parameter_base_t, array_at(aeo->parameters, i));
          if (pb)
            iamf_database_enable_parameter_block(self->database,
                                                 pb->parameter_id);
        }
      }

      if (element->scheme == ck_audio_element_type_object_based)
        iamf_database_enable_parameter_block(self->database,
                                             element->in.position_id);

      iamf_database_enable_parameter_block(self->database,
                                           element->mix_gain_id);
    } while (!hash_map_iterator_next(iter));

    hash_map_iterator_delete(iter);
  }

  // Enable momentary loudness parameters from mix presentation OBU
  iamf_mix_presentation_obu_t* mix_presentation_obu =
      iamf_database_get_mix_presentation_obu(self->database, self->id);
  if (mix_presentation_obu && mix_presentation_obu->sub_mixes) {
    for (size_t i = 0; i < array_size(mix_presentation_obu->sub_mixes); i++) {
      obu_sub_mix_t* sub_mix = def_value_wrap_type_ptr(
          obu_sub_mix_t, array_at(mix_presentation_obu->sub_mixes, i));
      if (sub_mix && sub_mix->loudness) {
        for (size_t j = 0; j < array_size(sub_mix->loudness); j++) {
          obu_loudness_info_t* loudness_info = def_value_wrap_type_ptr(
              obu_loudness_info_t, array_at(sub_mix->loudness, j));
          if (loudness_info &&
              (loudness_info->info_type & def_loudness_info_type_momentary)) {
            // Enable the momentary loudness parameter block
            iamf_database_enable_parameter_block(
                self->database,
                loudness_info->momentary_loudness.momentary_loudness_param.base
                    .parameter_id);
          }
        }
      }
    }
  }

  int n = vector_size(self->output_mix_gain_ids);
  for (int i = 0; i < n; i++)
    iamf_database_enable_parameter_block(
        self->database,
        def_value_wrap_u32(vector_at(self->output_mix_gain_ids, i)));

  return ret;
}

static int iamf_presentation_priv_activate_renderer(iamf_presentation_t* self) {
  hash_map_iterator_t* iter = hash_map_iterator_new(self->elements);
  if (!iter) return IAMF_ERR_INTERNAL;

  iamf_presentation_element_t* element = def_value_wrap_type_ptr(
      iamf_presentation_element_t, hash_map_iterator_get_value(iter));
  hash_map_iterator_delete(iter);

  int ret = iamf_renderer_init(&self->renderer, self->layout,
                               element->number_samples_per_frame,
                               element->sampling_rate);

  if (ret != IAMF_OK) return ret;

  iter = hash_map_iterator_new(self->elements);
  if (iter) {
    do {
      iamf_presentation_element_t* element = def_value_wrap_type_ptr(
          iamf_presentation_element_t, hash_map_iterator_get_value(iter));
      if (element->scheme == ck_audio_element_type_channel_based) {
        int dmx_default_mode = def_dmx_mode_none;
        int dmx_default_w_idx = def_dmx_weight_index_none;
        iamf_database_get_audio_element_default_demixing_info(
            self->database, element->element_id, &dmx_default_mode,
            &dmx_default_w_idx);
        ret = iamf_renderer_add_channel_based_element(
            &self->renderer, element->element_id, element->sub_mix_index,
            element->in.layout, element->rendering_config, dmx_default_mode,
            dmx_default_w_idx);
        if (ret != IAMF_OK)
          error(
              "Failed to add channel based element to renderer for element_id: "
              "%u with layout %d",
              element->element_id, element->in.layout);
      } else if (element->scheme == ck_audio_element_type_scene_based) {
        ret = iamf_renderer_add_scene_based_element(
            &self->renderer, element->element_id, element->sub_mix_index,
            element->num_channels, element->rendering_config);
        if (ret != IAMF_OK)
          error(
              ""
              "Failed to add scene based element to renderer for element_id: "
              "%u with scene channels %d",
              element->element_id, element->num_channels);
      } else if (element->scheme == ck_audio_element_type_object_based) {
        ret = iamf_renderer_add_object_based_element(
            &self->renderer, element->element_id, element->sub_mix_index,
            element->num_channels, element->rendering_config);
        if (ret != IAMF_OK)
          error(
              "Failed to add object based element to renderer for element_id: "
              "%u with object channels %d",
              element->element_id, element->num_channels);
      }

      if (ret != IAMF_OK) break;
    } while (!hash_map_iterator_next(iter));
    hash_map_iterator_delete(iter);
  }

  if (ret != IAMF_OK) {
    error("Failed to initialize renderer for presentation %u", self->id);
    iamf_renderer_uninit(&self->renderer);
  }

  return ret;
}

static void iamf_presentation_priv_delete_all_audio_blocks(
    iamf_presentation_t* self) {
  if (!self) return;
  hash_map_iterator_t* iter = hash_map_iterator_new(self->elements);
  if (iter) {
    do {
      iamf_presentation_element_t* element = def_value_wrap_type_ptr(
          iamf_presentation_element_t, hash_map_iterator_get_value(iter));
      if (element && element->block) {
        iamf_audio_block_delete(element->block);
        element->block = 0;
      }
    } while (!hash_map_iterator_next(iter));
    hash_map_iterator_delete(iter);
  }

  self->num_audio_blocks = 0;
}

static int iamf_presentation_priv_sync_audio_blocks(iamf_presentation_t* self) {
  if (!self || !self->elements || !self->synchronizer) {
    error("Invalid presentation or synchronizer");
    return IAMF_ERR_BAD_ARG;
  }

  iamf_audio_block_t* blocks[def_max_audio_streams];
  int n = 0;
  hash_map_iterator_t* iter = hash_map_iterator_new(self->elements);
  if (iter) {
    do {
      iamf_presentation_element_t* element = def_value_wrap_type_ptr(
          iamf_presentation_element_t, hash_map_iterator_get_value(iter));
      if (element) {
        if (!element->block) {
          element->block = iamf_audio_block_default_new(element->element_id);
          if (!element->block)
            warning(
                "Failed to allocate memory for audio block for element (%u) "
                "for synchronization.",
                element->element_id);
          else {
            debug("Allocated audio block for element_id: %u",
                  element->element_id);
            element->block->num_channels = element->num_channels;
          }
        }
        if (element->block && n < def_max_audio_streams)
          blocks[n++] = element->block;
      }

    } while (!hash_map_iterator_next(iter));
    hash_map_iterator_delete(iter);
  }

  return n > 0 ? iamf_synchronizer_sync_audio_blocks(self->synchronizer, blocks,
                                                     n)
               : IAMF_ERR_INTERNAL;
}

static int iamf_presentation_priv_update_element_downmix_info(
    iamf_presentation_t* self, iamf_presentation_element_t* element) {
  if (!self || !element) return IAMF_ERR_BAD_ARG;

  // Get downmix mode for the element
  int downmix_mode = iamf_database_get_audio_element_demix_mode(
      self->database, element->element_id,
      (fraction_t){.numerator = 1, .denominator = element->sampling_rate});

  // Update renderer if downmix mode is not none
  if (downmix_mode != def_dmx_mode_none) {
    int ret = iamf_renderer_update_element_downmix_mode(
        &self->renderer, element->element_id, downmix_mode,
        element->block->num_samples_per_channel);
    if (ret != IAMF_OK) {
      error("Failed to update downmix mode for element_id: %u",
            element->element_id);
      return ret;
    }
    debug("Updated downmix mode to %d for element_id: %u", downmix_mode,
          element->element_id);
  }

  return IAMF_OK;
}

static int iamf_presentation_priv_update_element_gain_offset(
    iamf_presentation_t* self, iamf_presentation_element_t* element) {
  if (!self || !element) return IAMF_ERR_BAD_ARG;

  // Apply element gain offset if enabled
  if (element->enable_gain_offset) {
    animated_float32_t animated_gain =
        _create_step_animated_gain(element->gain_offset.offset);
    int ret = iamf_renderer_update_element_animated_gain(
        &self->renderer, element->element_id, element->gain_offset_pid,
        animated_gain, element->block->num_samples_per_channel);
    if (ret != IAMF_OK) {
      error("Failed to update element gain offset for element_id: %u",
            element->element_id);
      return ret;
    }
    debug(
        "Successfully updated element gain offset: element_id=%u, "
        "gain_offset_pid=%u, offset=%f",
        element->element_id, element->gain_offset_pid,
        element->gain_offset.offset);
  }

  return IAMF_OK;
}

static int iamf_presentation_priv_update_element_mix_gain(
    iamf_presentation_t* self, iamf_presentation_element_t* element) {
  fraction_t num_samples_frac;
  if (!self || !element) return IAMF_ERR_BAD_ARG;

  // Handle second skip if present
  if (element->block->second_skip > 0) {
    animated_float32_t animated_gain = _create_step_animated_gain(1.0f);
    iamf_renderer_update_element_animated_gain(
        &self->renderer, element->element_id, element->mix_gain_id,
        animated_gain, element->block->second_skip);
  }

  parameter_subblock_t** subblocks = 0;
  num_samples_frac.numerator = element->block->num_samples_per_channel -
                               element->block->second_skip -
                               element->block->second_padding;
  num_samples_frac.denominator = element->sampling_rate;
  int n = iamf_database_get_subblocks(self->database, element->mix_gain_id,
                                      num_samples_frac, &subblocks);

  debug("mix gain subblocks count %d", n);

  // Process mix gain subblocks
  if (n > 0) {
    float durations = 0.0f;
    uint32_t rate =
        iamf_database_get_parameter_rate(self->database, element->mix_gain_id);

    for (int i = 0; i < n; i++) {
      uint32_t approximate_durations = durations + 0.5f;
      float duration = iamf_fraction_transform_float(
          def_fraction_instance(subblocks[i]->subblock_duration, rate),
          element->sampling_rate);
      durations += duration;
      duration = durations + 0.5f - approximate_durations;

      mix_gain_parameter_subblock_t* subblock =
          def_mix_gain_parameter_subblock_ptr(subblocks[i]);
      iamf_renderer_update_element_animated_gain(
          &self->renderer, element->element_id, element->mix_gain_id,
          subblock->gain_db, duration);

      durations += duration;
    }
  } else if (n < 0) {
    // Use default mix gain to build animated gain
    parameter_base_t* param_base =
        iamf_database_get_parameter_base(self->database, element->mix_gain_id);
    if (!param_base)
      warning("Parameter base not found for mix gain id %u",
              element->mix_gain_id);
    if (param_base && param_base->type == ck_iamf_parameter_type_mix_gain) {
      mix_gain_parameter_base_t* mix_gain_param =
          (mix_gain_parameter_base_t*)param_base;
      animated_float32_t animated_gain =
          _create_step_animated_gain(mix_gain_param->default_mix_gain_db);

      // Use the entire frame duration
      iamf_renderer_update_element_animated_gain(
          &self->renderer, element->element_id, element->mix_gain_id,
          animated_gain, element->block->num_samples_per_channel);
    }
  }

  if (element->block->second_padding > 0) {
    animated_float32_t animated_gain = _create_step_animated_gain(1.0f);
    iamf_renderer_update_element_animated_gain(
        &self->renderer, element->element_id, element->mix_gain_id,
        animated_gain, element->block->second_padding);
  }

  if (subblocks) def_free(subblocks);

  debug("Successfully processed mix gain for element_id: %u",
        element->element_id);
  return IAMF_OK;
}

static int iamf_presentation_priv_update_element_positions(
    iamf_presentation_t* self, iamf_presentation_element_t* element) {
  fraction_t num_samples_frac;
  if (!self || !element) return IAMF_ERR_BAD_ARG;

  // Only process positions for object-based elements
  if (element->scheme != ck_audio_element_type_object_based) return IAMF_OK;

  if (element->in.position_id == def_i32_id_none) {
    debug("No valid position_id for object-based element_id: %u",
          element->element_id);
    return IAMF_OK;
  }

  parameter_base_t* param_base =
      iamf_database_get_parameter_base(self->database, element->in.position_id);

  if (!param_base || !iamf_parameter_type_is_coordinate(param_base->type)) {
    warning(
        "Invalid or non-coordinate position parameter %u for element_id: %u",
        element->in.position_id, element->element_id);
    return IAMF_OK;
  }

  debug(
      "Processing positions for object-based element_id: %u using position_id: "
      "%u, type: %d",
      element->element_id, element->in.position_id, param_base->type);

  uint32_t position_param_id = element->in.position_id;
  num_samples_frac.numerator = element->block->num_samples_per_channel -
                               element->block->second_skip -
                               element->block->second_padding;
  num_samples_frac.denominator = element->sampling_rate;

  // Get position subblocks based on parameter type
  if (iamf_parameter_type_is_polar(param_base->type)) {
    parameter_subblock_t** subblock = 0;
    int n = iamf_database_get_subblocks(self->database, position_param_id,
                                        num_samples_frac, &subblock);
    polars_parameter_base_t* polars_param =
        def_polars_parameter_base_ptr(param_base);
    int num_objects = polars_param->num_polars > def_max_number_of_objects
                          ? def_max_number_of_objects
                          : polars_param->num_polars;

    debug("polar position subblocks count %d for element_id: %u", n,
          element->element_id);

    if (element->block->second_skip) {
      // Handle second skip for polar positions
      animated_polar_t animated_polar_positions[def_max_number_of_objects];
      for (uint32_t k = 0; k < num_objects; k++)
        animated_polar_positions[k] =
            _create_step_animated_polar(0.0f, 0.0f, 1.0f);

      iamf_renderer_update_element_animated_polar_positions(
          &self->renderer, element->element_id, position_param_id,
          animated_polar_positions, num_objects, element->block->second_skip);
    }

    if (n > 0) {
      float durations = 0.0f;
      uint32_t rate =
          iamf_database_get_parameter_rate(self->database, position_param_id);
      polars_parameter_subblock_t* polars_subblocks =
          def_polars_parameter_subblock_ptr(*subblock);

      for (int j = 0; j < n; j++) {
        uint32_t approximate_durations = durations + 0.5f;
        float duration = iamf_fraction_transform_float(
            def_fraction_instance(polars_subblocks[j].base.subblock_duration,
                                  rate),
            element->sampling_rate);
        durations += duration;
        duration = durations + 0.5f - approximate_durations;

        iamf_renderer_update_element_animated_polar_positions(
            &self->renderer, element->element_id, position_param_id,
            polars_subblocks->polars, polars_subblocks->num_polars, duration);

        durations += duration;
      }
    } else if (n < 0) {
      // Use default positions from parameter_base_t to create virtual subblock
      int num_objects = polars_param->num_polars > def_max_number_of_objects
                            ? def_max_number_of_objects
                            : polars_param->num_polars;

      animated_polar_t animated_polar_positions[def_max_number_of_objects];
      for (uint32_t k = 0; k < num_objects; k++)
        animated_polar_positions[k] = _create_step_animated_polar(
            polars_param->default_polars[k].azimuth,
            polars_param->default_polars[k].elevation,
            polars_param->default_polars[k].distance);

      iamf_renderer_update_element_animated_polar_positions(
          &self->renderer, element->element_id, position_param_id,
          animated_polar_positions, num_objects,
          element->block->num_samples_per_channel);
    }

    if (element->block->second_padding) {
      // Handle second padding for polar positions
      animated_polar_t animated_polar_positions[def_max_number_of_objects];
      for (uint32_t k = 0; k < num_objects; k++)
        animated_polar_positions[k] =
            _create_step_animated_polar(0.0f, 0.0f, 1.0f);

      iamf_renderer_update_element_animated_polar_positions(
          &self->renderer, element->element_id, position_param_id,
          animated_polar_positions, num_objects,
          element->block->second_padding);
    }

    if (subblock) def_free(subblock);

  } else if (iamf_parameter_type_is_cartesian(param_base->type)) {
    parameter_subblock_t** subblock = 0;
    int n = iamf_database_get_subblocks(self->database, position_param_id,
                                        num_samples_frac, &subblock);
    cartesians_parameter_base_t* cartesians_param =
        def_cartesians_parameter_base_ptr(param_base);
    int num_objects =
        cartesians_param->num_cartesians > def_max_number_of_objects
            ? def_max_number_of_objects
            : cartesians_param->num_cartesians;

    debug("cartesian position subblocks count %d for element_id: %u", n,
          element->element_id);

    if (element->block->second_skip) {
      // Handle second skip for cartesian positions
      animated_cartesian_t
          animated_cartesian_positions[def_max_number_of_objects];
      for (uint32_t k = 0; k < num_objects; k++)
        animated_cartesian_positions[k] =
            _create_step_animated_cartesian(0.0f, 0.0f, 0.0f);

      iamf_renderer_update_element_animated_cartesian_positions(
          &self->renderer, element->element_id, animated_cartesian_positions,
          num_objects, element->block->second_skip);
    }

    if (n > 0) {
      cartesians_parameter_subblock_t* cartesians_subblocks =
          def_cartesians_parameter_subblock_ptr(*subblock);
      float durations = 0.0f;
      uint32_t rate =
          iamf_database_get_parameter_rate(self->database, position_param_id);

      for (int j = 0; j < n; j++) {
        uint32_t approximate_durations = durations + 0.5f;
        float duration = iamf_fraction_transform_float(
            def_fraction_instance(
                cartesians_subblocks[j].base.subblock_duration, rate),
            element->sampling_rate);
        durations += duration;
        duration = durations + 0.5f - approximate_durations;

        iamf_renderer_update_element_animated_cartesian_positions(
            &self->renderer, element->element_id,
            cartesians_subblocks->cartesians,
            cartesians_subblocks->num_cartesians, duration);

        durations += duration;
      }
    } else if (n < 0) {
      // Use default positions from parameter_base_t
      int num_objects =
          cartesians_param->num_cartesians > def_max_number_of_objects
              ? def_max_number_of_objects
              : cartesians_param->num_cartesians;

      animated_cartesian_t
          animated_cartesian_positions[def_max_number_of_objects];
      for (uint32_t k = 0; k < num_objects; k++)
        animated_cartesian_positions[k] = _create_step_animated_cartesian(
            cartesians_param->default_cartesians[k].x,
            cartesians_param->default_cartesians[k].y,
            cartesians_param->default_cartesians[k].z);

      iamf_renderer_update_element_animated_cartesian_positions(
          &self->renderer, element->element_id, animated_cartesian_positions,
          num_objects, element->block->num_samples_per_channel);
    }

    if (element->block->second_padding) {
      // Handle second padding for cartesian positions
      animated_cartesian_t
          animated_cartesian_positions[def_max_number_of_objects];
      for (uint32_t k = 0; k < num_objects; k++)
        animated_cartesian_positions[k] =
            _create_step_animated_cartesian(0.0f, 0.0f, 0.0f);

      iamf_renderer_update_element_animated_cartesian_positions(
          &self->renderer, element->element_id, animated_cartesian_positions,
          num_objects, element->block->second_padding);
    }

    if (subblock) def_free(subblock);
  }

  debug("Successfully processed positions for object-based element_id: %u",
        element->element_id);
  return IAMF_OK;
}

static int iamf_presentation_priv_update_output_mix_gain(
    iamf_presentation_t* self, iamf_audio_block_t* block,
    fraction_t num_samples_frac) {
  int num_gains = vector_size(self->output_mix_gain_ids);
  if (!self || !block) return IAMF_ERR_BAD_ARG;
  if (num_gains <= 0) return IAMF_ERR_INTERNAL;

  for (int g = 0; g < num_gains; g++) {
    // set output mix gain
    uint32_t pid = def_value_wrap_u32(vector_at(self->output_mix_gain_ids, g));
    parameter_subblock_t** subblocks = 0;
    int n = iamf_database_get_subblocks(self->database, pid, num_samples_frac,
                                        &subblocks);
    uint32_t rate = iamf_database_get_parameter_rate(self->database, pid);

    // Handle second skip if present
    if (block->second_skip > 0) {
      animated_float32_t animated_gain = _create_step_animated_gain(1.0f);
      iamf_renderer_update_animated_gain(&self->renderer, pid, g, animated_gain,
                                         block->second_skip);
    }

    // Process output mix gain subblocks
    if (n > 0) {
      float durations = 0.0f;
      for (int i = 0; i < n; i++) {
        if (!subblocks[i]) continue;

        uint32_t approximate_durations = durations + 0.5f;
        float duration = iamf_fraction_transform_float(
            def_fraction_instance(subblocks[i]->subblock_duration, rate),
            num_samples_frac.denominator);
        durations += duration;
        duration = durations + 0.5f - approximate_durations;

        mix_gain_parameter_subblock_t* mix_gain_subblock =
            def_mix_gain_parameter_subblock_ptr(subblocks[i]);
        iamf_renderer_update_animated_gain(
            &self->renderer, pid, g, mix_gain_subblock->gain_db, duration);
        durations += duration;
      }
    } else if (n < 0) {
      // Use default mix gain to build animated gain
      parameter_base_t* param_base =
          iamf_database_get_parameter_base(self->database, pid);
      if (param_base && param_base->type == ck_iamf_parameter_type_mix_gain) {
        mix_gain_parameter_base_t* mix_gain_param =
            (mix_gain_parameter_base_t*)param_base;
        animated_float32_t animated_gain =
            _create_step_animated_gain(mix_gain_param->default_mix_gain_db);

        // Use the entire frame duration
        uint32_t frame_duration = block->num_samples_per_channel;
        iamf_renderer_update_animated_gain(&self->renderer, pid, g,
                                           animated_gain, frame_duration);
      }
    }

    if (block->second_padding > 0) {
      animated_float32_t animated_gain = _create_step_animated_gain(1.0f);
      iamf_renderer_update_animated_gain(&self->renderer, pid, g, animated_gain,
                                         block->second_padding);
    }

    if (subblocks) def_free(subblocks);
  }
  debug("Successfully processed output mix gain for presentation_id: %u",
        self->id);
  return IAMF_OK;
}

/**
 * @brief Get best loudness for a specific sub_mix
 *
 * This function searches for the best loudness value in a specific sub_mix
 * that matches the given audio layout.
 *
 * @param obj Pointer to mix presentation OBU
 * @param layout Target audio layout
 * @param sub_mix_index Index of sub_mix to search
 * @return Loudness value in dB (LKFS), or default loudness if not found
 */
static float _sub_mix_get_best_loudness(iamf_mix_presentation_obu_t* obj,
                                        iamf_layout_t* layout,
                                        int sub_mix_index) {
  obu_sub_mix_t* sub;
  float loudness_lkfs = def_default_loudness_lkfs;
  iamf_layout_t highest_layout =
      def_sound_system_layout_instance(SOUND_SYSTEM_NONE);

  sub = def_value_wrap_optional_ptr(array_at(obj->sub_mixes, sub_mix_index));
  if (!sub) {
    warning("Sub_mix at index %d is NULL", sub_mix_index);
    return loudness_lkfs;
  }

  int n = array_size(sub->loudness_layouts);
  if (n <= 0) {
    debug("No loudness layouts in sub_mix %d", sub_mix_index);
    return loudness_lkfs;
  }

  for (int i = 0; i < n; ++i) {
    iamf_layout_t* loudness_layout =
        def_value_wrap_optional_ptr(array_at(sub->loudness_layouts, i));
    if (!loudness_layout) continue;

    if (iamf_layout_is_equal(*layout, *loudness_layout)) {
      obu_loudness_info_t* li =
          def_value_wrap_optional_ptr(array_at(sub->loudness, i));
      if (li) {
        loudness_lkfs =
            iamf_gain_q78_to_db(def_lsb_16bits(li->integrated_loudness));
        info(
            "selected loudness %f db from exact match layout in sub_mix[%d] "
            "<- 0x%x",
            loudness_lkfs, sub_mix_index,
            def_lsb_16bits(li->integrated_loudness));
        return loudness_lkfs;
      }
    }

    if (iamf_layout_is_equal(highest_layout, def_sound_system_layout_instance(
                                                 SOUND_SYSTEM_NONE)) ||
        iamf_layout_higher_check(*loudness_layout, highest_layout, 1)) {
      obu_loudness_info_t* li =
          def_value_wrap_optional_ptr(array_at(sub->loudness, i));
      if (li) {
        highest_layout = *loudness_layout;
        loudness_lkfs =
            iamf_gain_q78_to_db(def_lsb_16bits(li->integrated_loudness));
        debug(
            "selected loudness %f db from highest layout in sub_mix[%d] <- "
            "0x%x",
            loudness_lkfs, sub_mix_index,
            def_lsb_16bits(li->integrated_loudness));
      }
    }
  }

  debug("selected loudness %f db from highest layout %s in sub_mix[%d]",
        loudness_lkfs, iamf_layout_string(highest_layout), sub_mix_index);

  return loudness_lkfs;
}

/**
 * @brief Initialize loudness for all sub-mixes (internal function) (MOD 5)
 *
 * This function is called during iamf_presentation_create() to
 * pre-fetch loudness values for all sub-mixes and store them in the
 * presentation. This optimizes performance by fetching loudness only once
 * during creation.
 *
 * @param self Pointer to presentation instance
 * @param layout Audio layout for loudness lookup
 * @return IAMF_OK on success, error code on failure
 *
 * @note This function:
 *       • Iterates through all sub-mixes in the mix presentation
 *       • Retrieves loudness for each sub-mix matching the given layout
 *       • Stores loudness values in self->loudnesses[] array
 *       • No need to re-fetch during iamf_presentation_set_loudness()
 */
static int iamf_presentation_priv_init_loudness(iamf_presentation_t* self,
                                                iamf_layout_t* layout) {
  if (!self || !layout) return IAMF_ERR_BAD_ARG;

  iamf_mix_presentation_obu_t* mpo =
      self->database
          ? iamf_database_get_mix_presentation_obu(self->database, self->id)
          : NULL;
  if (!mpo) {
    debug("No mix presentation, skip loudness initialization");
    return IAMF_OK;
  }

  int num_sub_mixes = array_size(mpo->sub_mixes);

  for (int sub_idx = 0; sub_idx < num_sub_mixes; ++sub_idx) {
    self->loudnesses[sub_idx] =
        _sub_mix_get_best_loudness(mpo, layout, sub_idx);
    debug("Sub_mix[%d] loudness: %f dB", sub_idx, self->loudnesses[sub_idx]);
  }

  self->num_loudnesses = num_sub_mixes;
  info("Initialized loudness for %u sub-mixes", num_sub_mixes);

  return IAMF_OK;
}

iamf_presentation_t* iamf_presentation_create(
    uint32_t id, iamf_database_t* database,
    iamf_element_reconstructor_t* reconstructor, iamf_layout_t layout) {
  iamf_presentation_t* self = 0;
  int ret = IAMF_OK;

  if (!database || !reconstructor) return 0;

  if (!iamf_database_get_mix_presentation_obu(database, id)) {
    error("No mix self OBU found for ID: %u", id);
    return 0;
  }

  self = def_mallocz(iamf_presentation_t, 1);
  if (!self) return 0;

  self->id = id;
  self->database = database;
  self->reconstructor = reconstructor;
  self->layout = layout;

  self->elements = hash_map_new(def_hash_map_capacity_elements);
  if (!self->elements) {
    def_err_msg_enomem("Elements", def_pst_str);
    iamf_presentation_destroy(self);
    return 0;
  }

  self->synchronizer = iamf_synchronizer_create();
  if (!self->synchronizer) {
    def_err_msg_enomem("Synchronizer", def_pst_str);
    iamf_presentation_destroy(self);
    return 0;
  }

  self->output_mix_gain_ids = vector_new();
  if (!self->output_mix_gain_ids) {
    error("Failed to allocate memory for output mix gain IDs");
    iamf_presentation_destroy(self);
    return 0;
  }

  if (iamf_presentation_priv_make_elements(self) != IAMF_OK) {
    error("Failed to create element presentations");
    iamf_presentation_destroy(self);
    return 0;
  }

  ret = iamf_presentation_priv_add_elements_to_reconstructor(self);
  if (ret != IAMF_OK) {
    error("Errno(%d): failed to add elements to reconstructor", ret);
    iamf_presentation_destroy(self);
    return 0;
  }

  ret = iamf_presentation_priv_activate_elements_and_parameters(self);
  if (ret != IAMF_OK) {
    error("Errno(%d): failed to set layout and activate reconstructor entries",
          ret);
    iamf_presentation_destroy(self);
    return 0;
  }

  if (self->elements && hash_map_size(self->elements) > 0) {
    hash_map_iterator_t* iter = hash_map_iterator_new(self->elements);
    if (iter) {
      iamf_presentation_element_t* element = def_value_wrap_type_ptr(
          iamf_presentation_element_t, hash_map_iterator_get_value(iter));
      if (element) {
        self->out_sampling_rate = element->sampling_rate;
        debug("Set presentation out_sampling_rate to %d from element %u",
              self->out_sampling_rate, element->element_id);
      }
      hash_map_iterator_delete(iter);
    }
  }

  if (iamf_presentation_priv_activate_renderer(self) != IAMF_OK) {
    error("Failed to activate renderer");
    iamf_presentation_destroy(self);
    return 0;
  }

  iamf_presentation_priv_init_loudness(self, &self->layout);

  info("Successfully initialized IAMF OAR");

  return self;
}

void iamf_presentation_destroy(iamf_presentation_t* self) {
  if (!self) return;

  if (self->output_mix_gain_ids) vector_free(self->output_mix_gain_ids, 0);
  if (self->elements) {
    iamf_presentation_priv_delete_all_elements(self->elements);
    hash_map_delete(self->elements, 0);
    self->elements = 0;
  }
  iamf_renderer_uninit(&self->renderer);
  if (self->synchronizer) iamf_synchronizer_destroy(self->synchronizer);
  def_free(self);
}

int iamf_presentation_set_element_gain_offset(iamf_presentation_t* self,
                                              uint32_t eid, float offset) {
  if (!self) {
    error("Invalid presentation object");
    return IAMF_ERR_BAD_ARG;
  }

  if (!self->elements) {
    error("Presentation elements not initialized");
    return IAMF_ERR_INTERNAL;
  }

  // Find the presentation element by element_id
  value_wrap_t* val = hash_map_get(self->elements, eid);
  if (!val) {
    error("Element with ID %u not found in presentation", eid);
    return IAMF_ERR_INTERNAL;
  }

  iamf_presentation_element_t* element =
      def_value_wrap_type_ptr(iamf_presentation_element_t, val);
  if (!element) {
    error("Invalid element for element_id: %u", eid);
    return IAMF_ERR_INTERNAL;
  }

  // Check if gain offset is enabled for this element
  if (!element->enable_gain_offset) {
    warning("Gain offset is not enabled for element ID %u", eid);
    return IAMF_ERR_BAD_ARG;
  }

  // Validate offset is within allowed range
  if (offset < element->gain_offset.min || offset > element->gain_offset.max) {
    error("Gain offset %f is out of range [%f, %f] for element ID %u", offset,
          element->gain_offset.min, element->gain_offset.max, eid);
    return IAMF_ERR_BAD_ARG;
  }

  // Set the gain offset
  element->gain_offset.offset = offset;
  debug("Successfully set gain offset to %f for element ID %u", offset, eid);

  return IAMF_OK;
}

int iamf_presentation_get_element_gain_offset(iamf_presentation_t* self,
                                              uint32_t eid, float* offset) {
  if (!self || !offset) {
    error("Invalid presentation object or offset pointer");
    return IAMF_ERR_BAD_ARG;
  }

  if (!self->elements) {
    error("Presentation elements not initialized");
    return IAMF_ERR_INTERNAL;
  }

  // Find the presentation element by element_id
  value_wrap_t* val = hash_map_get(self->elements, eid);
  if (!val) {
    error("Element with ID %u not found in presentation", eid);
    return IAMF_ERR_INTERNAL;
  }

  iamf_presentation_element_t* element =
      def_value_wrap_type_ptr(iamf_presentation_element_t, val);
  if (!element) {
    error("Invalid element for element_id: %u", eid);
    return IAMF_ERR_INTERNAL;
  }

  // Check if gain offset is enabled for this element
  if (!element->enable_gain_offset) {
    warning("Gain offset is not enabled for element ID %u", eid);
    return IAMF_ERR_BAD_ARG;
  }

  // Get the gain offset
  *offset = element->gain_offset.offset;
  debug("Successfully got gain offset %f for element ID %u", *offset, eid);

  return IAMF_OK;
}

int iamf_presentation_set_loudness(iamf_presentation_t* self,
                                   float target_loudness_lkfs) {
  if (!self) return IAMF_ERR_BAD_ARG;

  debug("Set loudness normalization: target=%.2f dB", target_loudness_lkfs);

  if (target_loudness_lkfs == def_default_loudness_lkfs) {
    int ret = iamf_renderer_enable_loudness_processor(&self->renderer, 0);
    if (ret != IAMF_OK) {
      error("Failed to disable loudness processor");
      return ret;
    }
    debug("Loudness normalization disabled (target=%.2f dB = default)",
          target_loudness_lkfs);
    return IAMF_OK;
  }

  for (int sub_idx = 0; sub_idx < self->num_loudnesses; ++sub_idx) {
    if (self->loudnesses[sub_idx] != def_default_loudness_lkfs) {
      float current_loudness = self->loudnesses[sub_idx];
      int ret = iamf_renderer_set_loudness(
          &self->renderer, sub_idx, current_loudness, target_loudness_lkfs);

      if (ret != IAMF_OK) {
        error(
            "Failed to set loudness for sub_mix[%u]: current=%.2f dB, "
            "target=%.2f dB",
            sub_idx, current_loudness, target_loudness_lkfs);
        return ret;
      }

      info("Set loudness for sub_mix[%u]: current=%.2f dB, target=%.2f dB",
           sub_idx, current_loudness, target_loudness_lkfs);
    } else {
      debug("Sub_mix[%u] has default loudness, skipping loudness setting",
            sub_idx);
    }
  }

  int ret = iamf_renderer_enable_loudness_processor(&self->renderer, 1);
  if (ret != IAMF_OK) {
    error("Failed to enable loudness processor");
    return ret;
  }

  info("Loudness normalization enabled with target=%.2f dB",
       target_loudness_lkfs);

  return IAMF_OK;
}

uint32_t iamf_presentation_get_id(iamf_presentation_t* self) {
  if (!self) return def_i32_id_none;
  return self->id;
}

int iamf_presentation_get_sampling_rate(iamf_presentation_t* self) {
  if (!self) return IAMF_ERR_BAD_ARG;
  return self->out_sampling_rate;
}

int iamf_presentation_add_audio_block(iamf_presentation_t* self,
                                      iamf_audio_block_t* audio_block) {
  int ret = IAMF_OK;
  if (!self || !audio_block) {
    error("Invalid arguments: presentation or audio_block is NULL");
    return IAMF_ERR_BAD_ARG;
  }

  uint32_t eid = audio_block->id;
  iamf_presentation_element_t* element = 0;
  value_wrap_t* val = 0;

  // Find the presentation element by element_id
  val = hash_map_get(self->elements, eid);
  if (!val) {
    error("Element with ID %u not found in presentation", eid);
    return IAMF_ERR_INTERNAL;
  }

  element = def_value_wrap_type_ptr(iamf_presentation_element_t, val);

  if (element->block) {
    ret = IAMF_ERR_INVALID_PACKET;
    warning("Audio block already exists for element ID %u", eid);
    iamf_presentation_priv_delete_all_audio_blocks(self);
  }

  element->block = audio_block;
  ++self->num_audio_blocks;
  debug("Successfully added audio block for element ID %u", eid);

  if (self->num_audio_blocks == hash_map_size(self->elements)) ret = 1;
  return ret;
}

iamf_audio_block_t* iamf_presentation_process(iamf_presentation_t* self) {
  fraction_t num_samples_frac;
  uint32_t num_output_samples = 0;
  iamf_audio_block_t* block = 0;
  int ret = IAMF_OK;

  if (!self || !self->elements || !self->database) {
    error("Invalid presentation object for processing.");
    return 0;
  }

  // Check if all audio blocks are present before starting processing.
  if (self->num_audio_blocks != hash_map_size(self->elements)) {
    warning(
        "Not all audio blocks received for presentation %u. Received %u, "
        "expected %u.",
        self->id, self->num_audio_blocks, hash_map_size(self->elements));
  }

  ret = iamf_presentation_priv_sync_audio_blocks(self);
  if (ret != IAMF_OK)
    warning("Failed to sync audio blocks for presentation %u", self->id);

  hash_map_iterator_t* iter = hash_map_iterator_new(self->elements);
  if (!iter) {
    error("Failed to create iterator for presentation elements.");
    return 0;
  }

  block = def_value_wrap_type_ptr(iamf_presentation_element_t,
                                  hash_map_iterator_get_value(iter))
              ->block;

  do {
    iamf_presentation_element_t* element = def_value_wrap_type_ptr(
        iamf_presentation_element_t, hash_map_iterator_get_value(iter));
    if (!element || !element->block) {
      // This should ideally not be hit if num_audio_blocks check passes.
      error("Invalid element or missing audio block for element_id: %u",
            hash_map_iterator_get_key(iter));
      ret = IAMF_ERR_INTERNAL;
      break;
    }

    if (!num_output_samples) {
      num_output_samples = element->number_samples_per_frame;
      num_samples_frac.numerator = element->block->num_samples_per_channel -
                                   element->block->second_skip -
                                   element->block->second_padding;
      num_samples_frac.denominator = element->sampling_rate;
    }

    iamf_presentation_priv_update_element_downmix_info(self, element);

    iamf_presentation_priv_update_element_gain_offset(self, element);

    iamf_presentation_priv_update_element_mix_gain(self, element);

    iamf_presentation_priv_update_element_positions(self, element);

    ret = iamf_renderer_add_element_audio_data(&self->renderer, element->block);
    if (ret != IAMF_OK) {
      error("Failed to add audio data for element_id: %u", element->element_id);
      break;
    }

  } while (!hash_map_iterator_next(iter));
  hash_map_iterator_delete(iter);

  if (ret != IAMF_OK) return 0;

  iamf_presentation_priv_update_output_mix_gain(self, block, num_samples_frac);

  iamf_audio_block_t* output_block = iamf_audio_block_new(
      self->id, num_output_samples, iamf_layout_channels_count(&self->layout));
  if (!output_block) {
    error("Failed to allocate memory for output audio block.");
    return 0;
  }

  iamf_audio_block_samples_info_copy(output_block, block);

  ret = iamf_renderer_render(&self->renderer, output_block);

  iamf_presentation_priv_delete_all_audio_blocks(self);

  if (ret != IAMF_OK) {
    error("errno <%d> : renderer failed to produce output audio block.", ret);
    iamf_audio_block_delete(output_block);
    return 0;
  }

  debug("Successfully processed presentation %u.", self->id);
  return output_block;
}

int iamf_presentation_set_head_rotation(iamf_presentation_t* self,
                                        const quaternion_t* quaternion) {
  if (!self || !quaternion) return IAMF_ERR_BAD_ARG;

  if (self->layout.type != ck_iamf_layout_type_binaural) {
    warning(
        "Head rotation is only effective for binaural output layout. "
        "Current layout type: %d. Head rotation will be stored but not "
        "applied.",
        self->layout.type);
    return IAMF_ERR_INVALID_STATE;
  }

  debug("Set head rotation in presentation: w=%f, x=%f, y=%f, z=%f",
        quaternion->w, quaternion->x, quaternion->y, quaternion->z);

  return iamf_renderer_set_head_rotation(&self->renderer, quaternion);
}

int iamf_presentation_enable_head_tracking(iamf_presentation_t* self,
                                           uint32_t enable) {
  if (!self) return IAMF_ERR_BAD_ARG;

  if (self->layout.type != ck_iamf_layout_type_binaural) {
    warning(
        "Head tracking is only effective for binaural output layout. "
        "Current layout type: %d. Head tracking setting will be stored but not "
        "applied.",
        self->layout.type);
    return IAMF_ERR_INVALID_STATE;
  }

  debug("Head tracking %s in presentation", enable ? "enabled" : "disabled");

  return iamf_renderer_enable_head_tracking(&self->renderer, enable);
}
