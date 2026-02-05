/*
 * Copyright (c) 2025, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomia.org/license/patent.
 */

/**
 * @file audio_element_obu.c
 * @brief Audio element OBU implementation.
 * @version 2.0.0
 * @date Created 12/11/2025
 **/

#include "audio_element_obu.h"

#include "iamf_layout.h"
#include "iamf_private_definitions.h"
#include "iamf_string.h"
#include "parameter_base.h"

#ifdef SUPPORT_VERIFIER
#include "vlogging_tool_sr.h"
#endif

#undef def_log_tag
#define def_log_tag "OBU_AE"
#define def_cco_str "Audio Element OBU"

static int _obu_ae_common_init(iamf_audio_element_obu_t *obu, uint32_t id,
                               iamf_audio_element_type_t type,
                               io_context_t *ior);
static void _obu_ae_common_clear(iamf_audio_element_obu_t *obu);
static channel_based_audio_element_obu_t *_obu_ae_channel_based_new(
    uint32_t id, io_context_t *ior);
static void _obu_ae_channel_based_free(channel_based_audio_element_obu_t *cae);
static scene_based_audio_element_obu_t *_obu_ae_scene_based_new(
    uint32_t id, io_context_t *ior);
static void _obu_ae_scene_based_free(scene_based_audio_element_obu_t *sae);
static object_based_audio_element_obu_t *_obu_ae_object_based_new(
    uint32_t id, io_context_t *ior);
static void _obu_ae_object_based_free(object_based_audio_element_obu_t *oae);

static int _obu_ae_check(iamf_audio_element_obu_t *obu);
static int _obu_ae_channel_based_check_profile(
    channel_based_audio_element_obu_t *cae, iamf_profile_t profile);
static int _obu_ae_scene_based_check_profile(
    scene_based_audio_element_obu_t *sae, iamf_profile_t profile);
static int _obu_ae_find_parameter(value_wrap_t item, value_wrap_t key);

iamf_audio_element_obu_t *iamf_audio_element_obu_new(io_context_t *ior) {
  io_context_t *r = ior;
  iamf_audio_element_obu_t *obu = 0;
  uint32_t val;
  uint32_t type;

  val = ior_leb128_u32(r);      // audio element id
  type = ior_8(r) >> 5 & 0x07;  // audio element type

  if (type == ck_audio_element_type_channel_based) {
    channel_based_audio_element_obu_t *cae =
        _obu_ae_channel_based_new(val, ior);
    if (cae) obu = def_audio_element_obu_ptr(cae);
  } else if (type == ck_audio_element_type_scene_based) {
    scene_based_audio_element_obu_t *sae = _obu_ae_scene_based_new(val, ior);
    if (sae) obu = def_audio_element_obu_ptr(sae);
  } else if (type == ck_audio_element_type_object_based) {
    object_based_audio_element_obu_t *oae = _obu_ae_object_based_new(val, ior);
    if (oae) obu = def_audio_element_obu_ptr(oae);
  } else {
    warning("Unsupported type %u of audio element %u", type, val);
    return 0;
  }

  if (!obu) {
    def_err_msg_enomem(def_cco_str, def_iamf_ustr);
    return 0;
  }

#if SUPPORT_VERIFIER
  vlog_obu(ck_iamf_obu_audio_element, obu, 0, 0);
#endif

  if (_obu_ae_check(obu) != def_pass) {
    iamf_audio_element_obu_free(obu);
    return 0;
  }

  return obu;
}

void iamf_audio_element_obu_free(iamf_audio_element_obu_t *obu) {
  if (!obu) return;

  if (obu->audio_element_type == ck_audio_element_type_channel_based) {
    _obu_ae_channel_based_free(def_channel_based_audio_element_obu_ptr(obu));
  } else if (obu->audio_element_type == ck_audio_element_type_scene_based) {
    _obu_ae_scene_based_free(def_scene_based_audio_element_obu_ptr(obu));
  } else if (obu->audio_element_type == ck_audio_element_type_object_based) {
    _obu_ae_object_based_free(def_object_based_audio_element_obu_ptr(obu));
  } else {
    warning("Unknown audio element type %u", obu->audio_element_type);
    _obu_ae_common_clear(obu);
  }
}

