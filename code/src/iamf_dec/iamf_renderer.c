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
 * @file iamf_renderer.c
 * @brief renderer implementation.
 * @version 2.0.0
 * @date Created 12/11/2025
 **/

#include "iamf_renderer.h"

#include <math.h>
#include <string.h>

#include "iamf_private_definitions.h"
#include "iamf_utils.h"

static const struct {
  iamf_sound_system_t sound_system;
  oar_layout_t layout;
} _iamf2oar_sound_system_map[] = {
    {SOUND_SYSTEM_A, ck_oar_layout_sound_system_a_020},
    {SOUND_SYSTEM_B, ck_oar_layout_sound_system_b_050},
    {SOUND_SYSTEM_C, ck_oar_layout_sound_system_c_250},
    {SOUND_SYSTEM_D, ck_oar_layout_sound_system_d_450},
    {SOUND_SYSTEM_E, ck_oar_layout_sound_system_e_451},
    {SOUND_SYSTEM_F, ck_oar_layout_sound_system_f_370},
    {SOUND_SYSTEM_G, ck_oar_layout_sound_system_g_490},
    {SOUND_SYSTEM_H, ck_oar_layout_sound_system_h_9a3},
    {SOUND_SYSTEM_I, ck_oar_layout_sound_system_i_070},
    {SOUND_SYSTEM_J, ck_oar_layout_sound_system_j_470},
    {SOUND_SYSTEM_EXT_712, ck_oar_layout_712},
    {SOUND_SYSTEM_EXT_312, ck_oar_layout_312},
    {SOUND_SYSTEM_MONO, ck_oar_layout_mono},
    {SOUND_SYSTEM_EXT_916, ck_oar_layout_916},
    {SOUND_SYSTEM_EXT_7154, ck_oar_layout_7154},
};

oar_layout_t _iamf2oar_output_layout_get(iamf_layout_t layout) {
  if (layout.type == ck_iamf_layout_type_binaural)
    return ck_oar_layout_binaural;
  if (layout.type != ck_iamf_layout_type_loudspeakers_ss_convention)
    return ck_oar_layout_none;
  for (size_t i = 0; i < sizeof(_iamf2oar_sound_system_map) /
                             sizeof(_iamf2oar_sound_system_map[0]);
       i++)
    if (layout.sound_system == _iamf2oar_sound_system_map[i].sound_system)
      return _iamf2oar_sound_system_map[i].layout;
  return ck_oar_layout_none;
}

static const struct {
  iamf_loudspeaker_layout_t iamf_layout;
  oar_layout_t oar_layout;
} _iamf2oar_channel_layout_map[] = {
    {ck_iamf_loudspeaker_layout_mono, ck_oar_layout_mono},
    {ck_iamf_loudspeaker_layout_stereo, ck_oar_layout_stereo},
    {ck_iamf_loudspeaker_layout_510, ck_oar_layout_51},
    {ck_iamf_loudspeaker_layout_512, ck_oar_layout_512},
    {ck_iamf_loudspeaker_layout_514, ck_oar_layout_514},
    {ck_iamf_loudspeaker_layout_710, ck_oar_layout_71},
    {ck_iamf_loudspeaker_layout_712, ck_oar_layout_712},
    {ck_iamf_loudspeaker_layout_714, ck_oar_layout_714},
    {ck_iamf_loudspeaker_layout_312, ck_oar_layout_312},
    {ck_iamf_loudspeaker_layout_binaural, ck_oar_layout_binaural},
    {ck_iamf_loudspeaker_layout_expanded_lfe, ck_oar_layout_lfe},
    {ck_iamf_loudspeaker_layout_expanded_stereo_s, ck_oar_layout_stereo_s},
    {ck_iamf_loudspeaker_layout_expanded_stereo_ss, ck_oar_layout_stereo_ss},
    {ck_iamf_loudspeaker_layout_expanded_stereo_rs, ck_oar_layout_stereo_rs},
    {ck_iamf_loudspeaker_layout_expanded_stereo_tf, ck_oar_layout_stereo_tf},
    {ck_iamf_loudspeaker_layout_expanded_stereo_tb, ck_oar_layout_stereo_tb},
    {ck_iamf_loudspeaker_layout_expanded_top_4ch, ck_oar_layout_top_4ch},
    {ck_iamf_loudspeaker_layout_expanded_3ch, ck_oar_layout_3ch},
    {ck_iamf_loudspeaker_layout_expanded_916, ck_oar_layout_916},
    {ck_iamf_loudspeaker_layout_expanded_stereo_f, ck_oar_layout_stereo_f},
    {ck_iamf_loudspeaker_layout_expanded_stereo_si, ck_oar_layout_stereo_si},
    {ck_iamf_loudspeaker_layout_expanded_stereo_tpsi,
     ck_oar_layout_stereo_tpsi},
    {ck_iamf_loudspeaker_layout_expanded_top_6ch, ck_oar_layout_top_6ch},
    {ck_iamf_loudspeaker_layout_expanded_lfe_pair, ck_oar_layout_lfe_pair},
    {ck_iamf_loudspeaker_layout_expanded_bottom_3ch, ck_oar_layout_bottom_3ch},
    {ck_iamf_loudspeaker_layout_expanded_bottom_4ch, ck_oar_layout_bottom_4ch},
    {ck_iamf_loudspeaker_layout_expanded_top_1ch, ck_oar_layout_top_1ch},
    {ck_iamf_loudspeaker_layout_expanded_top_5ch, ck_oar_layout_top_5ch},
    {ck_iamf_loudspeaker_layout_expanded_a293, ck_oar_layout_a293},
    {ck_iamf_loudspeaker_layout_expanded_7154, ck_oar_layout_7154},
};

