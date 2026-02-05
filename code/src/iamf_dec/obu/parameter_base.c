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
 * @file parameter_base.c
 * @brief Parameter base implementation.
 * @version 2.0.0
 * @date Created 12/11/2025
 **/

#include "parameter_base.h"

#include "iamf_private_definitions.h"
#include "iamf_string.h"
#include "iamf_utils.h"

#undef def_log_tag
#define def_log_tag "OBU_PB"
#define def_pb_str "Parameter Base"

static demixing_info_parameter_base_t *_obu_pb_demixing_new(io_context_t *ior);
static recon_gain_parameter_base_t *_obu_pb_recon_gain_new(io_context_t *ior);
static mix_gain_parameter_base_t *_obu_pb_mix_gain_new(io_context_t *ior);
static momentary_loudness_parameter_base_t *_obu_pb_momentary_loudness_new(
    io_context_t *ior);
static polars_parameter_base_t *_obu_pb_polars_new(io_context_t *ior,
                                                   uint32_t num_polars);
static cartesians_parameter_base_t *_obu_pb_cartesians_new(
    io_context_t *ior, iamf_parameter_type_t type, uint32_t num_cartesians);
static void _obu_pb_clear(parameter_base_t *param);
static int _obu_pb_init(parameter_base_t *param_base,
                        iamf_parameter_type_t type, io_context_t *r);
static int _obu_pb_mix_gain_init(mix_gain_parameter_base_t *mix_gain_param,
                                 io_context_t *ior);

parameter_base_t *iamf_parameter_base_new(io_context_t *ior,
                                          iamf_parameter_type_t type) {
  io_context_t *r = ior;
  parameter_base_t *param = 0;

  if (type == ck_iamf_parameter_type_demixing) {
    demixing_info_parameter_base_t *dxm_param = _obu_pb_demixing_new(r);
    param = def_param_base_ptr(dxm_param);
  } else if (type == ck_iamf_parameter_type_recon_gain) {
    recon_gain_parameter_base_t *recon_gain_param = _obu_pb_recon_gain_new(r);
    param = def_param_base_ptr(recon_gain_param);
  } else if (type == ck_iamf_parameter_type_mix_gain) {
    mix_gain_parameter_base_t *mix_gain_param = _obu_pb_mix_gain_new(r);
    param = def_param_base_ptr(mix_gain_param);
  } else if (type == ck_iamf_parameter_type_polar) {
    polars_parameter_base_t *single_pos_param = _obu_pb_polars_new(r, 1);
    param = def_param_base_ptr(single_pos_param);
  } else if (type == ck_iamf_parameter_type_dual_polar) {
    polars_parameter_base_t *dual_pos_param = _obu_pb_polars_new(r, 2);
    param = def_param_base_ptr(dual_pos_param);
  } else if (type == ck_iamf_parameter_type_cartesian_8 ||
             type == ck_iamf_parameter_type_cartesian_16) {
    cartesians_parameter_base_t *single_cart_param =
        _obu_pb_cartesians_new(r, type, 1);
    param = def_param_base_ptr(single_cart_param);
  } else if (type == ck_iamf_parameter_type_dual_cartesian_8 ||
             type == ck_iamf_parameter_type_dual_cartesian_16) {
    cartesians_parameter_base_t *dual_cart_param =
        _obu_pb_cartesians_new(r, type, 2);
    param = def_param_base_ptr(dual_cart_param);
  } else if (type == ck_iamf_parameter_type_momentary_loudness) {
    momentary_loudness_parameter_base_t *momentary_loudness_param =
        _obu_pb_momentary_loudness_new(r);
    param = def_param_base_ptr(momentary_loudness_param);
  } else {
    error("Invalid parameter type %d", type);
    return 0;
  }

  return param;
}

void iamf_parameter_base_free(parameter_base_t *param) {
  if (!param) return;
  _obu_pb_clear(param);
  free(param);
}

int mix_gain_parameter_base_init(mix_gain_parameter_base_t *mix_gain_param,
                                 io_context_t *ior) {
  if (!mix_gain_param || !ior) return IAMF_ERR_BAD_ARG;

  int ret =
      _obu_pb_init(&mix_gain_param->base, ck_iamf_parameter_type_mix_gain, ior);
  return ret == IAMF_OK ? _obu_pb_mix_gain_init(mix_gain_param, ior) : ret;
}

