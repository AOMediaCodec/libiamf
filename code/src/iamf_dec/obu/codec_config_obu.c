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
 * @file codec_config_obu.c
 * @brief Codec config OBU implementation.
 * @version 2.0.0
 * @date Created 12/11/2025
 **/

#include "codec_config_obu.h"

#include "iamf_private_definitions.h"
#include "iamf_string.h"
#include "iamf_utils.h"

#ifdef SUPPORT_VERIFIER
#include "vlogging_tool_sr.h"
#endif

#undef def_log_tag
#define def_log_tag "OBU_CC"

static int _obu_cc_check(iamf_codec_config_obu_t *obu);
static int _obu_cc_decoder_config_parse(iamf_codec_type_t type,
                                        io_context_t *ioc,
                                        audio_codec_parameter_t *param);

iamf_codec_config_obu_t *iamf_codec_config_obu_new(io_context_t *ior) {
  iamf_codec_config_obu_t *obu = 0;
  io_context_t *r = ior;
  uint32_t decoder_config_size = 0;

  obu = def_mallocz(iamf_codec_config_obu_t, 1);
  if (!obu) {
    def_err_msg_enomem("Codec Config OBU", def_iamf_ustr);
    return 0;
  }

  obu->obu.obu_type = ck_iamf_obu_codec_config;

  obu->codec_config_id = ior_leb128_u32(r);
  ior_read(r, (uint8_t *)&obu->codec_id, 4);
  obu->num_samples_per_frame = ior_leb128_u32(r);
  obu->audio_roll_distance = (int16_t)ior_b16(r);
  decoder_config_size = r->size - r->idx;
  obu->decoder_config = buffer_wrap_new(decoder_config_size);

  if (!obu->decoder_config) {
    def_err_msg_enomem("decoder config", "Codec Config OBU");
    iamf_codec_config_obu_free(obu);
    return 0;
  }

  ior_read(r, obu->decoder_config->data, decoder_config_size);

  info(
      "Codec cofing id(%u), codec id(%.4s), samples per frame(%u), "
      "roll distance (%d), decoder config size(%u)",
      obu->codec_config_id, (char *)&obu->codec_id, obu->num_samples_per_frame,
      obu->audio_roll_distance, obu->decoder_config->size);

#if SUPPORT_VERIFIER
  vlog_obu(ck_iamf_obu_codec_config, obu, 0, 0);
#endif

  if (_obu_cc_check(obu) != def_pass) {
    iamf_codec_config_obu_free(obu);
    return 0;
  }

  return obu;
}

void iamf_codec_config_obu_free(iamf_codec_config_obu_t *obu) {
  if (obu->decoder_config) buffer_wrap_free(obu->decoder_config);
  free(obu);
}

int iamf_codec_config_obu_get_parameter(iamf_codec_config_obu_t *obu,
                                        audio_codec_parameter_t *param) {
  io_context_t ioc, *r;
  int ret;

  if (!obu || !param) return IAMF_ERR_BAD_ARG;

  r = &ioc;

  param->type = iamf_codec_type_get(obu->codec_id);
  param->frame_size = obu->num_samples_per_frame;

  ioc_init(&ioc, obu->decoder_config->data, obu->decoder_config->size);

  ret = _obu_cc_decoder_config_parse(param->type, r, param);
  if (ret != IAMF_OK) return ret;

  return IAMF_OK;
}

static int _obu_cc_codec_id_check(uint32_t codec_id) {
  return iamf_codec_id_check(codec_id) ? def_pass : def_error;
}

#define def_opus_version_max 15
static int _obu_cc_decoder_config_check(uint32_t codec, buffer_wrap_t *buffer) {
  if (iamf_codec_type_get(codec) == ck_iamf_codec_type_opus) {
    if (buffer && buffer->data[0] > def_opus_version_max) {
      warning("Opus config invalid: version %u should less than %u.",
              buffer->data[0], def_opus_version_max);
      return def_error;
    }
  }

  return def_pass;
}

int _obu_cc_check(iamf_codec_config_obu_t *obu) {
  if (_obu_cc_codec_id_check(obu->codec_id) != def_pass) {
    warning("Codec config id(%u), invalid codec(%.4s)", obu->codec_config_id,
            (char *)&obu->codec_id);
    return def_error;
  }

  if (!obu->num_samples_per_frame) {
    error(
        "Codec config id(%u), number of samples per frame should not be zero.",
        obu->codec_config_id);
    return def_error;
  }

  if (_obu_cc_decoder_config_check(obu->codec_id, obu->decoder_config) !=
      def_pass) {
    warning("Codec config id(%u), codec(%.4s),  decoder config is invalid, ",
            obu->codec_config_id, (char *)&obu->codec_id);
    return def_error;
  }

  return def_pass;
}