static oar_layout_t _iamf2oar_input_layout_get(
    iamf_loudspeaker_layout_t layout) {
  for (size_t i = 0; i < sizeof(_iamf2oar_channel_layout_map) /
                             sizeof(_iamf2oar_channel_layout_map[0]);
       i++)
    if (layout == _iamf2oar_channel_layout_map[i].iamf_layout)
      return _iamf2oar_channel_layout_map[i].oar_layout;
  return ck_oar_layout_none;
}

static oar_hoa_t _iamf2oar_hoa_get(uint32_t channels) {
  switch (channels) {
    case 1:
      return ck_oar_zoa;
    case 4:
      return ck_oar_1oa;
    case 9:
      return ck_oar_2oa;
    case 16:
      return ck_oar_3oa;
    case 25:
      return ck_oar_4oa;
    default:
      return ck_oar_hoa_none;
  }
}

static oar_audio_block_t _iamf2oar_audio_block_copy(iamf_audio_block_t* block) {
  oar_audio_block_t data = {block->data, block->num_channels,
                            block->num_samples_per_channel};
  return data;
}

static void _set_rendering_parameters(parameter_set_t* parameters,
                                      iamf_rendering_param_t param) {
  parameters->flags |= def_parameter_set_flag_iamf_element_rendering_config;
  parameters->element_rendering_config.headphones_rendering_mode =
      param.headphones_rendering_mode;
  parameters->element_rendering_config.binaural_filter_profile =
      param.binaural_filter_profile;
}

static int iamf_renderer_private_acquire_group_id(iamf_renderer_t* self,
                                                  uint32_t index) {
  if (index >= def_max_sub_mixes) return IAMF_ERR_BAD_ARG;

  if (self->gids[index] == def_i32_id_none) {
    self->gids[index] = oar_add_audio_group(self->oar);
    if (self->gids[index] < 0) {
      self->gids[index] = def_i32_id_none;
      return IAMF_ERR_INTERNAL;
    }
  }

  return self->gids[index];
}

int iamf_renderer_private_add_element(
    iamf_renderer_t* self, uint32_t id, uint32_t group_index,
    const oar_audio_element_config_t* config) {
  int gid = iamf_renderer_private_acquire_group_id(self, group_index);
  if (gid < 0) return IAMF_ERR_BAD_ARG;
  return oar_add_audio_element(self->oar, gid, id, config) == ck_oar_ok
             ? IAMF_OK
             : IAMF_ERR_INTERNAL;
}

