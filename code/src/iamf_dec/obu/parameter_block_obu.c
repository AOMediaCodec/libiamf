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
 * @file parameter_block_obu.c
 * @brief Parameter block OBU implementation.
 * @version 2.0.0
 * @date Created 12/11/2025
 **/

#include "parameter_block_obu.h"

#include "iamf_private_definitions.h"
#include "iamf_string.h"
#include "iamf_utils.h"

#ifdef SUPPORT_VERIFIER
#include "vlogging_tool_sr.h"
#endif

#undef def_log_tag
#define def_log_tag "OBU_PBK"
#define def_pbo_str "Parameter Block OBU"

#define def_iamf_u32_to_i16_to_f32(v, bits) \
  iamf_i16_to_f32(iamf_u32_to_i16((v), (bits)), (bits))

static parameter_subblock_t *_obu_pb_subblock_new(
    io_context_t *, parameter_base_t *, uint32_t,
    iamf_pbo_extra_interfaces_t *);
static void _obu_pb_recon_gain_info_parameter_subblock_free(
    recon_gain_parameter_subblock_t *);

iamf_parameter_block_obu_t *iamf_parameter_block_obu_new(
    io_context_t *ior, iamf_pbo_extra_interfaces_t *interfaces) {
  iamf_parameter_block_obu_t *obu = 0;
  parameter_base_t *base = 0;
  io_context_t *r = ior;
  uint32_t parameter_id;
  uint32_t num_subblocks = 0;

  if (!interfaces || !interfaces->get_parameter_base) {
    warning("invalid input parameters: interfaces(%p), get base(%p)",
            interfaces, interfaces ? interfaces->get_parameter_base : 0);
    return 0;
  }

  parameter_id = ior_leb128_u32(r);
  base = interfaces->get_parameter_base(interfaces->this, parameter_id);
  if (!base) {
    debug("failed to get parameter base %u from extra", parameter_id);
    return 0;
  }

  obu = def_mallocz(iamf_parameter_block_obu_t, 1);
  if (!obu) {
    def_err_msg_enomem(def_pbo_str, def_iamf_ustr);
    return 0;
  }

  obu->obu.obu_type = ck_iamf_obu_parameter_block;
  obu->base = base;

  if (obu->base->param_definition_mode) {
    obu->duration = ior_leb128_u32(r);
    obu->constant_subblock_duration = ior_leb128_u32(r);
    if (!obu->constant_subblock_duration) num_subblocks = ior_leb128_u32(r);
  } else {
    obu->duration = base->duration;
    obu->constant_subblock_duration = base->constant_subblock_duration;
    if (!obu->constant_subblock_duration)
      num_subblocks = array_size(base->subblock_durations);
  }

  if (obu->constant_subblock_duration)
    num_subblocks = obu->duration / obu->constant_subblock_duration +
                    (obu->duration % obu->constant_subblock_duration ? 1 : 0);

  info(
      "Parameter type(%s<%u>), id(%u) , rate(%u), mode(%u), duration(%u), "
      "constant subblock duration(%u), number of subblocks(%u)",
      iamf_parameter_block_type_string(base->type), base->type,
      base->parameter_id, base->parameter_rate, base->param_definition_mode,
      obu->duration, obu->constant_subblock_duration, num_subblocks);

  obu->subblocks = array_new(num_subblocks);
  if (!obu->subblocks) {
    def_err_msg_enomem("subblocks", def_pbo_str);
    iamf_parameter_block_obu_free(obu);
    return 0;
  }

  for (uint32_t i = 0; i < num_subblocks; ++i) {
    parameter_subblock_t *subblock = 0;
    uint32_t subblock_duration = 0;

    if (!obu->constant_subblock_duration) {
      if (base->param_definition_mode) {
        subblock_duration = ior_leb128_u32(r);
      } else {
        subblock_duration = array_at(base->subblock_durations, i)->u32;
      }
    } else {
      if (i == num_subblocks - 1 &&
          obu->duration % obu->constant_subblock_duration)
        subblock_duration = obu->duration % obu->constant_subblock_duration;
      else
        subblock_duration = obu->constant_subblock_duration;
    }

    info("Subblock(%d), duration(%u)", i, subblock_duration);

    subblock = _obu_pb_subblock_new(r, base, subblock_duration, interfaces);
    if (!subblock) {
      iamf_parameter_block_obu_free(obu);
      return 0;
    }

    array_at(obu->subblocks, i)->ptr = subblock;
  }

#if SUPPORT_VERIFIER
  vlog_obu(ck_iamf_obu_parameter_block, obu, 0, 0);
#endif

  iamf_parameter_block_obu_display(obu);

  return obu;
}

