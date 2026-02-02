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
 * @file mix_presentation_obu.c
 * @brief Mix presentation OBU implementation.
 * @version 2.0.0
 * @date Created 12/11/2025
 **/

#include "mix_presentation_obu.h"

#include "audio_element_obu.h"
#include "iamf_layout.h"
#include "iamf_private_definitions.h"
#include "iamf_string.h"
#include "iamf_types.h"

#ifdef SUPPORT_VERIFIER
#include "vlogging_tool_sr.h"
#endif

#undef def_log_tag
#define def_log_tag "OBU_MP"
#define def_mp_str "Mix Presentation OBU"

#define def_loudness_info_type_base \
  (def_loudness_info_type_true_peak | def_loudness_info_type_anchored)

static int _obu_mp_read_n_strings(io_context_t *r, uint32_t count,
                                  array_t *array);
static obu_sub_mix_t *_obu_mp_sub_mix_new(io_context_t *ior,
                                          uint32_t count_label);
static void _obu_mp_sub_mix_free(obu_sub_mix_t *sub);
static void _obu_mp_loudness_info_free(obu_loudness_info_t *info);
static int _obu_mp_check(iamf_mix_presentation_obu_t *obu);
static int _obu_mp_find_audio_element(value_wrap_t a, value_wrap_t b);
static int _obu_mp_tag_check(const char *name, const char *value);

iamf_mix_presentation_obu_t *iamf_mix_presentation_obu_new(
    io_context_t *ior, iamf_obu_header_t *header) {
  io_context_t *r = ior;
  iamf_mix_presentation_obu_t *obu = 0;
  uint32_t count_label, val;

  obu = def_mallocz(iamf_mix_presentation_obu_t, 1);
  if (!obu) {
    def_err_msg_enomem(def_mp_str, def_iamf_ustr);
    return 0;
  }

  obu->obu.obu_type = ck_iamf_obu_mix_presentation;

  obu->mix_presentation_id = ior_leb128_u32(r);
  count_label = ior_leb128_u32(r);  // count_label

  if (count_label > 0) {
    obu->annotations_languages = array_new(count_label);
    obu->localized_presentation_annotations = array_new(count_label);

    if (!obu->annotations_languages ||
        !obu->localized_presentation_annotations) {
      if (!obu->annotations_languages)
        def_err_msg_enomem("languages", def_mp_str);
      if (!obu->localized_presentation_annotations)
        def_err_msg_enomem("annotations", def_mp_str);
      iamf_mix_presentation_obu_free(obu);
      return 0;
    }

    info("Mix presentation id(%u), count label(%u).", obu->mix_presentation_id,
         count_label);

    _obu_mp_read_n_strings(r, count_label, obu->annotations_languages);
    _obu_mp_read_n_strings(r, count_label,
                           obu->localized_presentation_annotations);

    for (uint32_t i = 0; i < count_label; ++i) {
      info("label(%u):", i);
      info("\tAnnotation language: %s",
           (char *)def_value_wrap_ptr(array_at(obu->annotations_languages, i)));
      info("\tLocalized presentation annotation: %s",
           (char *)def_value_wrap_ptr(
               array_at(obu->localized_presentation_annotations, i)));
    }
  }

  val = ior_leb128_u32(r);  // num_sub_mixes
  obu->sub_mixes = array_new(val);

  if (!obu->sub_mixes) {
    def_err_msg_enomem("sub-mixes", def_mp_str);
    iamf_mix_presentation_obu_free(obu);
    return 0;
  }

  info("Number of sub mixes(%u):", val);

  for (uint32_t i = 0; i < val; ++i) {
    obu_sub_mix_t *sub = _obu_mp_sub_mix_new(r, count_label);
    if (!sub) {
      def_err_msg_enomem("sub mix", def_mp_str);
      iamf_mix_presentation_obu_free(obu);
      return 0;
    }
    def_value_wrap_ptr(array_at(obu->sub_mixes, i)) = sub;
  }

  val = ioc_remain(r);

  if (val) {
    val = ior_8(r);
    info("Number of mix presentation tags(%u):", val);
    if (val) {
      obu->mix_presentation_tags = array_new(val);
      if (!obu->mix_presentation_tags) {
        def_err_msg_enomem("tags", def_mp_str);
        iamf_mix_presentation_obu_free(obu);
        return 0;
      }

      for (uint32_t i = 0; i < val; ++i) {
        value_wrap_t *v = 0;
        iamf_tag_t *tag = iamf_tag_new(r);
        if (!tag) {
          iamf_mix_presentation_obu_free(obu);
          return 0;
        }
        info("\tTag(%u): (%s:%s)", i, tag->name, tag->value);
        v = array_at(obu->mix_presentation_tags, i);
        v->ptr = tag;
      }
    }

    if (header && header->optional_fields_flag) {
      uint32_t size = ior_leb128_u32(r);
      obu->preferred_loudspeaker_renderer = ior_8(r);
      obu->preferred_binaural_renderer = ior_8(r);
      ior_skip(r, size - 2);
      info(
          "Preferred loudspeaker renderer(%u), preferred binaural renderer(%u)",
          obu->preferred_loudspeaker_renderer,
          obu->preferred_binaural_renderer);
    }
  }

#if SUPPORT_VERIFIER
  vlog_obu(ck_iamf_obu_mix_presentation, obu, 0, 0);
#endif

  iamf_mix_presentation_obu_display(obu);

  if (!_obu_mp_check(obu)) {
    iamf_mix_presentation_obu_free(obu);
    return 0;
  }

  return obu;
}