void mix_gain_parameter_base_clear(mix_gain_parameter_base_t *mix_gain_param) {
  if (mix_gain_param) _obu_pb_clear(&mix_gain_param->base);
}

int momentary_loudness_parameter_base_init(
    momentary_loudness_parameter_base_t *momentary_loudness_param,
    io_context_t *ior) {
  if (!momentary_loudness_param || !ior) return IAMF_ERR_BAD_ARG;
  return _obu_pb_init(&momentary_loudness_param->base,
                      ck_iamf_parameter_type_momentary_loudness, ior);
}

void momentary_loudness_parameter_base_clear(
    momentary_loudness_parameter_base_t *momentary_loudness_param) {
  if (momentary_loudness_param) _obu_pb_clear(&momentary_loudness_param->base);
}

int iamf_parameter_type_is_polar(iamf_parameter_type_t type) {
  return type == ck_iamf_parameter_type_polar ||
         type == ck_iamf_parameter_type_dual_polar;
}

int iamf_parameter_type_is_cartesian(iamf_parameter_type_t type) {
  return type == ck_iamf_parameter_type_cartesian_8 ||
         type == ck_iamf_parameter_type_cartesian_16 ||
         type == ck_iamf_parameter_type_dual_cartesian_8 ||
         type == ck_iamf_parameter_type_dual_cartesian_16;
}

int iamf_parameter_type_is_coordinate(iamf_parameter_type_t type) {
  return iamf_parameter_type_is_polar(type) ||
         iamf_parameter_type_is_cartesian(type);
}

uint32_t iamf_parameter_type_get_cartesian_bit_depth(
    iamf_parameter_type_t type) {
  switch (type) {
    case ck_iamf_parameter_type_cartesian_8:
    case ck_iamf_parameter_type_dual_cartesian_8:
      return def_cartesian_8_num_bits;
    case ck_iamf_parameter_type_cartesian_16:
    case ck_iamf_parameter_type_dual_cartesian_16:
      return def_cartesian_16_num_bits;
    default:
      error("Invalid cartesian parameter type %d for bit depth retrieval",
            type);
      return 0;
  }
}

int _obu_pb_init(parameter_base_t *param_base, iamf_parameter_type_t type,
                 io_context_t *r) {
  uint32_t val;

  param_base->type = type;
  param_base->parameter_id = ior_leb128_u32(r);
  param_base->parameter_rate = ior_leb128_u32(r);
  val = ior_8(r);
  param_base->param_definition_mode = (val >> 7) & 0x01;
  info(
      "Parameter type(%u), parameter id(%u), parameter rate(%u), "
      "parameter definition mode(%u)",
      param_base->type, param_base->parameter_id, param_base->parameter_rate,
      param_base->param_definition_mode);
  if (!param_base->param_definition_mode) {
    param_base->duration = ior_leb128_u32(r);
    param_base->constant_subblock_duration = ior_leb128_u32(r);
    info("duration(%u), constant segment interval(%u)", param_base->duration,
         param_base->constant_subblock_duration);
    if (!param_base->constant_subblock_duration) {
      val = ior_leb128_u32(r);  // num_subblocks
      info("Number of subblocks(%u):", val);
      if (val > 0) {
        param_base->subblock_durations = array_new(val);
        if (!param_base->subblock_durations) {
          def_err_msg_enomem("subblock durations", def_pb_str);
          return IAMF_ERR_ALLOC_FAIL;
        }

        for (uint32_t i = 0; i < val; ++i) {
          value_wrap_t *v = array_at(param_base->subblock_durations, i);
          v->u32 = ior_leb128_u32(r);
          info("\tSubblock(%d) duration(%u)", i, v->u32);
        }
      }
    }
  }

  return IAMF_OK;
}

static int _obu_pb_demixing_init(demixing_info_parameter_base_t *dxm_param,
                                 io_context_t *ior) {
  io_context_t *r = ior;
  dxm_param->dmixp_mode = ior_8(r) >> 5 & 0x07;
  dxm_param->default_w = ior_8(r) >> 4 & 0x0f;
  info("Default mode(%u), weight index(%u)", dxm_param->dmixp_mode,
       dxm_param->default_w);
  return IAMF_OK;
}