static int _obu_cc_opus_decoder_config_parse(io_context_t *ioc,
                                             audio_codec_parameter_t *param) {
  /**
   * @brief OPUS Specific
   * b08: version
   * b08: channel count
   * b16: pre-skip
   * b32: input sample rate
   * b16: output gain
   * b08: mapping family
   */

  // https://aomediacodec.github.io/iamf/v1.1.0.html#opus-specific
  // The sample rate used for computing offsets SHALL be 48k Hz.
  param->sample_rate = def_default_sampling_rate;
  param->bits_per_sample = SAMPLE_BIT_DEPTH_16;
  return IAMF_OK;
}

static int _obu_cc_aac_decoder_config_parse(io_context_t *ioc,
                                            audio_codec_parameter_t *param) {
  int ret, type;
  io_context_t *r = ioc;
  bits_io_context_t bits_ioc, *bits_r;
  static uint32_t sf[] = {96000, 88200, 64000, 48000, 44100, 32000,
                          24000, 22050, 16000, 12000, 11025, 8000,
                          7350,  0,     0,     0};
  bits_r = &bits_ioc;
  bits_ioc_init(bits_r, r);

  /* DecoderConfigDescriptor */
  ior_8(r);
  ior_expandable(r);
  ior_skip(r, 13);

  /* DecSpecificInfoTag */
  ior_8(r);
  ior_expandable(r);

  type = bits_ior_le32(bits_r, 5);
  if (type == 31) type = bits_ior_le32(bits_r, 6);

  ret = bits_ior_le32(bits_r, 4);
  if (ret == 0xf)
    param->sample_rate = bits_ior_le32(bits_r, 24);
  else
    param->sample_rate = sf[ret];
  return IAMF_OK;
}

static int _obu_cc_flac_decoder_config_parse(io_context_t *ioc,
                                             audio_codec_parameter_t *param) {
  io_context_t *r = ioc;
  uint32_t val = 0;
  int last, type, size, ret = IAMF_ERR_BAD_ARG;
  while (1) {
    val = ior_b32(r);

    /**
     * @brief METADATA_BLOCK_HEADER
     * b01: last metadata block flag
     * b07: block_type
     *      0x00: STREAMINFO
     * b24: length of metdata     *
     */
    last = val >> 31 & 0x01;
    type = val >> 24 & 0x7f;
    size = val & 0xffffff;

    if (!type) {
      /**
       * @brief METADATA_BLOCK_STREAMINFO
       * b16: min_block_size
       * b16: max_block_size
       * b24: min_frame_size
       * b24: max_frame_size
       * b20: sample_rate
       * b03: number_of_channels - 1
       * b05: bits_per_sample - 1
       * b36: total_samples
       * b128: MD5 sum       *
       */
      ior_skip(r, 10);
      val = ior_b32(r);
      param->sample_rate = val >> 12 & 0xfffff;
      param->bits_per_sample = (val >> 4 & 0x1f) + 1;
      ret = IAMF_OK;
      break;
    } else
      ior_skip(r, size);

    if (last) break;
  }
  return ret;
}

static int _obu_cc_pcm_decoder_config_parse(io_context_t *ioc,
                                            audio_codec_parameter_t *param) {
  io_context_t *r = ioc;

  /**
   * @brief LPCM Specific
   * b08: sample_format_flags: 0x01 - big endian, 0x00 - little endian
   * b08: sample_size
   * b32: sample_rate
   */
  param->big_endian = !ior_8(r);
  param->bits_per_sample = ior_8(r);
  param->sample_rate = ior_b32(r);
  return IAMF_OK;
}

int _obu_cc_decoder_config_parse(iamf_codec_type_t type, io_context_t *ioc,
                                 audio_codec_parameter_t *param) {
  int ret = IAMF_OK;
  switch (type) {
    case ck_iamf_codec_type_opus:
      ret = _obu_cc_opus_decoder_config_parse(ioc, param);
      break;
    case ck_iamf_codec_type_aac:
      ret = _obu_cc_aac_decoder_config_parse(ioc, param);
      break;
    case ck_iamf_codec_type_flac:
      ret = _obu_cc_flac_decoder_config_parse(ioc, param);
      break;
    case ck_iamf_codec_type_lpcm:
      ret = _obu_cc_pcm_decoder_config_parse(ioc, param);
      break;
    default:
      ret = IAMF_ERR_BAD_ARG;
      break;
  }

  if (ret == IAMF_OK && !param->sample_format)
    param->sample_format = def_audio_sample_format_default;

  return ret;
}