void iamf_mix_presentation_obu_free(iamf_mix_presentation_obu_t *obu) {
  if (!obu) return;

  if (obu->annotations_languages) array_free(obu->annotations_languages, free);
  if (obu->localized_presentation_annotations)
    array_free(obu->localized_presentation_annotations, free);
  if (obu->sub_mixes)
    array_free(obu->sub_mixes, def_default_free_ptr(_obu_mp_sub_mix_free));
  if (obu->mix_presentation_tags) array_free(obu->mix_presentation_tags, free);

  free(obu);
}

int iamf_mix_presentation_obu_check_profile(iamf_mix_presentation_obu_t *obu,
                                            vector_t *audio_element_obus,
                                            iamf_profile_t profile) {
  value_wrap_t v;
  obu_sub_mix_t *sub = 0;
  int n = 0;
  obu_audio_element_config_t *aelem_config;
  iamf_audio_element_obu_t *audio_element_obu;
  int channels = 0, elements = 0;
  const iamf_profile_info_t *info = iamf_profile_info_get(profile);

  n = array_size(obu->sub_mixes);
  if (n > info->max_sub_mixes) {
    warning(
        "Too many sub mixes %u (should be <= %u) in mix presentation %u for "
        "%s(%u",
        n, info->max_sub_mixes, obu->mix_presentation_id,
        iamf_profile_type_string(profile), profile);
    return def_error;
  }

  for (int i = 0; i < n; ++i) {
    int num = 0;
    sub = def_value_wrap_optional_ptr(array_at(obu->sub_mixes, i));
    num = array_size(sub->audio_element_configs);
    elements += num;

    if (elements > info->max_elements) {
      warning(
          "Too many elements %u (should be <= %u) in mix presentation %u for "
          "%s(%u)",
          elements, info->max_elements, obu->mix_presentation_id,
          iamf_profile_type_string(profile), profile);
      return def_error;
    }

    for (int e = 0; e < num; ++e) {
      aelem_config =
          def_value_wrap_optional_ptr(array_at(sub->audio_element_configs, e));
      if (vector_find(audio_element_obus,
                      def_value_wrap_instance_u32(aelem_config->element_id),
                      _obu_mp_find_audio_element, &v) < 0) {
        warning("Invalid element %u in mix presentation %u",
                aelem_config->element_id, obu->mix_presentation_id);
        return def_error;
      }

      if (aelem_config->rendering_config.headphones_rendering_mode >
          info->max_available_headphone_rendering_mode) {
        warning(
            "Invalid headphones rendering mode %d in element %u of mix "
            "presentation %u for profile %s(%u).",
            aelem_config->rendering_config.headphones_rendering_mode,
            aelem_config->element_id, obu->mix_presentation_id,
            iamf_profile_type_string(profile), profile);
        return def_error;
      }

      audio_element_obu = v.ptr;
      if (audio_element_obu->audio_element_type ==
          ck_audio_element_type_channel_based) {
        channel_based_audio_element_obu_t *cae =
            def_channel_based_audio_element_obu_ptr(audio_element_obu);
        obu_channel_layer_config_t *layer_config =
            def_value_wrap_optional_ptr(array_at(
                cae->channel_audio_layer_configs, cae->max_valid_layers - 1));
        channels += iamf_loudspeaker_layout_get_info(
                        iamf_audio_layer_layout_get(
                            layer_config->loudspeaker_layout,
                            layer_config->expanded_loudspeaker_layout))
                        ->channels;
      } else if (audio_element_obu->audio_element_type ==
                 ck_audio_element_type_scene_based) {
        scene_based_audio_element_obu_t *sae =
            def_scene_based_audio_element_obu_ptr(audio_element_obu);
        channels += sae->output_channel_count;
      } else if (audio_element_obu->audio_element_type ==
                 ck_audio_element_type_object_based) {
        object_based_audio_element_obu_t *oae =
            def_object_based_audio_element_obu_ptr(audio_element_obu);
        channels += oae->num_objects;
      }

      if (channels > info->max_channels) {
        warning("Id (%u) has %d channels, more than %u",
                obu->mix_presentation_id, channels, info->max_channels);
        return def_error;
      }
    }
  }

  return def_pass;
}