int iamf_audio_element_obu_check_profile(iamf_audio_element_obu_t *obu,
                                         iamf_profile_t profile) {
  if (obu->audio_element_type == ck_audio_element_type_channel_based)
    return _obu_ae_channel_based_check_profile(
        def_channel_based_audio_element_obu_ptr(obu), profile);
  if (obu->audio_element_type == ck_audio_element_type_scene_based)
    return _obu_ae_scene_based_check_profile(
        def_scene_based_audio_element_obu_ptr(obu), profile);
  if (obu->audio_element_type == ck_audio_element_type_object_based &&
      profile > ck_iamf_profile_base_enhanced)
    return def_pass;
  return def_error;
}

parameter_base_t *iamf_audio_element_obu_get_parameter(
    iamf_audio_element_obu_t *obu, iamf_parameter_type_t type) {
  value_wrap_t *v = 0;

  if (!obu || !obu->parameters) return 0;
  if (type != ck_iamf_parameter_type_demixing &&
      type != ck_iamf_parameter_type_recon_gain)
    return 0;

  v = array_find(obu->parameters, def_value_wrap_instance_u32(type),
                 _obu_ae_find_parameter);
  return v ? v->ptr : 0;
}

int _obu_ae_common_init(iamf_audio_element_obu_t *obu, uint32_t id,
                        iamf_audio_element_type_t type, io_context_t *ior) {
  uint32_t val = 0;
  io_context_t *r = ior;

  obu->obu.obu_type = ck_iamf_obu_audio_element;
  obu->audio_element_id = id;
  obu->audio_element_type = type;
  obu->codec_config_id = ior_leb128_u32(r);

  val = ior_leb128_u32(r);  // num_substreams
  info(
      "Audio element id(%u), audio element type(%s<%d>), "
      "codec config id(%u), sub-streams count(%u)",
      obu->audio_element_id,
      iamf_audio_element_type_string(obu->audio_element_type),
      obu->audio_element_type, obu->codec_config_id, val);

  obu->audio_substream_ids = array_new(val);
  if (!obu->audio_substream_ids) {
    def_err_msg_enomem("substream ids", def_cco_str);
    return IAMF_ERR_ALLOC_FAIL;
  }

  info("Sub-stream ids:");
  for (uint32_t i = 0; i < val; ++i) {
    value_wrap_t *v = array_at(obu->audio_substream_ids, i);
    v->u32 = ior_leb128_u32(r);
    info("\t%u", v->u32);
  }

  val = ior_leb128_u32(r);  // num_parameters
  info("Element parameters count %u", val);
  if (val) {
    obu->parameters = array_new(val);
    if (!obu->parameters) {
      def_err_msg_enomem("parameters", def_cco_str);
      return IAMF_ERR_ALLOC_FAIL;
    }
  }

  for (uint32_t i = 0; i < val; ++i) {
    value_wrap_t *v = 0;
    type = ior_leb128_u32(r);
    if (type != ck_iamf_parameter_type_demixing &&
        type != ck_iamf_parameter_type_recon_gain) {
      uint32_t size = ior_leb128_u32(r);
      ior_skip(r, size);
      warning(
          "Don't support parameter type(%u) in Audio Element OBU(%u), "
          "parameter definition bytes %u.",
          type, obu->audio_element_id, size);
      continue;
    }

    v = array_at(obu->parameters, i);
    v->ptr = iamf_parameter_base_new(r, type);
    if (!v->ptr) {
      def_err_msg_enomem("parameter", def_cco_str);
      continue;
    }
  }

  return IAMF_OK;
}

void _obu_ae_common_clear(iamf_audio_element_obu_t *obu) {
  if (obu->audio_substream_ids) array_free(obu->audio_substream_ids, 0);
  if (obu->parameters)
    array_free(obu->parameters, def_default_free_ptr(iamf_parameter_base_free));
}