void iamf_parameter_block_obu_free(iamf_parameter_block_obu_t *obu) {
  if (!obu) return;
  if (obu->subblocks)
    array_free(obu->subblocks,
               def_default_free_ptr(iamf_parameter_subblock_free));
  free(obu);
}

void iamf_parameter_subblock_free(parameter_subblock_t *subblock) {
  if (!subblock) return;

  switch (subblock->type) {
    case ck_iamf_parameter_type_mix_gain:
    case ck_iamf_parameter_type_demixing:
    case ck_iamf_parameter_type_polar:
    case ck_iamf_parameter_type_dual_polar:
    case ck_iamf_parameter_type_cartesian_8:
    case ck_iamf_parameter_type_cartesian_16:
    case ck_iamf_parameter_type_dual_cartesian_8:
    case ck_iamf_parameter_type_dual_cartesian_16:
    case ck_iamf_parameter_type_momentary_loudness:
      free(subblock);
      break;

    case ck_iamf_parameter_type_recon_gain:
      _obu_pb_recon_gain_info_parameter_subblock_free(
          (recon_gain_parameter_subblock_t *)subblock);
      break;

    default:
      break;
  }
}

static int _obu_pb_animated_parameter_gain_db(animated_float32_t *dst,
                                              ap_u32_t *src) {
  if (!dst || !src) return IAMF_ERR_BAD_ARG;

  dst->animation_type = src->animation_type;
  dst->data.start =
      iamf_gain_q78_to_db(iamf_u32_to_i16(src->data.start, def_q78_num_bits));
  dst->data.end =
      iamf_gain_q78_to_db(iamf_u32_to_i16(src->data.end, def_q78_num_bits));
  dst->data.control =
      iamf_gain_q78_to_db(iamf_u32_to_i16(src->data.control, def_q78_num_bits));
  dst->data.control_relative_time =
      iamf_divide_128f(src->data.control_relative_time);

  return IAMF_OK;
}

static mix_gain_parameter_subblock_t *_obu_pb_mix_gain_parameter_subblock_new(
    io_context_t *ior, uint32_t duration) {
  io_context_t *r = ior;
  bits_io_context_t bits_ioc, *bits_r;
  mix_gain_parameter_subblock_t *data =
      def_mallocz(mix_gain_parameter_subblock_t, 1);

  if (!data) {
    def_err_msg_enomem("mix gain parameter data", def_pbo_str);
    return 0;
  }

  bits_r = &bits_ioc;

  data->base.type = ck_iamf_parameter_type_mix_gain;
  data->base.subblock_duration = duration;
  data->gain.animation_type = ior_leb128_u32(r);

  bits_ioc_init(bits_r, r);
  animated_parameter_data_read_bits(
      bits_r, def_q78_num_bits, data->gain.animation_type, &data->gain.data);
  _obu_pb_animated_parameter_gain_db(&data->gain_db, &data->gain);
  info(
      "  Mix gain: animated type(%u), start(%f), end(%f), "
      "control(%f), control relative time(%f).",
      data->gain_db.animation_type, data->gain_db.data.start,
      data->gain_db.data.end, data->gain_db.data.control,
      data->gain_db.data.control_relative_time);

  debug("New mix gain parameter %p ", data);
  return data;
}

static demixing_info_parameter_subblock_t *
_obu_pb_demixing_info_parameter_subblock_new(io_context_t *ior,
                                             uint32_t duration) {
  io_context_t *r = ior;
  demixing_info_parameter_subblock_t *data =
      def_mallocz(demixing_info_parameter_subblock_t, 1);

  if (!data) {
    def_err_msg_enomem("demixing info parameter data", def_pbo_str);
    return 0;
  }

  data->base.type = ck_iamf_parameter_type_demixing;
  data->base.subblock_duration = duration;
  data->demixing_mode = ior_8(r) >> 5 & 0x07;
  info("\tDemixing parameter data: mode %u", data->demixing_mode);

  return data;
}

static void _obu_pb_recon_gain_free(recon_gain_t *gain) {
  if (!gain) return;
  if (gain->recon_gain) array_free(gain->recon_gain, 0);
  if (gain->recon_gain_linear) array_free(gain->recon_gain_linear, 0);
  free(gain);
}