demixing_info_parameter_base_t *_obu_pb_demixing_new(io_context_t *ior) {
  parameter_base_t *param = 0;
  demixing_info_parameter_base_t *dxm_param =
      def_mallocz(demixing_info_parameter_base_t, 1);

  if (!dxm_param) {
    def_err_msg_enomem("demixing parameter base", def_pb_str);
    return 0;
  }

  param = def_param_base_ptr(dxm_param);
  if (_obu_pb_init(param, ck_iamf_parameter_type_demixing, ior) != IAMF_OK ||
      _obu_pb_demixing_init(dxm_param, ior) != IAMF_OK) {
    iamf_parameter_base_free(param);
    return 0;
  }

  return dxm_param;
}

recon_gain_parameter_base_t *_obu_pb_recon_gain_new(io_context_t *ior) {
  parameter_base_t *param = 0;
  recon_gain_parameter_base_t *recon_gain_param =
      def_mallocz(recon_gain_parameter_base_t, 1);

  if (!recon_gain_param) {
    def_err_msg_enomem("recon gain parameter base", def_pb_str);
    return 0;
  }

  param = def_param_base_ptr(recon_gain_param);
  if (_obu_pb_init(param, ck_iamf_parameter_type_recon_gain, ior) != IAMF_OK) {
    iamf_parameter_base_free(param);
    return 0;
  }

  return recon_gain_param;
}

int _obu_pb_mix_gain_init(mix_gain_parameter_base_t *mix_gain_param,
                          io_context_t *ior) {
  io_context_t *r = ior;
  mix_gain_param->default_mix_gain = (int16_t)ior_b16(r);
  mix_gain_param->default_mix_gain_db =
      iamf_gain_q78_to_db(mix_gain_param->default_mix_gain);
  info("Default mix gain %f(%x) dB", mix_gain_param->default_mix_gain_db,
       mix_gain_param->default_mix_gain);
  return IAMF_OK;
}

mix_gain_parameter_base_t *_obu_pb_mix_gain_new(io_context_t *ior) {
  mix_gain_parameter_base_t *mix_gain_param =
      def_mallocz(mix_gain_parameter_base_t, 1);

  if (!mix_gain_param) {
    def_err_msg_enomem("mix gain parameter base", def_pb_str);
    return 0;
  }

  if (mix_gain_parameter_base_init(mix_gain_param, ior) != IAMF_OK) {
    parameter_base_t *param = def_param_base_ptr(mix_gain_param);
    iamf_parameter_base_free(param);
    return 0;
  }

  return mix_gain_param;
}

static int _obu_pb_polars_init(polars_parameter_base_t *pos_param,
                               io_context_t *ior, uint32_t num_polars) {
  bits_io_context_t bits_ioc, *bits_r;

  bits_r = &bits_ioc;
  bits_ioc_init(bits_r, ior);

  pos_param->num_polars = num_polars;
  for (int i = 0; i < num_polars; i++) {
    pos_param->encoded_default_polars[i].azimuth =
        bits_ior_le32(bits_r, def_azimuth_num_bits);
    pos_param->encoded_default_polars[i].elevation =
        bits_ior_le32(bits_r, def_elevation_num_bits);
    pos_param->encoded_default_polars[i].distance =
        bits_ior_le32(bits_r, def_distance_num_bits);
    info("Encoded Position(%d) azimuth(%02x), elevation(%02x), distance(%02x)",
         i, pos_param->encoded_default_polars[i].azimuth,
         pos_param->encoded_default_polars[i].elevation,
         pos_param->encoded_default_polars[i].distance);

    pos_param->default_polars[i].azimuth = def_azimuth_clip(iamf_u32_to_i16(
        pos_param->encoded_default_polars[i].azimuth, def_azimuth_num_bits));
    pos_param->default_polars[i].elevation = def_elevation_clip(
        iamf_u32_to_i16(pos_param->encoded_default_polars[i].elevation,
                        def_elevation_num_bits));
    pos_param->default_polars[i].distance = def_distance_clip(iamf_u32_to_f32(
        pos_param->encoded_default_polars[i].distance, def_distance_num_bits));

    info("Position(%d) azimuth(%f), elevation(%f), distance(%f)", i,
         pos_param->default_polars[i].azimuth,
         pos_param->default_polars[i].elevation,
         pos_param->default_polars[i].distance);
  }

  return IAMF_OK;
}