static obu_channel_layer_config_t *_obu_ae_channel_layout_config_new(
    io_context_t *ior, uint32_t layer) {
  io_context_t *r = ior;
  uint32_t val;
  obu_channel_layer_config_t *layer_config =
      def_mallocz(obu_channel_layer_config_t, 1);

  if (!layer_config) {
    def_err_msg_enomem("channel layer config", def_cco_str);
    return 0;
  }

  val = ior_8(r);
  layer_config->loudspeaker_layout = val >> 4 & 0x0f;
  layer_config->output_gain_is_present_flag = val >> 3 & 0x01;
  layer_config->recon_gain_is_present_flag = val >> 2 & 0x01;
  layer_config->substream_count = ior_8(r);
  layer_config->coupled_substream_count = ior_8(r);

  info(
      "\tLayer[%u] info: loudspeaker layout(%u:%s), "
      "output gain is present flag(%u), recon gain is present flag(%u), "
      "substreams count(%u), coupled substreams count(%u)",
      layer, layer_config->loudspeaker_layout,
      iamf_loudspeaker_layout_string(layer_config->loudspeaker_layout),
      layer_config->output_gain_is_present_flag,
      layer_config->recon_gain_is_present_flag, layer_config->substream_count,
      layer_config->coupled_substream_count);

  if (layer_config->output_gain_is_present_flag) {
    layer_config->output_gain_info.output_gain_flag = ior_8(r) >> 2 & 0x3f;
    layer_config->output_gain_info.output_gain = (int16_t)ior_b16(r);
    layer_config->output_gain_info.output_gain_linear =
        iamf_gain_q78_to_linear(layer_config->output_gain_info.output_gain);
    info(
        "\t\tOutput gain : output gain flag(0x%04x), output gain(0x%04x), "
        "linear gain(%f)",
        layer_config->output_gain_info.output_gain_flag,
        layer_config->output_gain_info.output_gain,
        layer_config->output_gain_info.output_gain_linear);
  }

  if (!layer && layer_config->loudspeaker_layout ==
                    def_iamf_loudspeaker_layout_expanded) {
    layer_config->expanded_loudspeaker_layout = ior_8(r);
    info("\t\tExpanded loudspeaker layout(%u:%s)",
         layer_config->expanded_loudspeaker_layout,
         iamf_expanded_loudspeaker_layout_string(
             layer_config->expanded_loudspeaker_layout));
  }

  return layer_config;
}

static int _obu_ae_channel_based_init(channel_based_audio_element_obu_t *cae,
                                      io_context_t *ior) {
  io_context_t *r = ior;
  uint32_t val;

  val = ior_8(r) >> 5 & 0x07;  // num_layers
  if (!val) return IAMF_ERR_BAD_ARG;

  cae->channel_audio_layer_configs = array_new(val);
  if (!cae->channel_audio_layer_configs) {
    def_err_msg_enomem("channel layer configs", def_cco_str);
    return IAMF_ERR_ALLOC_FAIL;
  }
  info("Scalable channel layout config, number of layers %u", val);

  for (uint32_t i = 0; i < val; ++i) {
    obu_channel_layer_config_t *layer_config =
        _obu_ae_channel_layout_config_new(r, i);
    if (!layer_config) continue;
    def_value_wrap_ptr(array_at(cae->channel_audio_layer_configs, i)) =
        layer_config;
  }
  return IAMF_OK;
}

channel_based_audio_element_obu_t *_obu_ae_channel_based_new(
    uint32_t id, io_context_t *ior) {
  channel_based_audio_element_obu_t *cae = 0;
  iamf_audio_element_obu_t *obu = 0;

  cae = def_mallocz(channel_based_audio_element_obu_t, 1);
  if (!cae) {
    def_err_msg_enomem("Channel Based Audio Element OBU", def_iamf_ustr);
    return 0;
  }

  obu = def_audio_element_obu_ptr(cae);

  if (_obu_ae_common_init(obu, id, ck_audio_element_type_channel_based, ior) !=
          IAMF_OK ||
      _obu_ae_channel_based_init(cae, ior) != IAMF_OK) {
    _obu_ae_channel_based_free(cae);
    return 0;
  }

  return cae;
}

void _obu_ae_channel_based_free(channel_based_audio_element_obu_t *cae) {
  if (!cae) return;
  _obu_ae_common_clear(&cae->base);
  if (cae->channel_audio_layer_configs)
    array_free(cae->channel_audio_layer_configs, def_default_free_ptr(free));
  free(cae);
}