int iamf_renderer_init(iamf_renderer_t* self, iamf_layout_t layout,
                       uint32_t number_samples_per_frame,
                       uint32_t sampling_rate) {
  oar_config_t _config;
  _config.target_layout = _iamf2oar_output_layout_get(layout);
  if (_config.target_layout == ck_oar_layout_none) return IAMF_ERR_BAD_ARG;
  _config.samples_per_channel = number_samples_per_frame;
  _config.sampling_rate = sampling_rate;
  self->oar = oar_create(&_config);
  if (!self->oar) return IAMF_ERR_INTERNAL;

  for (int i = 0; i < def_max_sub_mixes; ++i) self->gids[i] = def_i32_id_none;
  self->samples_per_frame = number_samples_per_frame;

  return IAMF_OK;
}

void iamf_renderer_uninit(iamf_renderer_t* self) {
  if (self->oar) oar_destroy(self->oar);
  memset(self, 0, sizeof(*self));
}

int iamf_renderer_add_channel_based_element(iamf_renderer_t* self, uint32_t id,
                                            uint32_t group_index,
                                            iamf_loudspeaker_layout_t layout,
                                            iamf_rendering_param_t param,
                                            int downmix_mode,
                                            int downmix_weight_index) {
  oar_audio_element_config_t config = {0};
  config.type = ck_channel_based;
  config.cbc.layout = _iamf2oar_input_layout_get(layout);
  if (config.cbc.layout == ck_oar_layout_none) return IAMF_ERR_BAD_ARG;
  _set_rendering_parameters(&config.parameters, param);
  if (downmix_mode != def_dmx_mode_none) {
    config.parameters.flags |= def_parameter_set_flag_iamf_downmix_info;
    config.parameters.downmix_info.mode = downmix_mode;
    config.parameters.downmix_info.weight_index = downmix_weight_index;
  }

  return iamf_renderer_private_add_element(self, id, group_index, &config);
}

int iamf_renderer_add_scene_based_element(iamf_renderer_t* self, uint32_t id,
                                          uint32_t group_index,
                                          uint32_t number_channels,
                                          iamf_rendering_param_t param) {
  oar_audio_element_config_t config = {0};
  config.type = ck_scene_based;
  config.sbc.order = _iamf2oar_hoa_get(number_channels);
  if (config.sbc.order == ck_oar_hoa_none) return IAMF_ERR_BAD_ARG;
  _set_rendering_parameters(&config.parameters, param);
  return iamf_renderer_private_add_element(self, id, group_index, &config);
}

int iamf_renderer_add_object_based_element(iamf_renderer_t* self, uint32_t id,
                                           uint32_t group_index,
                                           uint32_t number_objects,
                                           iamf_rendering_param_t param) {
  oar_audio_element_config_t config = {0};
  config.type = ck_object_based;
  config.obc.num_objects = number_objects;
  _set_rendering_parameters(&config.parameters, param);

  return iamf_renderer_private_add_element(self, id, group_index, &config);
}

int iamf_renderer_add_element_audio_data(iamf_renderer_t* self,
                                         iamf_audio_block_t* block) {
  if (!self || !block) return IAMF_ERR_BAD_ARG;
  if (!self->oar) return IAMF_ERR_INTERNAL;

  oar_audio_block_t audio_data = _iamf2oar_audio_block_copy(block);
  return oar_update_audio_element_data(self->oar, block->id, &audio_data) ==
                 ck_oar_ok
             ? IAMF_OK
             : IAMF_ERR_INTERNAL;
}

int iamf_renderer_update_element_animated_gain(iamf_renderer_t* self,
                                               uint32_t id, uint32_t pid,
                                               animated_float32_t animated_gain,
                                               uint32_t number) {
  oar_metadata_t gain;

  if (!self || !self->oar) return IAMF_ERR_BAD_ARG;

  gain.type = ck_metadata_gain;
  gain.gain.id = pid;
  gain.gain.param_type = ck_param_animated;

  // Assign animated_gains field
  gain.gain.animated_gains = animated_gain;
  gain.duration = number;

  debug(
      "display pid %d: gain %d: animation type %d: start %f, end %f, control "
      "%f, control_relative_time %f, duration %d",
      pid, pid, animated_gain.animation_type, animated_gain.data.start,
      animated_gain.data.end, animated_gain.data.control,
      animated_gain.data.control_relative_time, number);

  return oar_update_audio_element_metadata(self->oar, id, &gain) == ck_oar_ok
             ? IAMF_OK
             : IAMF_ERR_INTERNAL;
}