void iamf_mix_presentation_obu_display(iamf_mix_presentation_obu_t *obu) {
  if (!obu) {
    warning("Mix Presentation OBU is NULL, cannot display.");
    return;
  }

  debug("Displaying Mix Presentation OBU:");
  debug("  mix_presentation_id: %u", obu->mix_presentation_id);

  if (obu->annotations_languages && obu->localized_presentation_annotations) {
    uint32_t num_labels = array_size(obu->annotations_languages);
    if (num_labels != array_size(obu->localized_presentation_annotations)) {
      warning(
          "Mismatch in annotations_languages and "
          "localized_presentation_annotations array sizes.");
    } else {
      debug("  Number of annotation labels: %u", num_labels);
      for (uint32_t i = 0; i < num_labels; ++i) {
        char *lang =
            (char *)def_value_wrap_ptr(array_at(obu->annotations_languages, i));
        char *annotation = (char *)def_value_wrap_ptr(
            array_at(obu->localized_presentation_annotations, i));
        debug("  Label [%u]:", i);
        debug("    Annotation Language: %s", lang ? lang : "NULL");
        debug("    Localized Presentation Annotation: %s",
              annotation ? annotation : "NULL");
      }
    }
  } else {
    debug("  No annotation languages or localized presentation annotations.");
  }

  if (obu->sub_mixes) {
    uint32_t num_sub_mixes = array_size(obu->sub_mixes);
    debug("  Number of sub-mixes: %u", num_sub_mixes);
    for (uint32_t i = 0; i < num_sub_mixes; ++i) {
      obu_sub_mix_t *sub_mix =
          (obu_sub_mix_t *)def_value_wrap_ptr(array_at(obu->sub_mixes, i));
      if (!sub_mix) {
        warning("  Sub-mix [%u] is NULL.", i);
        continue;
      }
      debug("  Sub-mix [%u]:", i);

      if (sub_mix->audio_element_configs) {
        uint32_t num_audio_elements =
            array_size(sub_mix->audio_element_configs);
        debug("    Number of audio element configs: %u", num_audio_elements);
        for (uint32_t j = 0; j < num_audio_elements; ++j) {
          obu_audio_element_config_t *ae_config =
              (obu_audio_element_config_t *)def_value_wrap_ptr(
                  array_at(sub_mix->audio_element_configs, j));
          if (!ae_config) {
            warning("    Audio element config [%u] is NULL.", j);
            continue;
          }
          debug("    Audio Element Config [%u]:", j);
          debug("      element_id: %u", ae_config->element_id);
          debug("      headphones_rendering_mode: %u (%s)",
                ae_config->rendering_config.headphones_rendering_mode,
                iamf_headphones_rendering_mode_string(
                    ae_config->rendering_config.headphones_rendering_mode));

          // Display rendering config details
          debug("      Rendering Config:");
          debug("        flags: 0x%08x", ae_config->rendering_config.flags);
          debug("        binaural_filter_profile: %u (%s)",
                ae_config->rendering_config.binaural_filter_profile,
                iamf_binaural_filter_profile_string(
                    ae_config->rendering_config.binaural_filter_profile));
          debug("        rendering_config_extension_size: %u",
                ae_config->rendering_config.rendering_config_extension_size);

          // Display element gain offset information
          if (ae_config->rendering_config.flags &
              def_rendering_config_flag_element_gain_offset) {
            debug("        element_gain_offset_type: %u",
                  ae_config->rendering_config.element_gain_offset_type);
            if (ae_config->rendering_config.element_gain_offset_type ==
                ck_element_gain_offset_type_value) {
              debug("        element_gain_offset.value: %d",
                    ae_config->rendering_config.element_gain_offset.value);
              debug("        element_gain_offset_db.value: %f dB",
                    ae_config->rendering_config.element_gain_offset_db.value);
            } else if (ae_config->rendering_config.element_gain_offset_type ==
                       ck_element_gain_offset_type_range) {
              debug("        element_gain_offset.value: %d",
                    ae_config->rendering_config.element_gain_offset.value);
              debug("        element_gain_offset.min: %d",
                    ae_config->rendering_config.element_gain_offset.min);
              debug("        element_gain_offset.max: %d",
                    ae_config->rendering_config.element_gain_offset.max);
              debug("        element_gain_offset_db.value: %f dB",
                    ae_config->rendering_config.element_gain_offset_db.value);
              debug("        element_gain_offset_db.min: %f dB",
                    ae_config->rendering_config.element_gain_offset_db.min);
              debug("        element_gain_offset_db.max: %f dB",
                    ae_config->rendering_config.element_gain_offset_db.max);
            }
          }

          // Display parameters if they exist
          if (ae_config->rendering_config.parameters) {
            uint32_t num_params =
                array_size(ae_config->rendering_config.parameters);
            debug("        Number of parameters: %u", num_params);
            for (uint32_t k = 0; k < num_params; ++k) {
              parameter_base_t *param = (parameter_base_t *)def_value_wrap_ptr(
                  array_at(ae_config->rendering_config.parameters, k));
              if (param) parameter_base_display(param);
            }
          } else {
            debug("        No parameters.");
          }

          // Display element mix gain
          debug("      Element Mix Gain:");
          parameter_base_display(
              def_param_base_ptr(&ae_config->element_mix_gain));

          // Display localized element annotations
          if (ae_config->localized_element_annotations) {
            uint32_t num_local_ann =
                array_size(ae_config->localized_element_annotations);
            debug("      Number of localized element annotations: %u",
                  num_local_ann);
            for (uint32_t k = 0; k < num_local_ann; ++k) {
              char *local_ann = (char *)def_value_wrap_ptr(
                  array_at(ae_config->localized_element_annotations, k));
              debug("      Localized Element Annotation [%u]: %s", k,
                    local_ann ? local_ann : "NULL");
            }
          } else {
            debug("      No localized element annotations.");
          }
        }
      } else {
        debug("    No audio element configs.");
      }

      debug("    Output Mix Gain:");
      parameter_base_display(def_param_base_ptr(&sub_mix->output_mix_gain));

      if (sub_mix->loudness_layouts && sub_mix->loudness) {
        uint32_t num_layouts = array_size(sub_mix->loudness_layouts);
        if (num_layouts != array_size(sub_mix->loudness)) {
          warning("    Mismatch in loudness_layouts and loudness array sizes.");
        } else {
          debug("    Number of loudness layouts: %u", num_layouts);
          for (uint32_t j = 0; j < num_layouts; ++j) {
            iamf_layout_t *layout = (iamf_layout_t *)def_value_wrap_ptr(
                array_at(sub_mix->loudness_layouts, j));
            obu_loudness_info_t *loudness_info =
                (obu_loudness_info_t *)def_value_wrap_ptr(
                    array_at(sub_mix->loudness, j));

            debug("    Loudness Layout [%u]:", j);
            if (layout) {
              debug("      type: %u (%s)", layout->type,
                    iamf_layout_type_string(layout->type));
              if (layout->type ==
                  ck_iamf_layout_type_loudspeakers_ss_convention) {
                debug("      sound_system: %u (%s)", layout->sound_system,
                      iamf_sound_system_string(layout->sound_system));
              }
            } else {
              warning("    Loudness Layout [%u] is NULL.", j);
            }

            debug("    Loudness Info [%u]:", j);
            if (loudness_info) {
              debug("      info_type: %u (%s)", loudness_info->info_type,
                    iamf_loudness_info_type_string(loudness_info->info_type));
              debug("      integrated_loudness: %d (%f LKFS)",
                    loudness_info->integrated_loudness,
                    iamf_gain_q78_to_db(loudness_info->integrated_loudness));
              debug("      digital_peak: %d (%f dBFS)",
                    loudness_info->digital_peak,
                    iamf_gain_q78_to_db(loudness_info->digital_peak));
              if (loudness_info->info_type & def_loudness_info_type_true_peak) {
                debug("      true_peak: %d (%f dBFS)", loudness_info->true_peak,
                      iamf_gain_q78_to_db(loudness_info->true_peak));
              }
              if (loudness_info->info_type & def_loudness_info_type_anchored &&
                  loudness_info->anchored_loudnesses) {
                uint32_t num_anchored =
                    array_size(loudness_info->anchored_loudnesses);
                debug("      Number of anchored loudness: %u", num_anchored);
                for (uint32_t k = 0; k < num_anchored; ++k) {
                  obu_anchored_loudness_info_t *anchor =
                      def_value_wrap_type_ptr(
                          obu_anchored_loudness_info_t,
                          array_at(loudness_info->anchored_loudnesses, k));
                  if (anchor) {
                    debug(
                        "      Anchored Loudness [%u]: anchor_element=%u (%s), "
                        "anchored_loudness=%d (%f LKFS)",
                        k, anchor->anchor_element,
                        iamf_anchor_element_string(anchor->anchor_element),
                        anchor->anchored_loudness,
                        iamf_gain_q78_to_db(anchor->anchored_loudness));
                  }
                }
              }
              if (loudness_info->info_type & def_loudness_info_type_live) {
                debug("      Live loudness.");
              }
              if (loudness_info->info_type & def_loudness_info_type_momentary) {
                debug("      Momentary loudness:");

                parameter_base_display(
                    def_param_base_ptr(&loudness_info->momentary_loudness
                                            .momentary_loudness_param));

                debug(
                    "        num_bin_pairs_minus_one: %u",
                    loudness_info->momentary_loudness.num_bin_pairs_minus_one);
                debug("        bin_width_minus_one: %u",
                      loudness_info->momentary_loudness.bin_width_minus_one);
                debug("        first_bin_center: %u",
                      loudness_info->momentary_loudness.first_bin_center);
                if (loudness_info->momentary_loudness.bin_count) {
                  uint32_t num_bins =
                      array_size(loudness_info->momentary_loudness.bin_count);
                  debug("        Number of bins: %u", num_bins);
                  for (uint32_t k = 0; k < num_bins; ++k) {
                    uint32_t bin_count = def_value_wrap_u32(array_at(
                        loudness_info->momentary_loudness.bin_count, k));
                    debug("        Bin [%u]: %u", k, bin_count);
                  }
                }
              }
              if (loudness_info->info_type & def_loudness_info_type_range) {
                debug("      loudness_range: %u",
                      loudness_info->loudness_range);
              }
            } else {
              warning("    Loudness Info [%u] is NULL.", j);
            }
          }
        }
      } else {
        debug("    No loudness layouts or loudness info.");
      }
    }
  } else {
    debug("  No sub-mixes.");
  }

  if (obu->mix_presentation_tags) {
    uint32_t num_tags = array_size(obu->mix_presentation_tags);
    debug("  Number of mix presentation tags: %u", num_tags);
    for (uint32_t i = 0; i < num_tags; ++i) {
      iamf_tag_t *tag = (iamf_tag_t *)def_value_wrap_ptr(
          array_at(obu->mix_presentation_tags, i));
      if (!tag) {
        warning("  Mix presentation tag [%u] is NULL.", i);
        continue;
      }
      debug("  Mix Presentation Tag [%u]: name='%s', value='%s'", i, tag->name,
            tag->value);
    }
  } else {
    debug("  No mix presentation tags.");
  }
  debug("Finished displaying Mix Presentation OBU.");
}