polars_parameter_base_t *_obu_pb_polars_new(io_context_t *ior,
                                            uint32_t num_polars) {
  polars_parameter_base_t *pos_param = def_mallocz(polars_parameter_base_t, 1);
  if (!pos_param) {
    def_err_msg_enomem("position parameter base", def_pb_str);
    return 0;
  }

  if (_obu_pb_init(&pos_param->base,
                   num_polars == 1 ? ck_iamf_parameter_type_polar
                                   : ck_iamf_parameter_type_dual_polar,
                   ior) != IAMF_OK ||
      _obu_pb_polars_init(pos_param, ior, num_polars) != IAMF_OK) {
    iamf_parameter_base_free(def_param_base_ptr(pos_param));
    return 0;
  }

  return pos_param;
}

static int _obu_pb_cartesians_init(cartesians_parameter_base_t *cart_param,
                                   io_context_t *ior,
                                   iamf_parameter_type_t type,
                                   uint32_t num_cartesians) {
  bits_io_context_t bits_ioc, *bits_r;
  uint32_t num_bits;

  bits_r = &bits_ioc;
  bits_ioc_init(bits_r, ior);

  num_bits = iamf_parameter_type_get_cartesian_bit_depth(type);
  if (!num_bits) {
    error("Invalid cartesian parameter type %d for initialization", type);
    return IAMF_ERR_BAD_ARG;
  }

  cart_param->num_cartesians = num_cartesians;
  for (int i = 0; i < num_cartesians; i++) {
    cart_param->encoded_default_cartesians[i].x =
        bits_ior_le32(bits_r, num_bits);
    cart_param->encoded_default_cartesians[i].y =
        bits_ior_le32(bits_r, num_bits);
    cart_param->encoded_default_cartesians[i].z =
        bits_ior_le32(bits_r, num_bits);

    cart_param->default_cartesians[i].x = def_clip_normalized(iamf_i16_to_f32(
        iamf_u32_to_i16(cart_param->encoded_default_cartesians[i].x, num_bits),
        num_bits));
    cart_param->default_cartesians[i].y = def_clip_normalized(iamf_i16_to_f32(
        iamf_u32_to_i16(cart_param->encoded_default_cartesians[i].y, num_bits),
        num_bits));
    cart_param->default_cartesians[i].z = def_clip_normalized(iamf_i16_to_f32(
        iamf_u32_to_i16(cart_param->encoded_default_cartesians[i].z, num_bits),
        num_bits));

    info("Cartesian(%d) x(%f), y(%f), z(%f)", i,
         cart_param->default_cartesians[i].x,
         cart_param->default_cartesians[i].y,
         cart_param->default_cartesians[i].z);
  }

  return IAMF_OK;
}

static cartesians_parameter_base_t *_obu_pb_cartesians_new(
    io_context_t *ior, iamf_parameter_type_t type, uint32_t num_cartesians) {
  cartesians_parameter_base_t *cart_param =
      def_mallocz(cartesians_parameter_base_t, 1);
  if (!cart_param) {
    def_err_msg_enomem("cartesian parameter base", def_pb_str);
    return 0;
  }

  if (_obu_pb_init(&cart_param->base, type, ior) != IAMF_OK ||
      _obu_pb_cartesians_init(cart_param, ior, type, num_cartesians) !=
          IAMF_OK) {
    iamf_parameter_base_free(def_param_base_ptr(cart_param));
    return 0;
  }

  return cart_param;
}

momentary_loudness_parameter_base_t *_obu_pb_momentary_loudness_new(
    io_context_t *ior) {
  momentary_loudness_parameter_base_t *momentary_loudness_param =
      def_mallocz(momentary_loudness_parameter_base_t, 1);

  if (!momentary_loudness_param) {
    def_err_msg_enomem("momentary loudness parameter base", def_pb_str);
    return 0;
  }

  if (momentary_loudness_parameter_base_init(momentary_loudness_param, ior) !=
      IAMF_OK) {
    parameter_base_t *param = def_param_base_ptr(momentary_loudness_param);
    iamf_parameter_base_free(param);
    return 0;
  }

  return momentary_loudness_param;
}

void _obu_pb_clear(parameter_base_t *param) {
  if (param->subblock_durations) array_free(param->subblock_durations, 0);
}