int iamf_renderer_update_element_downmix_mode(iamf_renderer_t* self,
                                              uint32_t id, int mode,
                                              int period) {
  oar_metadata_t dmx;
  dmx.type = ck_metadata_iamf_downmix_mode;
  dmx.iamf_downmix_mode.mode = mode;
  dmx.duration = period;
  return oar_update_audio_element_metadata(self->oar, id, &dmx) == ck_oar_ok
             ? IAMF_OK
             : IAMF_ERR_INTERNAL;
}

int iamf_renderer_update_animated_gain(iamf_renderer_t* self, uint32_t pid,
                                       uint32_t group_index,
                                       animated_float32_t animated_gain,
                                       uint32_t number) {
  oar_metadata_t gain;

  if (!self || !self->oar || group_index >= def_max_sub_mixes ||
      self->gids[group_index] == def_i32_id_none)
    return IAMF_ERR_BAD_ARG;

  gain.type = ck_metadata_gain;
  gain.gain.id = pid;
  gain.gain.param_type = ck_param_animated;

  // Assign animated_gains field
  gain.gain.animated_gains = animated_gain;
  gain.duration = number;

  debug(
      "display gid %d: gain %d: animation type %d: start %f, end %f, control "
      "%f, control_relative_time %f, duration %d",
      self->gids[group_index], pid, animated_gain.animation_type,
      animated_gain.data.start, animated_gain.data.end,
      animated_gain.data.control, animated_gain.data.control_relative_time,
      number);

  return oar_update_metadata(self->oar, self->gids[group_index], &gain) ==
                 ck_oar_ok
             ? IAMF_OK
             : IAMF_ERR_INTERNAL;
}

int iamf_renderer_update_element_animated_polar_positions(
    iamf_renderer_t* self, uint32_t id, uint32_t pid,
    animated_polar_t* positions, uint32_t number, uint32_t duration) {
  oar_metadata_t metadata;

  if (!self || !self->oar || !positions) return IAMF_ERR_BAD_ARG;
  if (number == 0) return IAMF_ERR_BAD_ARG;

  metadata.type = ck_metadata_object_positions;
  metadata.object_positions.param_type = ck_param_animated;
  metadata.object_positions.position_type = ck_polar;

  // Count valid positions and copy animated polar positions
  uint32_t i = 0;
  for (; i < number && i < def_max_number_of_objects; i++) {
    metadata.object_positions.animated_polar_positions[i] = positions[i];
    debug(
        "Update animated polar positions for element %u-%u: num_objects=%u, "
        "duration=%u, index=%u, azimuth=%f, elevation=%f, distance=%f",
        id, pid, metadata.object_positions.num_objects, duration, i,
        positions[i].azimuth.start, positions[i].elevation.start,
        positions[i].distance.start);
  }
  metadata.object_positions.num_objects = i;

  metadata.duration = duration;

  return oar_update_audio_element_metadata(self->oar, id, &metadata) ==
                 ck_oar_ok
             ? IAMF_OK
             : IAMF_ERR_INTERNAL;
}

int iamf_renderer_update_element_animated_cartesian_positions(
    iamf_renderer_t* self, uint32_t id, animated_cartesian_t* positions,
    uint32_t number, uint32_t duration) {
  oar_metadata_t metadata;

  if (!self || !self->oar || !positions) return IAMF_ERR_BAD_ARG;
  if (number == 0) return IAMF_ERR_BAD_ARG;

  metadata.type = ck_metadata_object_positions;
  metadata.object_positions.param_type = ck_param_animated;
  metadata.object_positions.position_type = ck_cartesian;

  // Count valid positions and copy animated cartesian positions
  uint32_t i = 0;
  for (; i < number && i < def_max_number_of_objects; i++) {
    metadata.object_positions.animated_cartesian_positions[i] = positions[i];
    debug(
        "Update animated cartesian positions for element %u: index=%u, x=%f,"
        "y=%f, z=%f",
        id, i, positions[i].x.start, positions[i].y.start,
        positions[i].z.start);
  }
  metadata.object_positions.num_objects = i;

  metadata.duration = duration;

  debug(
      "Update animated cartesian positions for element %u: num_objects=%u, "
      "duration=%u",
      id, metadata.object_positions.num_objects, duration);

  return oar_update_audio_element_metadata(self->oar, id, &metadata) ==
                 ck_oar_ok
             ? IAMF_OK
             : IAMF_ERR_INTERNAL;
}