int _obu_mp_read_n_strings(io_context_t *ior, uint32_t count, array_t *array) {
  io_context_t *r = ior;
  string128_t str;
  for (uint32_t i = 0; i < count; ++i) {
    value_wrap_t *v = 0;
    string128_t *s = def_mallocz(string128_t, 1);

    v = array_at(array, i);
    if (!s) {
      def_err_msg_enomem("string128", def_mp_str);
      s = &str;
    } else {
      v->ptr = s;
    }

    ior_string(r, (char *)s, sizeof(string128_t));
  }
  return IAMF_OK;
}

static int _obu_mp_rendering_config_init(obu_rendering_config_t *config,
                                         io_context_t *ior) {
  io_context_t *r = ior;
  uint32_t val;
  int ret = IAMF_OK;

  val = ior_8(r);

  config->headphones_rendering_mode = val >> 6 & 0x3;
  if (val >> 5 & 0x1)
    config->flags |= def_rendering_config_flag_element_gain_offset;
  config->binaural_filter_profile = val >> 3 & 0x03;
  config->rendering_config_extension_size = ior_leb128_u32(r);
  if (config->rendering_config_extension_size > 0) {
    uint32_t pos = ioc_tell(ior);
    uint32_t num_parameters = ior_leb128_u32(r);
    if (num_parameters > 0) {
      config->parameters = array_new(num_parameters);
      info("Rendering config parameters(%u)", num_parameters);
      if (!config->parameters) {
        def_err_msg_enomem("parameters", def_mp_str);
        ret = IAMF_ERR_ALLOC_FAIL;
      } else {
        for (uint32_t j = 0; j < num_parameters; ++j) {
          value_wrap_t *v = 0;
          iamf_parameter_type_t param_definition_type = ior_leb128_u32(r);
          debug("Parameter definition type %u", param_definition_type);
          if (iamf_parameter_type_is_coordinate(param_definition_type)) {
            parameter_base_t *param =
                iamf_parameter_base_new(r, param_definition_type);
            if (!param) {
              def_err_msg_enomem("parameter", def_mp_str);
              ret = IAMF_ERR_ALLOC_FAIL;
              break;
            }
            v = array_at(config->parameters, j);
            v->ptr = param;
          } else {
            uint32_t rendering_config_params_extension_size = ior_leb128_u32(r);
            ior_skip(r, rendering_config_params_extension_size);
          }
        }
      }
    }
    if (config->flags & def_rendering_config_flag_element_gain_offset) {
      config->element_gain_offset_type = ior_8(r);
      if (config->element_gain_offset_type ==
          ck_element_gain_offset_type_value) {
        config->element_gain_offset.value = (int16_t)ior_b16(r);

        debug("encoded element gain offset value 0x%04x",
              config->element_gain_offset.value);

        config->element_gain_offset_db.value =
            config->element_gain_offset_db.min =
                config->element_gain_offset_db.max =
                    iamf_gain_q78_to_db(config->element_gain_offset.value);

        info("element gain offset value %f dB",
             config->element_gain_offset_db.value);
      } else if (config->element_gain_offset_type ==
                 ck_element_gain_offset_type_range) {
        float min, max;
        config->element_gain_offset.value = (int16_t)ior_b16(r);
        config->element_gain_offset.min = (int16_t)ior_b16(r);
        config->element_gain_offset.max = (int16_t)ior_b16(r);

        debug(
            "encoded element gain offset value 0x%04x, min 0x%04x, max 0x%04x",
            def_lsb_16bits(config->element_gain_offset.value),
            def_lsb_16bits(config->element_gain_offset.min),
            def_lsb_16bits(config->element_gain_offset.max));

        min = iamf_gain_q78_to_db(config->element_gain_offset.min);
        if (min > 0.f) min = 0.f;
        max = iamf_gain_q78_to_db(config->element_gain_offset.max);
        if (max < 0.f) max = 0.f;

        config->element_gain_offset_db.value =
            iamf_gain_q78_to_db(config->element_gain_offset.value);
        config->element_gain_offset_db.min =
            min + config->element_gain_offset_db.value;
        config->element_gain_offset_db.max =
            max + config->element_gain_offset_db.value;

        info("element gain offset value %f dB, min %f dB, max %f dB",
             config->element_gain_offset_db.value,
             config->element_gain_offset_db.min,
             config->element_gain_offset_db.max);
      } else {
        config->rendering_config_extension_size = ior_leb128_u32(r);
        ior_skip(r, config->rendering_config_extension_size);
      }
    }
    ior_skip(r,
             config->rendering_config_extension_size - (ioc_tell(ior) - pos));
  }

  info("%s: headphones rendering mode(%u), binaural filter profile(%u)",
       def_mp_str, config->headphones_rendering_mode,
       config->binaural_filter_profile);
  info("%s: skip extension size(%u)", def_mp_str,
       config->rendering_config_extension_size);

  return IAMF_OK;
}

