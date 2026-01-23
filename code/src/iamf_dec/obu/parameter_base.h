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
 * @file parameter_base.h
 * @brief Parameter base APIs.
 * @version 2.0.0
 * @date Created 12/11/2025
 **/

#ifndef __PARAMETER_BASE_H__
#define __PARAMETER_BASE_H__

#include "IAMF_defines.h"
#include "carray.h"
#include "iamf_private_definitions.h"
#include "iamf_types.h"
#include "iorw.h"
#include "oar_base.h"

#define def_demixing_info_param_base_ptr(a) \
  ((demixing_info_parameter_base_t *)a)
#define def_param_base_ptr(a) def_ptr(parameter_base, a)
#define def_mix_gain_parameter_base_ptr(a) def_ptr(mix_gain_parameter_base, a)
#define def_polars_parameter_base_ptr(a) def_ptr(polars_parameter_base, a)
#define def_cartesians_parameter_base_ptr(a) \
  def_ptr(cartesians_parameter_base, a)

typedef enum EIamfParameterType {
  ck_iamf_parameter_type_none = -1,
  ck_iamf_parameter_type_mix_gain = 0,
  ck_iamf_parameter_type_demixing,
  ck_iamf_parameter_type_recon_gain,
  ck_iamf_parameter_type_polar,
  ck_iamf_parameter_type_cartesian_8,
  ck_iamf_parameter_type_cartesian_16,
  ck_iamf_parameter_type_dual_polar,
  ck_iamf_parameter_type_dual_cartesian_8,
  ck_iamf_parameter_type_dual_cartesian_16,
  ck_iamf_parameter_type_momentary_loudness,
  ck_iamf_parameter_type_count,
} iamf_parameter_type_t;

typedef struct ParameterBase {
  uint32_t type;
  uint32_t parameter_id;
  uint32_t parameter_rate;
  uint32_t param_definition_mode;
  uint32_t duration;
  uint32_t constant_subblock_duration;
  array_t *subblock_durations;  // array<uint32_t>
} parameter_base_t;

typedef struct DemixingInfoParameterBase {
  parameter_base_t base;
  uint32_t dmixp_mode;
  uint32_t default_w;
} demixing_info_parameter_base_t;

typedef struct ReconGainParameterBase {
  parameter_base_t base;
} recon_gain_parameter_base_t;

typedef struct MixGainParameterBase {
  parameter_base_t base;
  int16_t default_mix_gain;
  float default_mix_gain_db;
} mix_gain_parameter_base_t;

typedef struct PolarsParameterBase {
  parameter_base_t base;

  uint32_t num_polars;
  encoded_polar_t encoded_default_polars[def_max_audio_objects];
  polar_t default_polars[def_max_audio_objects];
} polars_parameter_base_t;

typedef struct CartesiansParameterBase {
  parameter_base_t base;

  uint32_t num_cartesians;
  encoded_cartesian_t encoded_default_cartesians[def_max_audio_objects];

  /**
   * @brief Following the Cartesian coordinates axes for Objects in
   * [ITU-2076-2], the X, Y, Z coordinate values are provided as normalized
   * values where 1.0 and -1.0 are on the surface of the cube, with the origin
   * being the centre of  the cube. The direction of each axes are:
   *
   * X: left to right, with positive values to the right.
   * Y: front to back, with positive values to the front.
   * Z: top to bottom, with positive values to the top.
   */
  cartesian_t default_cartesians[def_max_audio_objects];
} cartesians_parameter_base_t;

typedef struct MomentaryLoudnessParameterBase {
  parameter_base_t base;
} momentary_loudness_parameter_base_t;

parameter_base_t *iamf_parameter_base_new(io_context_t *ior,
                                          iamf_parameter_type_t type);
void iamf_parameter_base_free(parameter_base_t *param);

int mix_gain_parameter_base_init(mix_gain_parameter_base_t *mix_gain_param,
                                 io_context_t *ior);
void mix_gain_parameter_base_clear(mix_gain_parameter_base_t *mix_gain_param);

int momentary_loudness_parameter_base_init(
    momentary_loudness_parameter_base_t *momentary_loudness_param,
    io_context_t *ior);
void momentary_loudness_parameter_base_clear(
    momentary_loudness_parameter_base_t *momentary_loudness_param);

int iamf_parameter_type_is_polar(iamf_parameter_type_t type);
int iamf_parameter_type_is_cartesian(iamf_parameter_type_t type);
int iamf_parameter_type_is_coordinate(iamf_parameter_type_t type);
uint32_t iamf_parameter_type_get_cartesian_bit_depth(
    iamf_parameter_type_t type);

void parameter_base_display(parameter_base_t *param_base);
#endif  //__PARAMETER_BASE_H__