static recon_gain_t *_obu_pb_recon_gain_new(io_context_t *ior, int index) {
  io_context_t *r = ior;
  int size = 0;
  recon_gain_t *recon_gain = def_mallocz(recon_gain_t, 1);
  if (!recon_gain) {
    def_err_msg_enomem("recon gain", def_pbo_str);
    return 0;
  }

  recon_gain->layer = index;
  recon_gain->recon_gain_flags = ior_leb128_u32(r);
  size = bit1_count(recon_gain->recon_gain_flags);
  recon_gain->recon_gain = array_new(size);
  recon_gain->recon_gain_linear = array_new(size);
  if (!recon_gain->recon_gain || !recon_gain->recon_gain_linear) {
    def_err_msg_enomem("recon gain values", def_pbo_str);
    _obu_pb_recon_gain_free(recon_gain);
    return 0;
  }

  info("\tRecon gain info: layer %d, flags 0x%08x", index,
       recon_gain->recon_gain_flags);
  for (int i = 0; i < size; ++i) {
    uint32_t val = ior_8(r);
    debug("\t\tChannel %d, gain 0x%02x(%f)", i, val,
          iamf_recon_gain_linear(val));
    array_at(recon_gain->recon_gain, i)->u32 = val;
    array_at(recon_gain->recon_gain_linear, i)->f32 =
        iamf_recon_gain_linear(val);
  }

  return recon_gain;
}

void _obu_pb_recon_gain_info_parameter_subblock_free(
    recon_gain_parameter_subblock_t *data) {
  if (!data) return;
  if (data->recon_gains)
    array_free(data->recon_gains,
               def_default_free_ptr(_obu_pb_recon_gain_free));
  free(data);
}

static recon_gain_parameter_subblock_t *
_obu_pb_recon_gain_info_parameter_subblock_new(
    io_context_t *ior, uint32_t duration, uint32_t id,
    iamf_pbo_extra_interfaces_t *interfaces) {
  io_context_t *r = ior;
  recon_gain_parameter_subblock_t *data = 0;
  array_t *recon_gain_present_flags = 0;
  int size = 0;

  if (!interfaces || !interfaces->get_recon_gain_present_flags) {
    warning(
        "Invalid input parameters: interfaces(%p) or "
        "get_recon_gain_present_flags(%p)",
        interfaces, interfaces ? interfaces->get_recon_gain_present_flags : 0);
    return 0;
  }

  recon_gain_present_flags =
      interfaces->get_recon_gain_present_flags(interfaces->this, id);
  if (!recon_gain_present_flags) {
    error("Failed to get recon gain flags for parameter id %u", id);
    return 0;
  }

  data = def_mallocz(recon_gain_parameter_subblock_t, 1);
  if (!data) {
    def_err_msg_enomem("recon gain info parameter data", def_pbo_str);
    return 0;
  }

  size = array_size(recon_gain_present_flags);
  data->base.type = ck_iamf_parameter_type_recon_gain;
  data->base.subblock_duration = duration;

  data->recon_gains = array_new(size);
  if (!data->recon_gains) {
    def_err_msg_enomem("recon gains", def_pbo_str);
    _obu_pb_recon_gain_info_parameter_subblock_free(data);
    return 0;
  }

  for (int i = 0; i < size; ++i) {
    uint32_t flags = def_value_wrap_u32(array_at(recon_gain_present_flags, i));
    if (flags) {
      recon_gain_t *recon_gain = _obu_pb_recon_gain_new(r, i);
      if (!recon_gain) {
        _obu_pb_recon_gain_info_parameter_subblock_free(data);
        return 0;
      }
      def_value_wrap_ptr(array_at(data->recon_gains, i)) = recon_gain;
    }
  }

  array_free(recon_gain_present_flags, 0);

  return data;
}

static void _obu_pb_animated_parameter_data_azimuth_clip(
    animated_data_float32_t *azimuth) {
  def_azimuth_clip3(azimuth->start);
  def_azimuth_clip3(azimuth->end);
  def_azimuth_clip3(azimuth->control);
}

static void _obu_pb_animated_parameter_data_elevation_clip(
    animated_data_float32_t *elevation) {
  def_elevation_clip3(elevation->start);
  def_elevation_clip3(elevation->end);
  def_elevation_clip3(elevation->control);
}