static void _obu_mp_audio_element_config_free(
    obu_audio_element_config_t *config) {
  if (!config) return;
  if (config->localized_element_annotations)
    array_free(config->localized_element_annotations, free);
  if (config->rendering_config.parameters)
    array_free(config->rendering_config.parameters,
               def_default_free_ptr(iamf_parameter_base_free));
  mix_gain_parameter_base_clear(&config->element_mix_gain);
  free(config);
}

static obu_audio_element_config_t *_obu_mp_audio_element_config_new(
    io_context_t *ior, uint32_t count_label) {
  io_context_t *r = ior;
  obu_audio_element_config_t *config =
      def_mallocz(obu_audio_element_config_t, 1);

  if (!config) {
    def_err_msg_enomem("audio element config", def_mp_str);
    return 0;
  }

  config->element_id = ior_leb128_u32(r);
  info("Element id(%u)", config->element_id);
  if (count_label) {
    config->localized_element_annotations = array_new(count_label);
    if (!config->localized_element_annotations) {
      def_err_msg_enomem("localized element annotations", def_mp_str);
      _obu_mp_audio_element_config_free(config);
      return 0;
    }

    _obu_mp_read_n_strings(r, count_label,
                           config->localized_element_annotations);

    for (uint32_t i = 0; i < count_label; ++i) {
      info("Localized element annotation: %s",
           (char *)def_value_wrap_ptr(
               array_at(config->localized_element_annotations, i)));
    }
  }

  _obu_mp_rendering_config_init(&config->rendering_config, r);
  info("%s: element mix gain:", def_mp_str);
  mix_gain_parameter_base_init(&config->element_mix_gain, r);

  return config;
}

