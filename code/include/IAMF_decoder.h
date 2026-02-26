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
 * @file IAMF_decoder.h
 * @brief IAMF decoder APIs.
 * @version 2.0.0
 * @date Created 03/03/2023
 **/

#ifndef __IAMF_DECODER_H__
#define __IAMF_DECODER_H__

#include <stdint.h>

#include "IAMF_defines.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Audio element gain offset range information.
 *
 * This structure defines the valid range for gain adjustment of a specific
 * audio element within a mix presentation. It allows users to adjust the
 * gain of individual audio elements within specified limits.
 */
typedef struct IamfElementGainOffsetRange {
  float min; /**< Minimum gain offset value in dB */
  float max; /**< Maximum gain offset value in dB */
} iamf_element_gain_offset_range_t;

typedef struct IamfElementPresentationInfo {
  uint32_t eid;                       /**< Audio element identifier */
  IAMF_HeadphonesRenderingMode mode;  /**< Headphones rendering mode */
  IAMF_BinauralFilterProfile profile; /**< Binaural filter profile */
  iamf_element_gain_offset_range_t *gain_offset_range; /**< Gain offset range */
} iamf_element_presentation_info_t;

/**
 * @brief Mix presentation information structure.
 *
 * This structure contains comprehensive information about a specific mix
 * presentation within an IAMF stream. Mix presentations define how different
 * audio elements are combined, processed, and presented to the listener,
 * supporting various output configurations including stereo, multichannel, and
 * binaural rendering.
 *
 * Each mix presentation can contain multiple audio elements with different
 * rendering modes, gain adjustments, and spatial audio processing parameters.
 * This structure provides the metadata needed for applications to understand
 * and select appropriate mix presentations based on playback capabilities and
 * user preferences.
 *
 * The structure includes information about audio elements such as their
 * identifiers, rendering modes (stereo, binaural), binaural filter profiles,
 * and gain offset ranges for user customization.
 */
typedef struct IamfMixPresentationInfo {
  uint32_t id; /**< Mix presentation identifier. Unique identifier for this mix
                  presentation within the IAMF stream. */
  uint32_t num_audio_elements; /**< Number of audio elements in this mix
                                  presentation. Indicates the size of the
                                  elements array. */
  /**< Array of audio element presentation information. Contains detailed
   * information about each audio element including rendering mode, binaural
   * profile, and gain settings. */
  iamf_element_presentation_info_t *elements;
} iamf_mix_presentation_info_t;

#define IAMF_MAX_CODECS 2

/**
 * @brief IAMF stream information structure.
 *
 * This structure contains comprehensive information about an IAMF stream,
 * including codec configuration, audio parameters, and available mix
 * presentations. It is returned by @ref IAMF_decoder_get_stream_info and
 * provides details about the stream's capabilities and configuration.
 */
typedef struct IAMF_StreamInfo {
  uint32_t max_frame_size; /**< Maximum frame size in samples */

  struct {
    IA_Profile primary_profile;    /**< Primary IAMF profile */
    IA_Profile additional_profile; /**< Additional IAMF profile */

    IAMF_CodecID codec_ids[IAMF_MAX_CODECS];
    IAMF_SampleBitDepth sampling_rate; /**< Output sampling rate */
    /**< Number of samples per channel per frame */
    uint32_t samples_per_channel_in_frame;

    /**< Number of available mix presentations */
    uint32_t mix_presentation_count;
    /**< Array of mix presentation information */
    iamf_mix_presentation_info_t *mix_presentations;
  } iamf_stream_info;

} IAMF_StreamInfo;

typedef struct IAMF_StreamInfo iamf_stream_info_t;

/**@}*/
/**\name Immersive audio decoder functions */
/**@{*/

typedef struct IAMF_Decoder *IAMF_DecoderHandle;

/**
 * @brief     Open an iamf decoder.
 * @return    return an iamf decoder handle.
 */
IAMF_DecoderHandle IAMF_decoder_open(void);

/**
 * @brief     Close an iamf decoder.
 * @param     [in] handle : iamf decoder handle.
 */
int IAMF_decoder_close(IAMF_DecoderHandle handle);