static int _obu_ae_scene_based_init(scene_based_audio_element_obu_t *sae,
                                    io_context_t *ior) {
  int ret = IAMF_OK;
  io_context_t *r = ior;

  sae->ambisonics_mode = ior_leb128_u32(r);
  if (sae->ambisonics_mode == ck_ambisonics_mode_mono) {
    sae->output_channel_count = ior_8(r);
    sae->substream_count = ior_8(r);
    sae->mapping_size = sae->output_channel_count;
    sae->channel_mapping = def_malloc(uint8_t, sae->mapping_size);
    if (!sae->channel_mapping) {
      def_err_msg_enomem("mono mapping", def_cco_str);
      return IAMF_ERR_ALLOC_FAIL;
    }
    ior_read(r, sae->channel_mapping, sae->output_channel_count);

    info(
        "%s(%u), output channel count(%u), substream count(%u), channel "
        "mapping "
        "size %u",
        iamf_ambisonics_mode_string(sae->ambisonics_mode), sae->ambisonics_mode,
        sae->output_channel_count, sae->substream_count,
        sae->output_channel_count);

  } else if (sae->ambisonics_mode == ck_ambisonics_mode_projection) {
    sae->output_channel_count = ior_8(r);
    sae->substream_count = ior_8(r);
    sae->coupled_substream_count = ior_8(r);
    sae->mapping_size = (sae->substream_count + sae->coupled_substream_count) *
                        sae->output_channel_count;
    sae->mapping_size *= sizeof(int16_t);
    sae->demixing_matrix = def_malloc(uint8_t, sae->mapping_size);
    if (!sae->demixing_matrix) {
      def_err_msg_enomem("projection mapping", def_cco_str);
      return IAMF_ERR_ALLOC_FAIL;
    }

    ior_read(r, sae->demixing_matrix, sae->mapping_size);

    debug(
        "%s<%u>, output channel count(%u), substream count(%u), "
        "coupled substream count(%u), demixing matrix (%u x %u) size(%u) ",
        iamf_ambisonics_mode_string(sae->ambisonics_mode), sae->ambisonics_mode,
        sae->output_channel_count, sae->substream_count,
        sae->coupled_substream_count, sae->output_channel_count,
        sae->substream_count + sae->coupled_substream_count, sae->mapping_size);

  } else {
    warning("Invalid ambisonics mode(%u)", sae->ambisonics_mode);
    ret = IAMF_ERR_BAD_ARG;
  }
  return ret;
}

scene_based_audio_element_obu_t *_obu_ae_scene_based_new(uint32_t id,
                                                         io_context_t *ior) {
  scene_based_audio_element_obu_t *sae = 0;

  sae = def_mallocz(scene_based_audio_element_obu_t, 1);
  if (!sae) {
    def_err_msg_enomem("Scene Based Audio Element OBU", def_iamf_ustr);
    return 0;
  }

  if (_obu_ae_common_init(&sae->base, id, ck_audio_element_type_scene_based,
                          ior) != IAMF_OK ||
      _obu_ae_scene_based_init(sae, ior) != IAMF_OK) {
    _obu_ae_scene_based_free(sae);
    return 0;
  }
  return sae;
}

void _obu_ae_scene_based_free(scene_based_audio_element_obu_t *sae) {
  _obu_ae_common_clear(&sae->base);
  def_free(sae->channel_mapping);
  def_free(sae);
}

static int _obu_ae_object_based_init(object_based_audio_element_obu_t *oae,
                                     io_context_t *ior) {
  io_context_t *r = ior;
  uint32_t val = ior_leb128_u32(r);

  if (val > 0) {
    oae->num_objects = ior_8(r);
    ior_skip(r, val - 1);
  }
  info("Number of objects %u", oae->num_objects);

  return oae->num_objects > 0 ? IAMF_OK : IAMF_ERR_BAD_ARG;
}

object_based_audio_element_obu_t *_obu_ae_object_based_new(uint32_t id,
                                                           io_context_t *ior) {
  object_based_audio_element_obu_t *oae = 0;

  oae = def_mallocz(object_based_audio_element_obu_t, 1);
  if (!oae) {
    def_err_msg_enomem("Object Based Audio Element OBU", def_iamf_ustr);
    return 0;
  }
  if (_obu_ae_common_init(&oae->base, id, ck_audio_element_type_object_based,
                          ior) != IAMF_OK ||
      _obu_ae_object_based_init(oae, ior) != IAMF_OK) {
    _obu_ae_object_based_free(oae);
    return 0;
  }
  debug("New object_based_audio_element_obu_t %p", oae);
  return oae;
}

void _obu_ae_object_based_free(object_based_audio_element_obu_t *oae) {
  _obu_ae_common_clear(&oae->base);
  def_free(oae);
}