static polars_parameter_subblock_t *_obu_pb_polars_parameter_subblock_new(
    io_context_t *ior, uint32_t duration, uint32_t count) {
  io_context_t *r = ior;
  polars_parameter_subblock_t *data;
  bits_io_context_t bits_ioc, *bits_r;

  if (count < 1 || count > def_max_audio_objects) {
    error("Invalid polar count: %u (must be 1-%d)", count,
          def_max_audio_objects);
    return 0;
  }

  data = def_mallocz(polars_parameter_subblock_t, 1);
  if (!data) {
    def_err_msg_enomem("polars parameter data", def_pbo_str);
    return 0;
  }

  data->base.type = count == 1 ? ck_iamf_parameter_type_polar
                               : ck_iamf_parameter_type_dual_polar;
  data->base.subblock_duration = duration;
  data->num_polars = count;
  data->animation_type = ior_leb128_u32(r);
  bits_r = &bits_ioc;
  bits_ioc_init(bits_r, r);

  for (int i = 0; i < count && i < def_max_audio_objects; ++i) {
    animated_parameter_data_read_bits(bits_r, def_azimuth_num_bits,
                                      data->animation_type,
                                      &data->encoded_polars[i].azimuth);
    animated_parameter_data_read_bits(bits_r, def_elevation_num_bits,
                                      data->animation_type,
                                      &data->encoded_polars[i].elevation);
    animated_parameter_data_read_bits(bits_r, def_distance_num_bits,
                                      data->animation_type,
                                      &data->encoded_polars[i].distance);

    data->polars[i].animation_type = (animation_type_t)data->animation_type;
    def_animated_parameter_data_convert_bits_function(
        &data->polars[i].azimuth, &data->encoded_polars[i].azimuth,
        def_azimuth_num_bits, iamf_u32_to_i16);
    def_animated_parameter_data_convert_bits_function(
        &data->polars[i].elevation, &data->encoded_polars[i].elevation,
        def_elevation_num_bits, iamf_u32_to_i16);
    def_animated_parameter_data_convert_bits_function(
        &data->polars[i].distance, &data->encoded_polars[i].distance,
        def_distance_num_bits, iamf_u32_to_f32);

    _obu_pb_animated_parameter_data_azimuth_clip(&data->polars[i].azimuth);
    _obu_pb_animated_parameter_data_elevation_clip(&data->polars[i].elevation);
  }

  if (data->animation_type != ck_iamf_animation_type_step &&
      data->animation_type != ck_iamf_animation_type_inter_linear) {
    error("Unsupported animation type %u for polar parameter",
          data->animation_type);
    iamf_parameter_subblock_free(def_parameter_subblock_ptr(data));
    data = 0;
  }
  return data;
}

static cartesians_parameter_subblock_t *
_obu_pb_cartesians_parameter_subblock_new(io_context_t *ior, uint32_t duration,
                                          iamf_parameter_type_t type,
                                          uint32_t count) {
  io_context_t *r = ior;
  cartesians_parameter_subblock_t *data;
  bits_io_context_t bits_ioc, *bits_r;
  uint32_t num_bits = iamf_parameter_type_get_cartesian_bit_depth(type);

  if (count < 1 || count > def_max_audio_objects) {
    error("Invalid cartesian count: %u (must be 1-%d)", count,
          def_max_audio_objects);
    return 0;
  }

  if (!num_bits) return 0;

  data = def_mallocz(cartesians_parameter_subblock_t, 1);
  if (!data) {
    def_err_msg_enomem("cartesians parameter data", def_pbo_str);
    return 0;
  }

  data->base.type = type;
  data->base.subblock_duration = duration;
  data->num_cartesians = count;
  data->animation_type = ior_leb128_u32(r);

  bits_r = &bits_ioc;
  bits_ioc_init(bits_r, r);

  for (int i = 0; i < count && i < def_max_audio_objects; ++i) {
    animated_parameter_data_read_bits(bits_r, num_bits, data->animation_type,
                                      &data->encoded_cartesians[i].x);
    animated_parameter_data_read_bits(bits_r, num_bits, data->animation_type,
                                      &data->encoded_cartesians[i].y);
    animated_parameter_data_read_bits(bits_r, num_bits, data->animation_type,
                                      &data->encoded_cartesians[i].z);

    data->cartesians[i].animation_type = (animation_type_t)data->animation_type;
    def_animated_parameter_data_convert_bits_function(
        &data->cartesians[i].x, &data->encoded_cartesians[i].x, num_bits,
        def_iamf_u32_to_i16_to_f32);
    def_animated_parameter_data_convert_bits_function(
        &data->cartesians[i].y, &data->encoded_cartesians[i].y, num_bits,
        def_iamf_u32_to_i16_to_f32);
    def_animated_parameter_data_convert_bits_function(
        &data->cartesians[i].z, &data->encoded_cartesians[i].z, num_bits,
        def_iamf_u32_to_i16_to_f32);
  }

  if (data->animation_type != ck_iamf_animation_type_step &&
      data->animation_type != ck_iamf_animation_type_inter_linear &&
      data->animation_type != ck_iamf_animation_type_inter_bezier) {
    error("Unsupported animation type %u for cartesian parameter",
          data->animation_type);
    iamf_parameter_subblock_free(def_parameter_subblock_ptr(data));
    data = 0;
  }
  return data;
}