static iamf_layout_t *_obu_mp_layout_new(io_context_t *ior) {
  io_context_t *r = ior;
  uint32_t val;
  iamf_layout_t *layout = def_mallocz(iamf_layout_t, 1);

  if (!layout) {
    def_err_msg_enomem("layout", def_mp_str);
    return 0;
  }

  val = ior_8(r);
  layout->type = val >> 6 & 0x03;
  if (layout->type == ck_iamf_layout_type_loudspeakers_ss_convention) {
    layout->sound_system = val >> 2 & 0x0f;
    info("Layout type(%u), %s<%u>", layout->type,
         iamf_sound_system_string(layout->sound_system), layout->sound_system);
  } else {
    info("Layout type(%u)", layout->type);
  }

  return layout;
}

static obu_anchored_loudness_info_t *_obu_mp_anchor_loudness_new(
    io_context_t *ior) {
  io_context_t *r = ior;
  obu_anchored_loudness_info_t *anchor =
      def_mallocz(obu_anchored_loudness_info_t, 1);

  if (!anchor) {
    def_err_msg_enomem("anchor loudness", def_mp_str);
    return 0;
  }

  anchor->anchor_element = ior_8(r);
  anchor->anchored_loudness = (int16_t)ior_b16(r);
  info("Auchor loundess > anchor element(%u), anchored loudness(0x%04x)",
       anchor->anchor_element, anchor->anchored_loudness);

  return anchor;
}

void _obu_mp_loudness_info_free(obu_loudness_info_t *info) {
  if (!info) return;
  if (info->anchored_loudnesses) array_free(info->anchored_loudnesses, free);
  momentary_loudness_parameter_base_clear(
      &info->momentary_loudness.momentary_loudness_param);
  if (info->momentary_loudness.bin_count)
    array_free(info->momentary_loudness.bin_count, 0);
  free(info);
}