/**
 * @brief     Set the IAMF profile for the decoder.
 *
 * This function sets the IAMF profile that the decoder will use for processing
 * audio data. The profile determines the features and capabilities that the
 * decoder will support. The profile must be set before configuring the decoder
 * with descriptor OBUs.
 *
 * @param     [in] handle : IAMF decoder handle.
 * @param     [in] profile : The IAMF profile to set. Must be one of the values
 *                           defined in @ref IA_Profile.
 * @return    @ref IAErrCode. IAMF_OK on success, IAMF_ERR_BAD_ARG on invalid
 * parameters.
 *
 * @note      The profile must be set before calling @ref IAMF_decoder_configure
 * with descriptor OBUs. Setting the profile after configuration may not have
 * the desired effect.
 */
int IAMF_decoder_set_profile(IAMF_DecoderHandle handle, IA_Profile profile);

/**
 * @brief     Get the current IAMF profile of the decoder.
 *
 * This function retrieves the currently set IAMF profile of the decoder. The
 * profile determines the features and capabilities that the decoder supports.
 *
 * @param     [in] handle : IAMF decoder handle.
 * @return    The current IAMF profile, or ck_iamf_profile_none if the handle
 *            is invalid.
 *
 * @note      The returned profile may be different from the one set by
 *            @ref IAMF_decoder_set_profile if the decoder has been configured
 * with an IA Sequence Header OBU that specifies a different profile.
 */
IA_Profile IAMF_decoder_get_profile(IAMF_DecoderHandle handle);

/**
 * @brief     Configurate an iamf decoder. The first configurating decoder must
 *            need descriptor OBUs, then if only some properties have been
 *            changed, @ref IAMF_decoder_set_mix_presentation_label API. the
 *            descriptor OBUs is not needed.
 * @param     [in] handle : iamf decoder handle.
 * @param     [in] data : the bitstream.
 * @param     [in] size : the size in bytes of bitstream.
 * @param     [in & out] rsize : the size in bytes of bitstream that has been
 *                               consumed. If is null, it means the data is the
 *                               complete configuration OBUs.
 * @return    @ref IAErrCode.
 */
int IAMF_decoder_configure(IAMF_DecoderHandle handle, const uint8_t *data,
                           uint32_t size, uint32_t *rsize);

/**
 * @brief     Decode bitstream.
 * @param     [in] handle : iamf decoder handle.
 * @param     [in] data : the OBUs in bitstream.
 *                        if is null, the output is delay signal.
 * @param     [in] size : the size in bytes of bitstream.
 * @param     [in & out] rsize : the size in bytes of bitstream that has been
 *                               consumed. If it is null, it means the data is
 *                               a complete access unit which includes all OBUs
 *                               of substream frames and parameters.
 * @param     [out] pcm : output signal.
 * @return    the number of decoded samples or @ref IAErrCode.
 */
int IAMF_decoder_decode(IAMF_DecoderHandle handle, const uint8_t *data,
                        int32_t size, uint32_t *rsize, void *pcm);

/**
 * @brief     Set mix presentation id to select the mix that match the user's
 *            preferences.
 * @param     [in] handle : iamf decoder handle.
 * @param     [in] id : an identifier for a Mix Presentation.
 * @return    @ref IAErrCode.
 */
int IAMF_decoder_set_mix_presentation_id(IAMF_DecoderHandle handle,
                                         uint64_t id);

/**
 * @brief     Get selected mix presentation id.
 * @param     [in] handle : iamf decoder handle.
 * @return    valid id ( >= 0) or -1.
 */
int64_t IAMF_decoder_get_mix_presentation_id(IAMF_DecoderHandle handle);

/**
 * @brief     Set sound system output layout.
 * @param     [in] handle : iamf decoder handle.
 * @param     [in] ss : the sound system (@ref IAMF_SoundSystem).
 * @return    @ref IAErrCode.
 */
int IAMF_decoder_output_layout_set_sound_system(IAMF_DecoderHandle handle,
                                                IAMF_SoundSystem ss);

/**
 * @brief     Set binaural output layout.
 * @param     [in] handle : iamf decoder handle.
 * @return    @ref IAErrCode.
 */
int IAMF_decoder_output_layout_set_binaural(IAMF_DecoderHandle handle);