static momentary_loudness_parameter_subblock_t *
_obu_pb_momentary_loudness_parameter_subblock_new(io_context_t *ior,
                                                  uint32_t duration) {
  io_context_t *r = ior;
  momentary_loudness_parameter_subblock_t *data =
      def_mallocz(momentary_loudness_parameter_subblock_t, 1);

  if (!data) {
    def_err_msg_enomem("momentary loudness parameter data", def_pbo_str);
    return 0;
  }

  data->base.type = ck_iamf_parameter_type_momentary_loudness;
  data->base.subblock_duration = duration;
  data->momentary_loudness = ior_8(r);
  data->momentary_loudness >>= 2;
  info("\tMomentary loudness parameter data: value %u",
       data->momentary_loudness);

  return data;
}

parameter_subblock_t *_obu_pb_subblock_new(
    io_context_t *ior, parameter_base_t *base, uint32_t duration,
    iamf_pbo_extra_interfaces_t *interfaces) {
  parameter_subblock_t *data = 0;
  io_context_t *r = ior;

  switch (base->type) {
    case ck_iamf_parameter_type_mix_gain:
      data = def_parameter_subblock_ptr(
          _obu_pb_mix_gain_parameter_subblock_new(r, duration));
      break;
    case ck_iamf_parameter_type_demixing:
      data = def_parameter_subblock_ptr(
          _obu_pb_demixing_info_parameter_subblock_new(r, duration));
      break;
    case ck_iamf_parameter_type_recon_gain:
      data = def_parameter_subblock_ptr(
          _obu_pb_recon_gain_info_parameter_subblock_new(
              r, duration, base->parameter_id, interfaces));
      break;
    case ck_iamf_parameter_type_polar:
    case ck_iamf_parameter_type_dual_polar:
      data = def_parameter_subblock_ptr(_obu_pb_polars_parameter_subblock_new(
          r, duration, base->type == ck_iamf_parameter_type_polar ? 1 : 2));
      break;
    case ck_iamf_parameter_type_cartesian_8:
    case ck_iamf_parameter_type_cartesian_16:
      data =
          def_parameter_subblock_ptr(_obu_pb_cartesians_parameter_subblock_new(
              r, duration, base->type, 1));
      break;
    case ck_iamf_parameter_type_dual_cartesian_8:
    case ck_iamf_parameter_type_dual_cartesian_16:
      data =
          def_parameter_subblock_ptr(_obu_pb_cartesians_parameter_subblock_new(
              r, duration, base->type, 2));
      break;
    case ck_iamf_parameter_type_momentary_loudness:
      data = def_parameter_subblock_ptr(
          _obu_pb_momentary_loudness_parameter_subblock_new(r, duration));
      break;

    default: {
      info("invalid type %u", base->type);
      break;
    }
  }

  return data;
}

static void _obu_pb_subblock_info_display(parameter_subblock_t *subblock,
                                          uint32_t index) {
  debug("  Subblock [%u]:", index);
  debug("    type: %u (%s)", subblock->type,
        iamf_parameter_block_type_string(subblock->type));
  debug("    subblock_duration: %u", subblock->subblock_duration);
}

static void _obu_pb_mix_gain_subblock_display(
    mix_gain_parameter_subblock_t *mix_gain_subblock) {
  debug("    Mix Gain Subblock:");
  debug("      animation_type: %u (%s)", mix_gain_subblock->gain.animation_type,
        iamf_animation_type_string(mix_gain_subblock->gain.animation_type));
  if (mix_gain_subblock->gain.animation_type !=
          ck_iamf_animation_type_inter_linear &&
      mix_gain_subblock->gain.animation_type !=
          ck_iamf_animation_type_inter_bezier)
    debug("      start: %u", mix_gain_subblock->gain.data.start);
  if (mix_gain_subblock->gain.animation_type != ck_iamf_animation_type_step)
    debug("      end: %u", mix_gain_subblock->gain.data.end);
  if (mix_gain_subblock->gain.animation_type == ck_iamf_animation_type_bezier ||
      mix_gain_subblock->gain.animation_type ==
          ck_iamf_animation_type_inter_bezier) {
    debug("      control: %u", mix_gain_subblock->gain.data.control);
    debug("      control_relative_time: %u",
          mix_gain_subblock->gain.data.control_relative_time);
  }
  if (mix_gain_subblock->gain.animation_type !=
          ck_iamf_animation_type_inter_linear &&
      mix_gain_subblock->gain.animation_type !=
          ck_iamf_animation_type_inter_bezier)
    debug("      start (dB): %f", mix_gain_subblock->gain_db.data.start);
  if (mix_gain_subblock->gain.animation_type != ck_iamf_animation_type_step)
    debug("      end (dB): %f", mix_gain_subblock->gain_db.data.end);
  if (mix_gain_subblock->gain.animation_type == ck_iamf_animation_type_bezier ||
      mix_gain_subblock->gain.animation_type ==
          ck_iamf_animation_type_inter_bezier) {
    debug("      control (dB): %f", mix_gain_subblock->gain_db.data.control);
    debug("      control_relative_time: %f",
          mix_gain_subblock->gain_db.data.control_relative_time);
  }
}