static obu_loudness_info_t *_obu_mp_loudness_info_new(io_context_t *ior) {
  io_context_t *r = ior;
  obu_loudness_info_t *loudness = def_mallocz(obu_loudness_info_t, 1);
  if (!loudness) {
    def_err_msg_enomem("loundess info", def_mp_str);
    return 0;
  }

  loudness->info_type = ior_8(r);
  loudness->integrated_loudness = (int16_t)ior_b16(r);
  loudness->digital_peak = (int16_t)ior_b16(r);

  info(
      "Loudness > info type(0x%x), integrated loudness %f dB(0x%04x), "
      "digital peak %f dB(0x%04x)",
      loudness->info_type, iamf_gain_q78_to_db(loudness->integrated_loudness),
      def_lsb_16bits(loudness->integrated_loudness),
      iamf_gain_q78_to_db(loudness->digital_peak),
      def_lsb_16bits(loudness->digital_peak));

  if (loudness->info_type & def_loudness_info_type_true_peak) {
    loudness->true_peak = (int16_t)ior_b16(r);
    info("Loudness > true peak(0x%04x)", loudness->true_peak);
  }

  if (loudness->info_type & def_loudness_info_type_anchored) {
    uint32_t val = ior_8(r);
    info("%s: loudness > number of anchor loudness(%u)", def_mp_str, val);

    if (val) {
      loudness->anchored_loudnesses = array_new(val);
      if (!loudness->anchored_loudnesses) {
        def_err_msg_enomem("anchor loudnesses", def_mp_str);
        _obu_mp_loudness_info_free(loudness);
        return 0;
      }

      for (uint32_t i = 0; i < val; ++i) {
        value_wrap_t *v = 0;
        obu_anchored_loudness_info_t *anchor = _obu_mp_anchor_loudness_new(r);
        if (!anchor) {
          _obu_mp_loudness_info_free(loudness);
          return 0;
        }
        v = array_at(loudness->anchored_loudnesses, i);
        v->ptr = anchor;
      }
    }
  }

  if (loudness->info_type & ~def_loudness_info_type_base) {
    uint32_t size = ior_leb128_u32(r);
    uint32_t s = ioc_tell(r);

    if (loudness->info_type & def_loudness_info_type_momentary) {
      momentary_loudness_parameter_base_init(
          &loudness->momentary_loudness.momentary_loudness_param, r);
      uint32_t val = ior_b16(r);
      loudness->momentary_loudness.num_bin_pairs_minus_one = (val >> 13) & 0x7;
      loudness->momentary_loudness.bin_width_minus_one = (val >> 7) & 0x3f;
      loudness->momentary_loudness.first_bin_center = (val >> 1) & 0x3f;

      val = (loudness->momentary_loudness.num_bin_pairs_minus_one + 1) * 2;
      loudness->momentary_loudness.bin_count = array_new(val);
      for (int i = 0; i < val; ++i) {
        if (!loudness->momentary_loudness.bin_count)
          ior_leb128_u32(r);
        else
          def_value_wrap_u32(array_at(loudness->momentary_loudness.bin_count,
                                      i)) = ior_leb128_u32(r);
      }
    }

    if (loudness->info_type & def_loudness_info_type_range) {
      loudness->loudness_range = ior_8(r);
      loudness->loudness_range >>= 2;
    }

    size = size - (ioc_tell(r) - s);
    if (size > 0) ior_skip(r, size);
  }

  return loudness;
}

static void _obu_mp_sub_mix_free(obu_sub_mix_t *sub) {
  if (!sub) return;

  if (sub->audio_element_configs)
    array_free(sub->audio_element_configs,
               def_default_free_ptr(_obu_mp_audio_element_config_free));
  mix_gain_parameter_base_clear(&sub->output_mix_gain);

  if (sub->loudness_layouts) array_free(sub->loudness_layouts, free);
  if (sub->loudness)
    array_free(sub->loudness, def_default_free_ptr(_obu_mp_loudness_info_free));
  free(sub);
}

