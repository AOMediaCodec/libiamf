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
 * @file iamf_element_reconstructor.h
 * @brief IAMF element reconstructor APIs.
 * @version 0.1
 * @date Created 12/11/2025
 **/

#ifndef __IAMF_ELEMENT_RECONSTRUCTOR_H__
#define __IAMF_ELEMENT_RECONSTRUCTOR_H__

#include <stddef.h>
#include <stdint.h>

#include "iamf_audio_block.h"
#include "iamf_obu_all.h"

typedef struct IamfElementReconstructor iamf_element_reconstructor_t;

/**
 * @brief Create an iamf_element_reconstructor_t instance.
 *
 * @return iamf_element_reconstructor_t* Success: pointer to the new instance;
 * Failure: NULL.
 */
iamf_element_reconstructor_t* iamf_element_reconstructor_create();

/**
 * @brief Destroy an iamf_element_reconstructor_t instance and release its
 * internal resources.
 *
 * @param reconstructor Pointer to the iamf_element_reconstructor_t instance to
 * destroy.
 */
void iamf_element_reconstructor_destroy(iamf_element_reconstructor_t* self);

/**
 * @brief Add an Audio Element to be reconstructed.
 *
 * This function will create a specific reconstructor (SceneBased, ChannelBased,
 * or ObjectBased) based on the iamf_audio_element_obu_t type and store it
 * internally.
 *
 * @param reconstructor iamf_element_reconstructor_t instance.
 * @param audio_element_obu Pointer to the Audio Element OBU.
 * @param codec_config_obu Pointer to the associated Codec Config OBU.
 * @return int Success: 0; Failure: error code.
 */
int iamf_element_reconstructor_add_element(
    iamf_element_reconstructor_t* self,
    const iamf_audio_element_obu_t* audio_element_obu,
    const iamf_codec_config_obu_t* codec_config_obu);

/**
 * @brief Receive and process a raw OBU stream.
 *
 * This function uses the internal IamfObuParser to parse the OBU stream and
 * then dispatches it to the appropriate specific reconstructor based on the OBU
 * type (e.g., Audio Frame, Parameter Block).
 *
 * @param reconstructor iamf_element_reconstructor_t instance.
 * @return int Success: 0; Failure: error code.
 */
int iamf_element_reconstructor_add_audio_frame_obu(
    iamf_element_reconstructor_t* self, iamf_audio_frame_obu_t* obu);

iamf_audio_block_t* iamf_element_reconstructor_process(
    iamf_element_reconstructor_t* self);

iamf_audio_block_t* iamf_element_reconstructor_flush(
    iamf_element_reconstructor_t* self, uint32_t id);

/**
 * @brief Update demixing info for a Channel-based Element.
 *
 * @param reconstructor iamf_element_reconstructor_t instance.
 * @param audio_element_id The ID of the Audio Element to update.
 * @param demixing_info Pointer to the new Demixing Info.
 * @return int Success: 0; Failure: error code (e.g., element not found or not
 * channel-based).
 */
int iamf_element_reconstructor_update_demixing_mode(
    iamf_element_reconstructor_t* self, int audio_element_id,
    int demixing_mode);

/**
 * @brief Update recon gain for a Channel-based Element.
 *
 * @param reconstructor iamf_element_reconstructor_t instance.
 * @param audio_element_id The ID of the Audio Element to update.
 * @param recon_gain Pointer to the new Recon Gain.
 * @return int Success: 0; Failure: error code (e.g., element not found or not
 * channel-based).
 */
int iamf_element_reconstructor_update_recon_gain(
    iamf_element_reconstructor_t* self, int audio_element_id,
    const recon_gain_t* recon_gain);

/**
 * @brief Set the target layout for a Channel-based Element.
 *
 * @param reconstructor iamf_element_reconstructor_t instance.
 * @param audio_element_id The ID of the Audio Element to update.
 * @param target_layout The target loudspeaker layout to set.
 * @return int Success: 0; Failure: error code (e.g., element not found or not
 * channel-based).
 */
int iamf_element_reconstructor_set_channel_based_target_layout(
    iamf_element_reconstructor_t* self, int audio_element_id,
    iamf_loudspeaker_layout_t target_layout);

iamf_loudspeaker_layout_t
iamf_element_reconstructor_get_channel_based_reconstructed_layout(
    iamf_element_reconstructor_t* self, int audio_element_id);

int iamf_element_reconstructor_get_channel_based_reconstructed_layout_index(
    iamf_element_reconstructor_t* self, int audio_element_id);
/**
 * @brief Activate a ReconstructorEntry for an Audio Element.
 *
 * This function activates the ReconstructorEntry associated with the given
 * audio_element_id, making it ready for processing audio frames.
 *
 * @param reconstructor iamf_element_reconstructor_t instance.
 * @param audio_element_id The ID of the Audio Element to activate.
 * @return int Success: 0; Failure: error code (e.g., element not found).
 */
int iamf_element_reconstructor_activate_element(
    iamf_element_reconstructor_t* self, int audio_element_id);

#endif  // __IAMF_ELEMENT_RECONSTRUCTOR_H__