static void _obu_pb_demixing_subblock_display(
    demixing_info_parameter_subblock_t *demixing_subblock) {
  debug("    Demixing Info Subblock:");
  debug("      demixing_mode: %u", demixing_subblock->demixing_mode);
}

static void _obu_pb_recon_gain_subblock_display(
    recon_gain_parameter_subblock_t *recon_gain_subblock) {
  debug("    Recon Gain Subblock:");
  if (recon_gain_subblock->recon_gains) {
    uint32_t num_recon_gains = array_size(recon_gain_subblock->recon_gains);
    debug("      Number of recon gains: %u", num_recon_gains);
    for (uint32_t j = 0; j < num_recon_gains; ++j) {
      value_wrap_t *rg_v = array_at(recon_gain_subblock->recon_gains, j);
      recon_gain_t *recon_gain = (recon_gain_t *)rg_v->ptr;
      if (recon_gain) {
        debug("      Recon Gain [%u]:", j);
        debug("        layer: %u", recon_gain->layer);
        debug("        recon_gain_flags: 0x%08x", recon_gain->recon_gain_flags);
        if (recon_gain->recon_gain) {
          uint32_t num_gains = array_size(recon_gain->recon_gain);
          debug("        Number of gains: %u", num_gains);
          for (uint32_t k = 0; k < num_gains; ++k) {
            value_wrap_t *g_v = array_at(recon_gain->recon_gain, k);
            debug("        Gain [%u]: 0x%02x", k, g_v->u32);
          }
        }
      }
    }
  }
}

static void _obu_pb_polars_subblock_display(
    polars_parameter_subblock_t *polars_subblock) {
  debug("    Polars Subblock:");
  debug("      num_polars: %u", polars_subblock->num_polars);
  debug("      animation_type: %u (%s)", polars_subblock->animation_type,
        iamf_animation_type_string(polars_subblock->animation_type));
  for (uint32_t j = 0; j < polars_subblock->num_polars; ++j) {
    debug("      Polar [%u]:", j);
    if (polars_subblock->animation_type !=
            ck_iamf_animation_type_inter_linear &&
        polars_subblock->animation_type !=
            ck_iamf_animation_type_inter_bezier) {
      debug("        encoded_azimuth start: %u",
            polars_subblock->encoded_polars[j].azimuth.start);
      debug("        encoded_elevation start: %u",
            polars_subblock->encoded_polars[j].elevation.start);
      debug("        encoded_distance start: %u",
            polars_subblock->encoded_polars[j].distance.start);
    }
    if (polars_subblock->animation_type != ck_iamf_animation_type_step) {
      debug("        encoded_azimuth end: %u",
            polars_subblock->encoded_polars[j].azimuth.end);
      debug("        encoded_elevation end: %u",
            polars_subblock->encoded_polars[j].elevation.end);
      debug("        encoded_distance end: %u",
            polars_subblock->encoded_polars[j].distance.end);
    }

    if (polars_subblock->animation_type == ck_iamf_animation_type_bezier ||
        polars_subblock->animation_type ==
            ck_iamf_animation_type_inter_bezier) {
      debug("        encoded_azimuth control: %u",
            polars_subblock->encoded_polars[j].azimuth.control);
      debug("        encoded_azimuth control_relative_time: %u",
            polars_subblock->encoded_polars[j].azimuth.control_relative_time);
      debug("        encoded_elevation control: %u",
            polars_subblock->encoded_polars[j].elevation.control);
      debug("        encoded_elevation control_relative_time: %u",
            polars_subblock->encoded_polars[j].elevation.control_relative_time);
      debug("        encoded_distance control: %u",
            polars_subblock->encoded_polars[j].distance.control);
      debug("        encoded_distance control_relative_time: %u",
            polars_subblock->encoded_polars[j].distance.control_relative_time);
    }

    // Display non-encoded parameters in encoded format
    if (polars_subblock->animation_type !=
            ck_iamf_animation_type_inter_linear &&
        polars_subblock->animation_type !=
            ck_iamf_animation_type_inter_bezier) {
      debug("        azimuth start: %f",
            polars_subblock->polars[j].azimuth.start);
      debug("        elevation start: %f",
            polars_subblock->polars[j].elevation.start);
      debug("        distance start: %f",
            polars_subblock->polars[j].distance.start);
    }
    if (polars_subblock->animation_type != ck_iamf_animation_type_step) {
      debug("        azimuth end: %f", polars_subblock->polars[j].azimuth.end);
      debug("        elevation end: %f",
            polars_subblock->polars[j].elevation.end);
      debug("        distance end: %f",
            polars_subblock->polars[j].distance.end);
    }
    if (polars_subblock->animation_type == ck_iamf_animation_type_bezier ||
        polars_subblock->animation_type ==
            ck_iamf_animation_type_inter_bezier) {
      debug("        azimuth control: %f",
            polars_subblock->polars[j].azimuth.control);
      debug("        azimuth control_relative_time: %f",
            polars_subblock->polars[j].azimuth.control_relative_time);
      debug("        elevation control: %f",
            polars_subblock->polars[j].elevation.control);
      debug("        elevation control_relative_time: %f",
            polars_subblock->polars[j].elevation.control_relative_time);
      debug("        distance control: %f",
            polars_subblock->polars[j].distance.control);
      debug("        distance control_relative_time: %f",
            polars_subblock->polars[j].distance.control_relative_time);
    }
  }
}