obu_sub_mix_t *_obu_mp_sub_mix_new(io_context_t *ior, uint32_t count_label) {
  io_context_t *r = ior;
  uint32_t val;
  obu_sub_mix_t *sub = def_mallocz(obu_sub_mix_t, 1);

  if (!sub) {
    def_err_msg_enomem("sub mix", def_mp_str);
    return 0;
  }

  val = ior_leb128_u32(r);  // num_audio_elements
  info("Number of audio elements(%u): ", val);
  if (val) {
    sub->audio_element_configs = array_new(val);
    if (!sub->audio_element_configs) {
      def_err_msg_enomem("audio element configs", def_mp_str);
      _obu_mp_sub_mix_free(sub);
      return 0;
    }
    for (uint32_t i = 0; i < val; ++i) {
      obu_audio_element_config_t *config =
          _obu_mp_audio_element_config_new(r, count_label);
      if (!config) {
        _obu_mp_sub_mix_free(sub);
        return 0;
      }
      def_value_wrap_ptr(array_at(sub->audio_element_configs, i)) = config;
    }
  }

  info("Output mix gain:");
  if (mix_gain_parameter_base_init(&sub->output_mix_gain, r) != IAMF_OK) {
    _obu_mp_sub_mix_free(sub);
    return 0;
  }

  val = ior_leb128_u32(r);  // num_layouts
  info("Number of layouts(%u)", val);
  if (val) {
    sub->loudness_layouts = array_new(val);
    sub->loudness = array_new(val);

    if (!sub->loudness_layouts || !sub->loudness) {
      if (!sub->loudness_layouts)
        def_err_msg_enomem("loudness layouts", def_mp_str);
      if (!sub->loudness) def_err_msg_enomem("loudness", def_mp_str);
      _obu_mp_sub_mix_free(sub);
      return 0;
    }

    for (uint32_t i = 0; i < val; ++i) {
      obu_loudness_info_t *loudness = 0;
      iamf_layout_t *loudness_layout = _obu_mp_layout_new(r);

      if (!loudness_layout) {
        _obu_mp_sub_mix_free(sub);
        return 0;
      }
      def_value_wrap_ptr(array_at(sub->loudness_layouts, i)) = loudness_layout;

      loudness = _obu_mp_loudness_info_new(r);
      if (!loudness) {
        _obu_mp_sub_mix_free(sub);
        return 0;
      }
      def_value_wrap_ptr(array_at(sub->loudness, i)) = loudness;
    }
  }

  return sub;
}

int _obu_mp_check(iamf_mix_presentation_obu_t *obu) {
  int num = array_size(obu->sub_mixes);

  if (num > def_max_sub_mixes) {
    warning("id(%u): num_sub_mixes should be <= %d.", obu->mix_presentation_id,
            def_max_sub_mixes);
    return def_error;
  }

  for (int i = 0; i < num; ++i) {
    int n;
    obu_sub_mix_t *sub =
        def_value_wrap_optional_ptr(array_at(obu->sub_mixes, i));
    if (!sub) {
      warning("id(%u): sub mix (%d) should not be null.",
              obu->mix_presentation_id, i);
      return def_error;
    }

    n = array_size(sub->audio_element_configs);
    if (!n) {
      warning("id(%u): num_audio_elements should not be set to 0.",
              obu->mix_presentation_id);
      return def_error;
    }

    for (int i = 0; i < n; ++i) {
      obu_audio_element_config_t *aelem_config =
          def_value_wrap_optional_ptr(array_at(sub->audio_element_configs, i));

      if (aelem_config->rendering_config.headphones_rendering_mode >
          HEADPHONES_RENDERING_MODE_HEAD_LOCKED) {
        warning(
            "Invalid headphones rendering mode %d in element %u of mix "
            "presentation %u",
            aelem_config->rendering_config.headphones_rendering_mode,
            aelem_config->element_id, obu->mix_presentation_id);
        return def_error;
      }
    }

    n = array_size(sub->loudness_layouts);
    for (int i = 0; i < n; ++i) {
      iamf_layout_t *layout =
          def_value_wrap_optional_ptr(array_at(sub->loudness_layouts, i));
      if (layout->type == ck_iamf_layout_type_loudspeakers_ss_convention &&
          !iamf_sound_system_check(layout->sound_system)) {
        warning("Find unsupported sound system %d in mix presentation %u.",
                layout->sound_system, obu->mix_presentation_id);
      }
    }
  }

  num = array_size(obu->mix_presentation_tags);
  for (int i = 0; i < num; ++i) {
    iamf_tag_t *tag =
        def_value_wrap_optional_ptr(array_at(obu->mix_presentation_tags, i));
    if (_obu_mp_tag_check(tag->name, tag->value) != def_pass)
      warning("Invalid tag name %s or value %s.", tag->name, tag->value);
  }
  return def_pass;
}

static int _obu_mp_find_audio_element(value_wrap_t a, value_wrap_t b) {
  return ((iamf_audio_element_obu_t *)a.ptr)->audio_element_id == b.u32;
}

static int _obu_mp_tag_check(const char *name, const char *value) {
  static const char *const _content_type_values[] = {"sub-mix2_system_sound"};

  if (strcmp(name, "content_language") == 0) {
    if (strlen(value) != 3) return def_error;
  } else if (strcmp(name, "content_type") == 0) {
    int num_values =
        sizeof(_content_type_values) / sizeof(_content_type_values[0]);
    int is_valid = 0;
    for (int i = 0; i < num_values; ++i) {
      if (strcmp(value, _content_type_values[i]) == 0) {
        is_valid = 1;
        break;
      }
    }
    if (!is_valid) return def_error;
  }
  return def_pass;
}