static int _obu_ae_channel_based_check(channel_based_audio_element_obu_t *cae) {
  if (array_size(cae->base.parameters) > 2)
    warning("Channle based audio element(%u) has more than two parameters.",
            cae->base.audio_element_id);

  cae->max_valid_layers = 0;
  if (cae->channel_audio_layer_configs) {
    uint32_t channels = 0;
    int n = array_size(cae->channel_audio_layer_configs);
    if (!n) {
      warning("Element (%u) doesn't have valid layer.",
              cae->base.audio_element_id);
      return def_error;
    }

    for (int i = 0; i < n; ++i) {
      obu_channel_layer_config_t *layer_config = def_value_wrap_optional_ptr(
          array_at(cae->channel_audio_layer_configs, i));

      if (cae->max_valid_layers == i) {
        channels += (layer_config->substream_count +
                     layer_config->coupled_substream_count);
        if (iamf_audio_layer_base_layout_check(
                layer_config->loudspeaker_layout) &&
            (layer_config->substream_count > 0) &&
            (iamf_loudspeaker_layout_get_info(layer_config->loudspeaker_layout)
                 ->channels == channels)) {
          ++cae->max_valid_layers;
        } else if (!i && n == 1 &&
                   layer_config->loudspeaker_layout ==
                       def_iamf_loudspeaker_layout_expanded) {
          if (iamf_audio_layer_expanded_layout_check(
                  layer_config->expanded_loudspeaker_layout)) {
            cae->max_valid_layers = 1;
          } else {
            warning("Layer[%d] info : invalid expanded layout %d", i,
                    cae->max_valid_layers);
          }
        } else if (i || layer_config->loudspeaker_layout !=
                            def_iamf_loudspeaker_layout_expanded) {
          warning("Element (%u) Layer %d: Invalid loudspeaker layout %u",
                  cae->base.audio_element_id, i,
                  layer_config->loudspeaker_layout);
          break;
        }
      }
    }
    info("Valid layers %u", cae->max_valid_layers);
  }
  return cae->max_valid_layers ? def_pass : def_error;
}

int _obu_ae_channel_based_check_profile(channel_based_audio_element_obu_t *cae,
                                        iamf_profile_t profile) {
  array_t *configs = cae->channel_audio_layer_configs;

  if (array_size(configs) == 1 && profile <= ck_iamf_profile_base) {
    obu_channel_layer_config_t *layer_config =
        def_value_wrap_optional_ptr(array_at(configs, 0));
    if (layer_config &&
        !iamf_audio_layer_base_layout_check(layer_config->loudspeaker_layout)) {
      warning("Element (%u) doesn't support layout %u in profile %u.",
              cae->base.audio_element_id, layer_config->loudspeaker_layout,
              profile);
      return def_error;
    }
  }
  return def_pass;
}

static int _obu_ae_scene_based_check(scene_based_audio_element_obu_t *sae) {
  int channels = sae->substream_count + sae->coupled_substream_count;

  if (array_size(sae->base.parameters) > 0)
    warning("Scene based audio element(%u) has parameter.",
            sae->base.audio_element_id);

  if (iamf_ambisionisc_get_order(sae->output_channel_count) < 0 ||
      sae->output_channel_count < channels) {
    warning(
        "Invalid output channel count %d or invalid input channels %d in "
        "ambisonics mode",
        sae->output_channel_count, channels);
    return def_error;
  }
  return def_pass;
}

static int _obu_ae_object_based_check(object_based_audio_element_obu_t *oae) {
  if (array_size(oae->base.parameters) > 0)
    warning("Object based audio element(%u) has parameter.",
            oae->base.audio_element_id);

  if (oae->num_objects <= 0) {
    warning("Object based audio element(%u) has no objects.",
            oae->base.audio_element_id);
    return def_error;
  }
  return def_pass;
}

int _obu_ae_scene_based_check_profile(scene_based_audio_element_obu_t *sae,
                                      iamf_profile_t profile) {
  int channels = sae->substream_count + sae->coupled_substream_count;
  const iamf_profile_info_t *info = iamf_profile_info_get(profile);
  if (channels > info->max_amb_channels) {
    warning("Invalid channels %d in ambisonics mode, it should less than %d",
            channels, info->max_amb_channels);
    return def_error;
  }
  return def_pass;
}

int _obu_ae_check(iamf_audio_element_obu_t *obu) {
  if (obu->audio_element_type == ck_audio_element_type_channel_based)
    return _obu_ae_channel_based_check(
        def_channel_based_audio_element_obu_ptr(obu));
  if (obu->audio_element_type == ck_audio_element_type_scene_based)
    return _obu_ae_scene_based_check(
        def_scene_based_audio_element_obu_ptr(obu));
  if (obu->audio_element_type == ck_audio_element_type_object_based)
    return _obu_ae_object_based_check(
        def_object_based_audio_element_obu_ptr(obu));
  return def_error;
}

int _obu_ae_find_parameter(value_wrap_t item, value_wrap_t key) {
  return item.ptr
             ? def_value_wrap_type_ptr(parameter_base_t, &item)->type == key.u32
             : 0;
}