static void _obu_pb_cartesians_subblock_display(
    cartesians_parameter_subblock_t *cartesians_subblock) {
  debug("    Cartesians Subblock:");
  debug("      num_cartesians: %u", cartesians_subblock->num_cartesians);
  debug("      animation_type: %u (%s)", cartesians_subblock->animation_type,
        iamf_animation_type_string(cartesians_subblock->animation_type));
  for (uint32_t j = 0; j < cartesians_subblock->num_cartesians; ++j) {
    debug("      Cartesian [%u]:", j);
    // Display encoded values based on animation type
    if (cartesians_subblock->animation_type !=
            ck_iamf_animation_type_inter_linear &&
        cartesians_subblock->animation_type !=
            ck_iamf_animation_type_inter_bezier) {
      debug("        encoded_x start: %u",
            cartesians_subblock->encoded_cartesians[j].x.start);
      debug("        encoded_y start: %u",
            cartesians_subblock->encoded_cartesians[j].y.start);
      debug("        encoded_z start: %u",
            cartesians_subblock->encoded_cartesians[j].z.start);
    }
    if (cartesians_subblock->animation_type != ck_iamf_animation_type_step) {
      debug("        encoded_x end: %u",
            cartesians_subblock->encoded_cartesians[j].x.end);
      debug("        encoded_y end: %u",
            cartesians_subblock->encoded_cartesians[j].y.end);
      debug("        encoded_z end: %u",
            cartesians_subblock->encoded_cartesians[j].z.end);
    }
    if (cartesians_subblock->animation_type == ck_iamf_animation_type_bezier ||
        cartesians_subblock->animation_type ==
            ck_iamf_animation_type_inter_bezier) {
      debug("        encoded_x control: %u",
            cartesians_subblock->encoded_cartesians[j].x.control);
      debug("        encoded_x control_relative_time: %u",
            cartesians_subblock->encoded_cartesians[j].x.control_relative_time);
      debug("        encoded_y control: %u",
            cartesians_subblock->encoded_cartesians[j].y.control);
      debug("        encoded_y control_relative_time: %u",
            cartesians_subblock->encoded_cartesians[j].y.control_relative_time);
      debug("        encoded_z control: %u",
            cartesians_subblock->encoded_cartesians[j].z.control);
      debug("        encoded_z control_relative_time: %u",
            cartesians_subblock->encoded_cartesians[j].z.control_relative_time);
    }

    // Display non-encoded parameters
    if (cartesians_subblock->animation_type !=
            ck_iamf_animation_type_inter_linear &&
        cartesians_subblock->animation_type !=
            ck_iamf_animation_type_inter_bezier) {
      debug("        x start: %f", cartesians_subblock->cartesians[j].x.start);
      debug("        y start: %f", cartesians_subblock->cartesians[j].y.start);
      debug("        z start: %f", cartesians_subblock->cartesians[j].z.start);
    }
    if (cartesians_subblock->animation_type != ck_iamf_animation_type_step) {
      debug("        x end: %f", cartesians_subblock->cartesians[j].x.end);
      debug("        y end: %f", cartesians_subblock->cartesians[j].y.end);
      debug("        z end: %f", cartesians_subblock->cartesians[j].z.end);
    }
    if (cartesians_subblock->animation_type == ck_iamf_animation_type_bezier ||
        cartesians_subblock->animation_type ==
            ck_iamf_animation_type_inter_bezier) {
      debug("        x control: %f",
            cartesians_subblock->cartesians[j].x.control);
      debug("        x control_relative_time: %f",
            cartesians_subblock->cartesians[j].x.control_relative_time);
      debug("        y control: %f",
            cartesians_subblock->cartesians[j].y.control);
      debug("        y control_relative_time: %f",
            cartesians_subblock->cartesians[j].y.control_relative_time);
      debug("        z control: %f",
            cartesians_subblock->cartesians[j].z.control);
      debug("        z control_relative_time: %f",
            cartesians_subblock->cartesians[j].z.control_relative_time);
    }
  }
}