/**
 * @brief     Get the number of channels of the sound system.
 * @param     [in] ss : the sound system (@ref IAMF_SoundSystem).
 * @return    the number of channels.
 */
int IAMF_layout_sound_system_channels_count(IAMF_SoundSystem ss);

/**
 * @brief     Get the number of channels of binaural pattern.
 * @return    the number of channels.
 */
int IAMF_layout_binaural_channels_count(void);

/**
 * @brief     Get the codec capability of iamf. Need to free string manually.
 *            The codec string format will be:
 *            "iamf.xxx.yyy.Opus;iamf.xxx.yyy.mp4a.40.2" Where xxx is three
 *            digits to indicate the value of the primary_profile and yyy
 *            is three digits to indicate the value of the additional_profile.
 * @return    the supported codec string.
 */
char *IAMF_decoder_get_codec_capability(void);

/**
 * @brief     Set target normalization loudness value, then loudness will be
 *            adjusted to the setting target.
 * @param     [in] handle : iamf decoder handle.
 * @param     [in] loundness : target normalization loudness in LKFS.
 *                             0 dose not do normalization,
 *                             others(<0) target value of normalization.
 * @return    @ref IAErrCode.
 */
int IAMF_decoder_set_normalization_loudness(IAMF_DecoderHandle handle,
                                            float loudness);

/**
 * @brief     Set pcm output bitdepth.
 * @param     [in] handle : iamf decoder handle.
 * @param     [in] bit_depth : target bit depth in bit. Must be one of the
 *                             values defined in @ref iamf_sample_bit_depth_t
 *                             (16, 24, or 32 bits).
 * @return    @ref IAErrCode.
 */
int IAMF_decoder_set_bit_depth(IAMF_DecoderHandle handle, uint32_t bit_depth);

/**
 * @brief     Enable peak limiter. In the decoder, the peak limiter is enabled
 *            by default.
 * @param     [in] handle : iamf decoder handle.
 * @param     [in] enable : 1 indicates enabled, and 0 indicates disable.
 * @return    @ref IAErrCode.
 */
int IAMF_decoder_peak_limiter_enable(IAMF_DecoderHandle handle,
                                     uint32_t enable);

/**
 * @brief     Set peak threshold value of peak limiter.
 * @param     [in] handle : iamf decoder handle.
 * @param     [in] db : peak threshold in dB.
 * @return    @ref IAErrCode.
 */
int IAMF_decoder_peak_limiter_set_threshold(IAMF_DecoderHandle handle,
                                            float db);

/**
 * @brief     Get peak threshold value of peak limiter.
 * @param     [in] handle : iamf decoder handle.
 * @return    Peak threshold in dB.
 */
float IAMF_decoder_peak_limiter_get_threshold(IAMF_DecoderHandle handle);

/**
 * @brief     Set pcm output sampling rate.
 * @param     [in] handle : iamf decoder handle.
 * @param     [in] rate : sampling rate. Must be one of the values defined in
 *                        @ref IAMF_SampleBitDepth (7350, 8000, 11025, 16000,
 *                        22050, 24000, 32000, 44100, 48000, 64000, 88200,
 *                        96000, 176400, or 192000 Hz).
 * @return    @ref IAErrCode.
 */
int IAMF_decoder_set_sampling_rate(IAMF_DecoderHandle handle, uint32_t rate);

/**
 * @brief     Set gain offset for a specific audio element.
 * @param     [in] handle : iamf decoder handle.
 * @param     [in] id : audio element identifier.
 * @param     [in] gain : gain offset value in dB.
 * @return    @ref IAErrCode.
 */
int IAMF_decoder_set_audio_element_gain_offset(IAMF_DecoderHandle handle,
                                               uint32_t id, float gain);

/**
 * @brief     Get gain offset for a specific audio element.
 * @param     [in] handle : iamf decoder handle.
 * @param     [in] id : audio element identifier.
 * @param     [out] gain : pointer to store the gain offset value in dB.
 * @return    @ref IAErrCode.
 */
int IAMF_decoder_get_audio_element_gain_offset(IAMF_DecoderHandle handle,
                                               uint32_t id, float *gain);