int iamf_renderer_render(iamf_renderer_t* self, iamf_audio_block_t* out_block) {
  if (!self || !out_block) return IAMF_ERR_BAD_ARG;
  if (!self->oar) return IAMF_ERR_INTERNAL;

  uint32_t output_channels = oar_get_number_of_output_channels(self->oar);
  uint32_t output_samples = oar_get_samples_per_channel(self->oar);

  if (out_block->capacity_per_channel * out_block->num_channels <
      output_samples * output_channels) {
    error(
        "Output buffer size too small for rendering! Expected %ux%u, got %ux%u",
        output_samples, output_channels, out_block->capacity_per_channel,
        out_block->num_channels);
    return IAMF_ERR_BAD_ARG;
  }

  if (out_block->num_channels != output_channels)
    out_block->num_channels = output_channels;

  oar_audio_block_t output_data = {out_block->data, output_channels,
                                   output_samples};

  return oar_render(self->oar, &output_data) == ck_oar_ok ? IAMF_OK
                                                          : IAMF_ERR_INTERNAL;
}

int iamf_renderer_set_head_rotation(iamf_renderer_t* self,
                                    const quaternion_t* quaternion) {
  int ret = IAMF_OK;
  if (!self || !quaternion) return IAMF_ERR_BAD_ARG;
  if (!self->oar) return IAMF_ERR_INTERNAL;

  oar_metadata_t metadata;
  metadata.type = ck_metadata_head_rotation;
  metadata.head_rotation = *quaternion;
  metadata.duration = 0;

  debug("Set head rotation in renderer: w=%f, x=%f, y=%f, z=%f", quaternion->w,
        quaternion->x, quaternion->y, quaternion->z);

  for (int i = 0; i < def_max_sub_mixes; i++) {
    if (self->gids[i] != def_i32_id_none) {
      if (oar_update_metadata(self->oar, self->gids[i], &metadata) != ck_oar_ok)
        ret = IAMF_ERR_INTERNAL;
    }
  }
  return ret;
}

int iamf_renderer_enable_head_tracking(iamf_renderer_t* self, uint32_t enable) {
  if (!self) return IAMF_ERR_BAD_ARG;

  debug("Head tracking %s in renderer", enable ? "enabled" : "disabled");
  return oar_enable_head_tracking(self->oar, enable) == ck_oar_ok
             ? IAMF_OK
             : IAMF_ERR_INTERNAL;
}

int iamf_renderer_enable_loudness_processor(iamf_renderer_t* self,
                                            uint32_t enable) {
  if (!self) return IAMF_ERR_BAD_ARG;

  debug("Loudness processor %s in renderer", enable ? "enabled" : "disabled");
  return oar_enable_loudness_processor(self->oar, enable) == ck_oar_ok
             ? IAMF_OK
             : IAMF_ERR_INTERNAL;
}

int iamf_renderer_set_loudness(iamf_renderer_t* self, uint32_t group_index,
                               float loudness, float target_loudness) {
  if (!self || !self->oar) return IAMF_ERR_BAD_ARG;
  if (group_index >= def_max_sub_mixes ||
      self->gids[group_index] == def_i32_id_none) {
    error("Invalid group index %u for loudness setting", group_index);
    return IAMF_ERR_BAD_ARG;
  }

  debug("Set loudness for group %u (id=%u): current=%.2f dB, target=%.2f dB",
        group_index, self->gids[group_index], loudness, target_loudness);

  return oar_set_loudness(self->oar, self->gids[group_index], loudness,
                          target_loudness) == ck_oar_ok
             ? IAMF_OK
             : IAMF_ERR_INTERNAL;
}
