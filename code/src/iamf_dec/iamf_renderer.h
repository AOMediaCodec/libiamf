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
 * @file iamf_renderer.h
 * @brief renderer API.
 * @version 2.0.0
 * @date Created 12/11/2025
 **/

#ifndef __IAMF_RENDERER_H__
#define __IAMF_RENDERER_H__

#include "IAMF_defines.h"
#include "chashmap.h"
#include "iamf_audio_block.h"
#include "iamf_private_definitions.h"
#include "iamf_types.h"
#include "oar.h"
#include "oar_metadata.h"
#include "parameter_block_obu.h"

typedef struct IamfOar {
  oar_t* oar;
  int gids[def_max_sub_mixes];
  int samples_per_frame;
} iamf_renderer_t;

typedef struct IamfRenderingParam {
  uint32_t headphones_rendering_mode;
  uint32_t binaural_filter_profile;
} iamf_rendering_param_t;

#define def_rendering_param_instance(headphone_mode, binaural_profile) \
  (iamf_rendering_param_t) {                                           \
    .headphones_rendering_mode = headphone_mode,                       \
    .binaural_filter_profile = binaural_profile                        \
  }

int iamf_renderer_init(iamf_renderer_t* self, iamf_layout_t layout,
                       uint32_t number_samples_per_frame,
                       uint32_t sampling_rate);
void iamf_renderer_uninit(iamf_renderer_t* self);
int iamf_renderer_add_channel_based_element(iamf_renderer_t* self, uint32_t id,
                                            uint32_t group_index,
                                            iamf_loudspeaker_layout_t layout,
                                            iamf_rendering_param_t param,
                                            int downmix_mode,
                                            int downmix_weight_index);
int iamf_renderer_add_scene_based_element(iamf_renderer_t* self, uint32_t id,
                                          uint32_t group_index,
                                          uint32_t number_channels,
                                          iamf_rendering_param_t param);
int iamf_renderer_add_object_based_element(iamf_renderer_t* self, uint32_t id,
                                           uint32_t group_index,
                                           uint32_t number_objects,
                                           iamf_rendering_param_t param);
int iamf_renderer_add_element_audio_data(iamf_renderer_t* self,
                                         iamf_audio_block_t* block);
int iamf_renderer_update_element_animated_gain(iamf_renderer_t* self,
                                               uint32_t id, uint32_t pid,
                                               animated_float32_t animated_gain,
                                               uint32_t number);
int iamf_renderer_update_element_downmix_mode(iamf_renderer_t* self,
                                              uint32_t id, int mode,
                                              int period);
int iamf_renderer_update_animated_gain(iamf_renderer_t* self, uint32_t pid,
                                       uint32_t group_index,
                                       animated_float32_t animated_gain,
                                       uint32_t number);
int iamf_renderer_update_element_animated_polar_positions(
    iamf_renderer_t* self, uint32_t id, uint32_t pid,
    animated_polar_t* positions, uint32_t number, uint32_t duration);
int iamf_renderer_update_element_animated_cartesian_positions(
    iamf_renderer_t* self, uint32_t id, animated_cartesian_t* positions,
    uint32_t number, uint32_t duration);

int iamf_renderer_render(iamf_renderer_t* self, iamf_audio_block_t* out_block);

/**
 * @brief Set head rotation for 3D audio rendering.
 *
 * This function sets the head rotation using quaternion representation to
 * counter-rotate the intermediate Ambisonic bed, producing stable sound sources
 * in binaural reproduction. The head rotation is essential for immersive 3D
 * audio experiences where the listener's head orientation affects audio
 * perception.
 *
 * @param     [in] self : Pointer to the renderer instance.
 * @param     [in] quaternion : Pointer to the quaternion structure containing
 * head rotation.
 *
 * @return    0 on success, non-zero error code on failure.
 *
 * @note      This function should be called regularly during audio playback to
 * update the head position based on sensor data from head tracking devices. The
 * head rotation data is used during the rendering process to create realistic
 * 3D audio spatialization.
 *
 * @warning   Head rotation functionality is only effective for binaural output
 * layout. For other output layouts (stereo, multichannel), head rotation
 * settings will be stored but will not affect the audio output.
 */
int iamf_renderer_set_head_rotation(iamf_renderer_t* self,
                                    const quaternion_t* quaternion);

/**
 * @brief Enable or disable head tracking for 3D audio rendering.
 *
 * This function enables or disables head tracking functionality. When enabled,
 * the renderer will use head rotation data to create immersive 3D audio
 * experiences.
 *
 * @param     [in] self : Pointer to the renderer instance.
 * @param     [in] enable : 1 to enable, 0 to disable.
 *
 * @return    0 on success, non-zero error code on failure.
 *
 * @note      Head tracking must be enabled before setting head rotation values.
 * This function should be called before starting audio playback or when
 * dynamically enabling/disabling head tracking during playback.
 *
 * @warning   Head tracking functionality is only effective for binaural output
 * layout. For other output layouts (stereo, multichannel), this setting
 * will be stored but will not affect the audio output.
 */
int iamf_renderer_enable_head_tracking(iamf_renderer_t* self, uint32_t enable);

/**
 * @brief Enable or disable loudness processor.
 *
 * This function enables or disables the loudness normalization processor in
 * OAR. When enabled, OAR will apply group-specific loudness gain during
 * rendering to normalize the audio to a target loudness level.
 *
 * @param     [in] self : Pointer to the renderer instance.
 * @param     [in] enable : 1 to enable, 0 to disable.
 *
 * @return    0 on success, non-zero error code on failure.
 */
int iamf_renderer_enable_loudness_processor(iamf_renderer_t* self,
                                            uint32_t enable);

/**
 * @brief Set loudness parameters for a specific audio group.
 *
 * This function sets the current loudness and target loudness for a specific
 * audio group in OAR. The loudness processor will use these parameters to
 * calculate and apply the appropriate gain during rendering.
 *
 * @param     [in] self : Pointer to the renderer instance.
 * @param     [in] group_index : Group index (0 or 1) to set loudness for.
 * @param     [in] loudness : Current loudness in dB (LKFS).
 * @param     [in] target_loudness : Target loudness in dB (LKFS).
 *
 * @return    0 on success, non-zero error code on failure.
 */
int iamf_renderer_set_loudness(iamf_renderer_t* self, uint32_t group_index,
                               float loudness, float target_loudness);

#endif  // __IAMF_RENDERER_H__
