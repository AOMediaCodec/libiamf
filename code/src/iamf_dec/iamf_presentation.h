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
 * @file iamf_presentation.h
 * @brief IAMF presentation API.
 * @version 2.0.0
 * @date Created 05/09/2025
 **/

#ifndef __IAMF_PRESENTATION_H__
#define __IAMF_PRESENTATION_H__

#include "IAMF_defines.h"
#include "chashmap.h"
#include "iamf_audio_block.h"
#include "iamf_database.h"
#include "iamf_element_reconstructor.h"
#include "iamf_obu_parser.h"
#include "iamf_renderer.h"
#include "iamf_types.h"
#include "obu/mix_presentation_obu.h"

typedef struct IamfPresentation iamf_presentation_t;

iamf_presentation_t* iamf_presentation_create(
    uint32_t id, iamf_database_t* database,
    iamf_element_reconstructor_t* reconstructor, iamf_layout_t layout);

void iamf_presentation_destroy(iamf_presentation_t* self);

int iamf_presentation_set_element_gain_offset(iamf_presentation_t* self,
                                              uint32_t eid, float offset);
int iamf_presentation_get_element_gain_offset(iamf_presentation_t* self,
                                              uint32_t eid, float* offset);
int iamf_presentation_set_loudness_gain(iamf_presentation_t* self, float gain);

uint32_t iamf_presentation_get_id(iamf_presentation_t* self);
int iamf_presentation_get_sampling_rate(iamf_presentation_t* self);

int iamf_presentation_add_audio_block(iamf_presentation_t* self,
                                      iamf_audio_block_t* audio_block);

iamf_audio_block_t* iamf_presentation_process(iamf_presentation_t* self);

/**
 * @brief Set head rotation for 3D audio rendering in the presentation.
 *
 * This function sets the head rotation using quaternion representation to
 * counter-rotate the intermediate Ambisonic bed, producing stable sound sources
 * in binaural reproduction. The head rotation is essential for immersive 3D
 * audio experiences where the listener's head orientation affects audio
 * perception.
 *
 * @param     [in] self : Pointer to the presentation instance.
 * @param     [in] quaternion : Pointer to the quaternion structure containing
 * head rotation.
 *
 * @return    0 on success, non-zero error code on failure.
 *
 * @note      This function should be called regularly during audio playback to
 * update the head position based on sensor data from head tracking devices. The
 * head rotation data is passed to the renderer for processing during audio
 * rendering to create realistic 3D audio spatialization.
 *
 * @warning   Head rotation functionality is only effective for binaural output
 * layout. For other output layouts (stereo, multichannel), head rotation
 * settings will be stored but will not affect the audio output.
 */
int iamf_presentation_set_head_rotation(iamf_presentation_t* self,
                                        const quaternion_t* quaternion);

/**
 * @brief Enable or disable head tracking for 3D audio rendering in the
 * presentation.
 *
 * This function enables or disables head tracking functionality. When enabled,
 * the renderer will use head rotation data to create immersive 3D audio
 * experiences.
 *
 * @param     [in] self : Pointer to the presentation instance.
 * @param     [in] enable : 1 to enable head tracking, 0 to disable.
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
int iamf_presentation_enable_head_tracking(iamf_presentation_t* self,
                                           uint32_t enable);

#endif  // __IAMF_PRESENTATION_H__
