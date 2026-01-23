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
 * @file parameter_block_obu.h
 * @brief Parameter block OBU APIs.
 * @version 2.0.0
 * @date Created 12/11/2025
 **/

#ifndef __PARAMETER_BLOCK_OBU_H__
#define __PARAMETER_BLOCK_OBU_H__

#include "animated_parameter.h"
#include "carray.h"
#include "cvector.h"
#include "iamf_obu.h"
#include "iamf_private_definitions.h"
#include "parameter_base.h"

/**
 *  Parameter Block OBU.
 * */

#define def_demixing_info_parameter_subblock_ptr(a) \
  ((demixing_info_parameter_subblock_t *)a)

#define def_recon_gain_parameter_subblock_ptr(a) \
  ((recon_gain_parameter_subblock_t *)a)
#define def_parameter_block_obu_ptr(a) def_ptr(iamf_parameter_block_obu, a)
#define def_parameter_subblock_ptr(a) def_ptr(parameter_subblock, a)
#define def_mix_gain_parameter_subblock_ptr(a) \
  def_ptr(mix_gain_parameter_subblock, a)
#define def_polars_parameter_subblock_ptr(a) \
  def_ptr(polars_parameter_subblock, a)
#define def_cartesians_parameter_subblock_ptr(a) \
  def_ptr(cartesians_parameter_subblock, a)
#define def_momentary_loudness_parameter_subblock_ptr(a) \
  def_ptr(momentary_loudness_parameter_subblock, a)

typedef struct IamfParameterBlockObu {
  iamf_obu_t obu;
  parameter_base_t *base;
  uint32_t duration;
  uint32_t constant_subblock_duration;

  array_t *subblocks;  // array<parameter_subblock_t>;
} iamf_parameter_block_obu_t;

typedef struct ParameterSubblock {
  iamf_parameter_type_t type;
  uint32_t subblock_duration;
} parameter_subblock_t;

typedef struct MixGainParameterSubblock {
  parameter_subblock_t base;
  ap_u32_t gain;
  animated_float32_t gain_db;
} mix_gain_parameter_subblock_t;

typedef struct DemixingInfoParameterSubblock {
  parameter_subblock_t base;
  uint32_t demixing_mode;
} demixing_info_parameter_subblock_t;

typedef struct ReconGainInfoParameterSubblock {
  parameter_subblock_t base;
  array_t *recon_gains;  // array<recon_gain_t>;
} recon_gain_parameter_subblock_t;

typedef struct ReconGain {
  uint32_t layer;
  uint32_t recon_gain_flags;
  array_t *recon_gain;         // array<uint32_t>
  array_t *recon_gain_linear;  // array<float>;  // need to remove
} recon_gain_t;

typedef struct PolarsParameterSubblock {
  parameter_subblock_t base;

  uint32_t num_polars;
  iamf_animation_type_t animation_type;
  struct {
    apd_u32_t azimuth, elevation, distance;
  } encoded_polars[def_max_audio_objects];
  animated_polar_t polars[def_max_audio_objects];
} polars_parameter_subblock_t;

typedef struct CartesiansParameterSubblock {
  parameter_subblock_t base;

  uint32_t num_cartesians;
  iamf_animation_type_t animation_type;

  struct {
    apd_u32_t x, y, z;
  } encoded_cartesians[def_max_audio_objects];
  animated_cartesian_t cartesians[def_max_audio_objects];
} cartesians_parameter_subblock_t;

typedef struct MomentaryLoudnessParameterSubblock {
  parameter_subblock_t base;
  uint32_t momentary_loudness;
} momentary_loudness_parameter_subblock_t;

iamf_parameter_block_obu_t *iamf_parameter_block_obu_new(
    io_context_t *ior, iamf_pbo_extra_interfaces_t *interfaces);
void iamf_parameter_block_obu_free(iamf_parameter_block_obu_t *obu);
void iamf_parameter_subblock_free(parameter_subblock_t *subblock);
void iamf_parameter_block_obu_display(iamf_parameter_block_obu_t *obu);

#endif  // __PARAMETER_BLOCK_OBU_H__