/**
 * @brief     Enable or disable head tracking for 3D audio rendering.
 *
 * This function enables or disables head tracking functionality. When enabled,
 * the decoder will use head rotation data to create immersive 3D audio
 * experiences. Head tracking must be enabled before setting head rotation
 * values.
 *
 * @param     [in] handle : IAMF decoder handle.
 * @param     [in] enable : 1 to enable head tracking, 0 to disable.
 * @return    @ref IAErrCode.
 */
int IAMF_decoder_enable_head_tracking(IAMF_DecoderHandle handle,
                                      uint32_t enable);

/**
 * @brief     Set the head rotation for 3D audio rendering in world space.
 *
 * This function sets the head rotation using quaternion representation to
 * counter-rotate the intermediate Ambisonic bed, producing stable sound sources
 * in binaural reproduction. The head rotation is essential for immersive 3D
 * audio experiences where the listener's head orientation affects audio
 * perception.
 *
 * The function uses a right-handed coordinate system with the following
 * reference frame:
 * - X axis: points to the right
 * - Y axis: points up
 * - Z axis: points backward (swap sign of z if using front-facing Z coordinate)
 *
 * The quaternion will be automatically normalized if not unit length. Invalid
 * quaternions (too close to zero) will be rejected.
 *
 * @param     [in] handle : IAMF decoder handle.
 * @param     [in] w : Quaternion scalar component (W).
 * @param     [in] x : Quaternion X component.
 * @param     [in] y : Quaternion Y component.
 * @param     [in] z : Quaternion Z component.
 * @return    @ref IAErrCode. IAMF_OK on success, IAMF_ERR_BAD_ARG on invalid
 * parameters.
 *
 * @note      This function should be called regularly during audio playback to
 * update the head position based on sensor data from head tracking devices. The
 * head rotation data is used during the rendering process to create realistic
 * 3D audio spatialization in world space.
 *
 * @warning   Head rotation functionality is only effective for binaural output
 * layout. The decoder must be configured for binaural output using
 *            @ref IAMF_decoder_output_layout_set_binaural() before calling this
 * function. For other output layouts (stereo, multichannel), head rotation
 * settings will be stored but will not affect the audio output.
 */
int IAMF_decoder_set_head_rotation(IAMF_DecoderHandle handle, float w, float x,
                                   float y, float z);

/**
 * @brief     Get comprehensive IAMF stream information.
 *
 * This function retrieves detailed information about the configured IAMF
 * stream, including codec parameters, audio format specifications, and
 * available mix presentations. The stream information is essential for
 * applications to understand the capabilities and configuration of the audio
 * stream for proper playback setup.
 *
 * The function must be called after successful decoder configuration using
 * @ref IAMF_decoder_configure. It returns a pointer to an @ref IAMF_StreamInfo
 * structure containing:
 * - Maximum frame size for buffer allocation
 * - IAMF profile information (primary and additional)
 * - Codec types and parameters (Opus, AAC, FLAC, etc.)
 * - Sampling rate and frame size information
 * - Available mix presentations with their audio elements
 * - Audio element rendering modes and gain settings
 *
 * @param     [in] handle : IAMF decoder handle. Must be a valid handle returned
 * by
 *                         @ref IAMF_decoder_open and successfully configured.
 *
 * @return    Pointer to allocated @ref IAMF_StreamInfo structure containing
 * stream information, or NULL if the decoder is not configured or an error
 * occurs.
 *
 * @note      The returned structure and all its nested arrays are dynamically
 * allocated. After using the stream information, the caller MUST free the
 * allocated memory by calling @ref IAMF_decoder_free_stream_info to prevent
 * memory leaks. The stream information remains valid until the decoder is
 * reconfigured or closed.
 *
 * @warning   Do not attempt to free the returned structure manually using
 * standard free() as it contains nested allocations that require proper
 * cleanup. Always use @ref IAMF_decoder_free_stream_info for proper resource
 * management.
 */
IAMF_StreamInfo *IAMF_decoder_get_stream_info(IAMF_DecoderHandle handle);

/**
 * @brief     Free the memory allocated for stream info.
 * @param     [in] info : pointer to the stream info structure to be freed.
 */
void IAMF_decoder_free_stream_info(iamf_stream_info_t *info);

#ifdef __cplusplus
}
#endif

#endif /* __IAMF_DECODER_H__ */
