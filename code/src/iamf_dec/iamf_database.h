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
 * @file iamf_database.h
 * @brief IAMF database APIs.
 * @version 2.0.0
 * @date Created 12/11/2025
 **/

#ifndef __IAMF_DATABASE_H__
#define __IAMF_DATABASE_H__

#include "cdeque.h"
#include "cvector.h"
#include "iamf_obu_all.h"
#include "iamf_obu_parser.h"
#include "iamf_utils.h"

#define def_parameter_block_ptr(a) def_ptr(parameter_block, a)
#define def_mix_gain_parameter_block_ptr(a) def_ptr(mix_gain_parameter_block, a)
#define def_polars_parameter_block_ptr(a) def_ptr(polars_parameter_block, a)
#define def_cartesians_parameter_block_ptr(a) \
  def_ptr(cartesians_parameter_block, a)

typedef int (*func_sub_mix_check_t)(obu_sub_mix_t *, iamf_layout_t, void *);

typedef enum EDBDescriptorsState {
  ck_descriptors_state_none = 0,
  ck_descriptors_state_processing,
  ck_descriptors_state_completed,
} descriptors_state_t;

typedef struct DBCodecConfig {
  uint32_t id;
  iamf_codec_config_obu_t *codec_config_obu;

  audio_codec_parameter_t codec_param;
} codec_config_t;

/***
 * Audio Element Info
 */

typedef struct DBAudioElement {
  uint32_t id;
  iamf_audio_element_obu_t *audio_element_obu;

  codec_config_t *codec_config;

  uint32_t demixing_info_id;
  uint32_t recon_gain_id;
} audio_element_t;

/***
 * Parameter Block Info
 * */

typedef struct DBParameterBlock {
  parameter_base_t *base;
  deque_t *subblocks;
  uint32_t duration;
  int elapsed;
  int enabled;
  // Default start of first block is 0, this variable records the start position
  // of second block, If 0, it means there is no second block.
  int next_block_index;
} parameter_block_t;

typedef struct DBMixGainParameterBlock {
  parameter_block_t parameter_block;
  float end_gain;
} mix_gain_parameter_block_t;

typedef struct DBPolarsParameterBlock {
  parameter_block_t parameter_block;
  uint32_t num_polars;
  polar_t end_polars[2];
} polars_parameter_block_t;

typedef struct DBCartesiansParameterBlock {
  parameter_block_t parameter_block;
  uint32_t num_cartesians;
  cartesian_t end_cartesians[2];
} cartesians_parameter_block_t;

typedef struct DBParameterBlockManager {
  vector_t *parameter_blocks;  // vector<parameter_block_t>

  vector_t *demixing_infos;  // vector<parameter_block_t>
  vector_t *recon_gains;     // vector<parameter_block_t>
  vector_t *mix_gains;       // vector<parameter_block_t>
  vector_t *coordinates;     // vector<parameter_block_t>
} parameter_block_manager_t;

typedef struct IamfDescriptors {
  iamf_sequence_header_obu_t *ia_sequence_header_obu;

  vector_t *codec_config_obus;
  vector_t *audio_element_obus;
  vector_t *mix_presentation_obus;

  int num_lpcm_codec;
  int num_object_elements;
  descriptors_state_t state;
} iamf_descriptors_t;

/***
 * OBU Database Info
 * */

typedef struct Iamf_Database {
  iamf_descriptors_t descriptors;

  vector_t *codec_configs;   // vector<codec_config_t>
  vector_t *audio_elements;  // vector<audio_element_t>

  parameter_block_manager_t parameter_block_manager;

  iamf_profile_t profile;
} iamf_database_t;

// WARNING: it well be removed in the future.
typedef struct MixGainUnit {
  int count;
  float constant_gain;
  float *gains;
} mix_gain_unit_t;

/***
 * IAMF OBU Database APIs.
 * */

int iamf_database_init(iamf_database_t *database);
void iamf_database_uninit(iamf_database_t *database);
int iamf_database_reset_descriptors(iamf_database_t *database);
int iamf_database_set_profile(iamf_database_t *database,
                              iamf_profile_t profile);
iamf_profile_t iamf_database_get_profile(iamf_database_t *database);

int iamf_database_add_obu(iamf_database_t *database, iamf_obu_t *obu);
iamf_codec_config_obu_t *iamf_database_get_codec_config_obu(
    iamf_database_t *database, uint32_t id);
audio_element_t *iamf_database_get_audio_element(iamf_database_t *database,
                                                 uint32_t id);
audio_element_t *iamf_database_get_audio_element_with_pid(
    iamf_database_t *database, uint32_t pid);
iamf_audio_element_obu_t *iamf_database_get_audio_element_obu(
    iamf_database_t *database, uint32_t eid);
iamf_audio_element_obu_t *iamf_database_get_audio_element_obu_with_pid(
    iamf_database_t *database, uint32_t pid);
iamf_mix_presentation_obu_t *iamf_database_get_mix_presentation_obu(
    iamf_database_t *database, uint32_t id);
iamf_mix_presentation_obu_t *iamf_database_get_mix_presentation_obu_default(
    iamf_database_t *database);
iamf_mix_presentation_obu_t *iamf_database_find_mix_presentation_obu(
    iamf_database_t *database, func_sub_mix_check_t check_func,
    iamf_layout_t target_layout);
iamf_mix_presentation_obu_t *
iamf_database_find_mix_presentation_obu_with_highest_layout(
    iamf_database_t *database, iamf_layout_t reference_layout);
parameter_base_t *iamf_database_get_parameter_base(iamf_database_t *database,
                                                   uint32_t pid);
parameter_block_t *iamf_database_get_parameter_block(iamf_database_t *database,
                                                     uint32_t pid);
int iamf_database_enable_parameter_block(iamf_database_t *database,
                                         uint32_t pid);
int iamf_database_enable_mix_presentation_parameter_blocks(
    iamf_database_t *database, uint32_t pid);
int iamf_database_disable_parameter_blocks(iamf_database_t *database);
int iamf_database_disabled_parameter_blocks_clear(iamf_database_t *database);
int iamf_database_get_demix_mode(iamf_database_t *database, uint32_t pid,
                                 uint32_t offset);
array_t *iamf_database_get_recon_gain_present_flags(iamf_database_t *database,
                                                    uint32_t pid);
int iamf_database_get_subblocks(iamf_database_t *database, uint32_t pid,
                                fraction_t number,
                                parameter_subblock_t ***subblocks);
uint32_t iamf_database_get_parameter_rate(iamf_database_t *database,
                                          uint32_t pid);
int iamf_database_time_elapse(iamf_database_t *database, fraction_t time);
int iamf_database_get_audio_element_default_demixing_info(
    iamf_database_t *database, uint32_t eid, int *mode, int *weight_index);
int iamf_database_get_audio_element_demix_mode(iamf_database_t *database,
                                               uint32_t eid, fraction_t offset);

descriptors_state_t iamf_database_descriptors_get_state(
    iamf_database_t *database);
int iamf_database_descriptors_complete(iamf_database_t *database);

#endif  // __IAMF_DATABASE_H__