static void _obu_pb_momentary_loudness_subblock_display(
    momentary_loudness_parameter_subblock_t *momentary_loudness_subblock) {
  debug("    Momentary Loudness Subblock:");
  debug("      momentary_loudness: %u ( %d LKFS)",
        momentary_loudness_subblock->momentary_loudness,
        -1 * momentary_loudness_subblock->momentary_loudness);
}

void iamf_parameter_block_obu_display(iamf_parameter_block_obu_t *obu) {
  if (!obu) {
    debug("IAMF Parameter Block OBU is NULL, cannot display.");
    return;
  }

  debug("Displaying IAMF Parameter Block OBU:");
  debug("  obu_type: %u (%s)", obu->obu.obu_type,
        iamf_obu_type_string(obu->obu.obu_type));

  if (obu->base) {
    debug("  parameter_id: %u", obu->base->parameter_id);
    debug("  parameter_type: %u (%s)", obu->base->type,
          iamf_parameter_block_type_string(obu->base->type));
    debug("  parameter_rate: %u", obu->base->parameter_rate);
    debug("  param_definition_mode: %u", obu->base->param_definition_mode);
    debug("  duration: %u", obu->duration);
    debug("  constant_subblock_duration: %u", obu->constant_subblock_duration);
  } else {
    debug("  base: NULL");
  }

  if (obu->subblocks) {
    uint32_t num_subblocks = array_size(obu->subblocks);
    debug("  Number of subblocks: %u", num_subblocks);

    for (uint32_t i = 0; i < num_subblocks; ++i) {
      value_wrap_t *v = array_at(obu->subblocks, i);
      parameter_subblock_t *subblock = (parameter_subblock_t *)v->ptr;

      if (!subblock) {
        debug("  Subblock [%u] is NULL.", i);
        continue;
      }

      // Display common subblock information
      _obu_pb_subblock_info_display(subblock, i);

      // Display type-specific information
      switch (subblock->type) {
        case ck_iamf_parameter_type_mix_gain:
          _obu_pb_mix_gain_subblock_display(
              (mix_gain_parameter_subblock_t *)subblock);
          break;

        case ck_iamf_parameter_type_demixing:
          _obu_pb_demixing_subblock_display(
              (demixing_info_parameter_subblock_t *)subblock);
          break;

        case ck_iamf_parameter_type_recon_gain:
          _obu_pb_recon_gain_subblock_display(
              (recon_gain_parameter_subblock_t *)subblock);
          break;

        case ck_iamf_parameter_type_polar:
        case ck_iamf_parameter_type_dual_polar:
          _obu_pb_polars_subblock_display(
              (polars_parameter_subblock_t *)subblock);
          break;

        case ck_iamf_parameter_type_cartesian_8:
        case ck_iamf_parameter_type_cartesian_16:
        case ck_iamf_parameter_type_dual_cartesian_8:
        case ck_iamf_parameter_type_dual_cartesian_16:
          _obu_pb_cartesians_subblock_display(
              (cartesians_parameter_subblock_t *)subblock);
          break;

        case ck_iamf_parameter_type_momentary_loudness:
          _obu_pb_momentary_loudness_subblock_display(
              (momentary_loudness_parameter_subblock_t *)subblock);
          break;

        default:
          debug("    Unknown subblock type: %u", subblock->type);
          break;
      }
    }
  } else {
    debug("  No subblocks.");
  }

  debug("Finished displaying IAMF Parameter Block OBU.");
}
