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
 * @file iamf_obu.c
 * @brief OBU raw APIs.
 * @version 2.0.0
 * @date Created 12/11/2025
 **/

#include <inttypes.h>
#include <string.h>

#include "IAMF_defines.h"
#include "clog.h"
#include "iamf_obu_all.h"
#include "iamf_private_definitions.h"
#include "iamf_string.h"

#ifdef IA_DBG
#include "debug/iamf_obu_debug.h"
#endif

// obu_type(5) + obu_redundant_copy(1) + obu_trimming_status_flag(1) +
// obu_extension_flag(1) + obu_size(min:8)
#define def_iamf_obu_min_size 2

// 4. Profiles
// The maximum size of an OBU (an OBU Header followed by the OBU payload) SHALL
// be limited to 2MB(2^21).
#define def_iamf_obu_max_size 2097152

#undef def_log_tag
#define def_log_tag "IAMF_OBU"

// ==============================================================================================
static int _iamf_obu_header_init(iamf_obu_header_t *h) {
  memset(h, 0, sizeof(iamf_obu_header_t));
  h->obu_type = ck_iamf_obu_none;
  return IAMF_OK;
}

static int _iamf_obu_type_is_audio_frame(iamf_obu_type_t type) {
  return type >= ck_iamf_obu_audio_frame &&
         type <= ck_iamf_obu_audio_frame_id17;
}

static int _iamf_obu_raw_parse_header(io_context_t *ior, iamf_obu_header_t *h) {
  io_context_t *r = ior;
  uint32_t obu_extension_flag = 0;
  uint32_t val = ior_8(r);
  uint32_t obu_size = 0;

  h->obu_type = (val >> 3) & 0x1f;
  h->obu_redundant_copy = (val >> 2) & 0x01;
  h->obu_trimming_status_flag = (val >> 1) & 0x01;
  obu_extension_flag = val & 0x01;

  trace(
      "obu header: obu_type(%s<%d>), obu_redundant_copy(%u), "
      "%s(%u), obu_extension_flag(%u)",
      iamf_obu_type_string(h->obu_type), h->obu_type, h->obu_redundant_copy,
      iamf_obu_header_flag2_string(h->obu_type), h->obu_trimming_status_flag,
      obu_extension_flag);

  obu_size = ior_leb128_u32(r);
  trace("obu header: obu_size(%u)", obu_size);

  if (_iamf_obu_type_is_audio_frame(h->obu_type) &&
      h->obu_trimming_status_flag) {
    h->num_samples_to_trim_at_end = ior_leb128_u32(r);
    h->num_samples_to_trim_at_start = ior_leb128_u32(r);
    trace(
        "obu header: num_samples_to_trim_at_end(%u), "
        "num_samples_to_trim_at_start(%u)",
        h->num_samples_to_trim_at_end, h->num_samples_to_trim_at_start);
  }

  if (obu_extension_flag) {
    uint32_t extension_header_size = ior_leb128_u32(r);
    ior_skip(r, extension_header_size);  // extension_header_bytes
    trace("obu header: extension_header_size(%u)", extension_header_size);
  }

  return IAMF_OK;
}

static int _iamf_obu_raw_parse_body(io_context_t *ior,
                                    iamf_obu_header_t *header,
                                    iamf_obu_extra_parameters_t *extra,
                                    iamf_obu_t **obu) {
  int ret = IAMF_OK;
  switch (header->obu_type) {
    case ck_iamf_obu_sequence_header:
      *obu = (iamf_obu_t *)iamf_sequence_header_obu_new(ior);
      break;
    case ck_iamf_obu_codec_config:
      *obu = (iamf_obu_t *)iamf_codec_config_obu_new(ior);
      break;
    case ck_iamf_obu_audio_element:
      *obu = (iamf_obu_t *)iamf_audio_element_obu_new(ior);
      break;
    case ck_iamf_obu_mix_presentation:
      *obu = (iamf_obu_t *)iamf_mix_presentation_obu_new(ior, header);
      break;
    case ck_iamf_obu_parameter_block:
      if (extra)
        *obu = (iamf_obu_t *)iamf_parameter_block_obu_new(
            ior, &extra->pbo_interfaces);
      break;
    case ck_iamf_obu_temporal_delimiter:
      *obu = (iamf_obu_t *)iamf_temporal_delimiter_obu_new(ior, header);
      break;
    case ck_iamf_obu_metadata:
      *obu = (iamf_obu_t *)iamf_metadata_obu_new(ior);
      break;
    default:
      if (header->obu_type >= ck_iamf_obu_audio_frame &&
          header->obu_type <= ck_iamf_obu_audio_frame_id17) {
        *obu = (iamf_obu_t *)iamf_audio_frame_obu_new(ior, header);
      } else if (header->obu_type != ck_iamf_obu_temporal_delimiter) {
        warning("Reserved OBU type %u", header->obu_type);
      }
      break;
  }

  if (*obu && header->obu_redundant_copy)
    (*obu)->obu_flags = ck_iamf_obu_flag_redundant;
  return ret;
}

static int iamf_obu_raw_init(iamf_obu_raw_t *raw, uint8_t *data,
                             uint32_t size) {
  ioc_init(&raw->ioctx, data, size);
  return 0;
}

// ==============================================================================================

uint32_t iamf_obu_raw_split(uint8_t *data, uint32_t size, iamf_obu_raw_t *raw) {
  io_context_t ioc, *r;
  uint32_t val;

  if (size < def_iamf_obu_min_size) return 0;

  r = &ioc;
  ioc_init(r, data, size);
  ior_8(r);
  val = ior_leb128_u32(r);

  if (val > ioc_remain(r)) return 0;

  ior_skip(r, val);
  val = ioc_tell(r);
  iamf_obu_raw_init(raw, data, val);

#ifdef IA_DBG
  obu_dump(data, val, (data[0] >> 3) & 0x1f);
#endif

  return val;
}

int iamf_obu_raw_check(iamf_obu_raw_t *raw) {
  return !(!raw->ioctx.buffer || raw->ioctx.size < def_iamf_obu_min_size ||
           raw->ioctx.size > def_iamf_obu_max_size);
}

int iamf_obu_raw_is_reserved_obu(iamf_obu_raw_t *raw, iamf_profile_t profile) {
  iamf_obu_type_t type = ck_iamf_obu_none;

  if (!iamf_obu_raw_check(raw)) return 0;

  type = (raw->ioctx.buffer[0] >> 3) & 0x1f;
  return (type >= ck_iamf_obu_reserved_start &&
          type <= ck_iamf_obu_reserved_end) ||
         (profile > ck_iamf_profile_base_enhanced
              ? 0
              : type == ck_iamf_obu_metadata);
}

iamf_obu_t *iamf_obu_new(iamf_obu_raw_t *raw,
                         iamf_obu_extra_parameters_t *param) {
  iamf_obu_t *obu = 0;
  io_context_t *r;
  iamf_obu_header_t header;

  r = &raw->ioctx;

  _iamf_obu_header_init(&header);
  trace("raw obu size(%u)", ioc_remain(r));
  _iamf_obu_raw_parse_header(r, &header);
  trace("obu header size(%u), obu body size(%u)", ioc_tell(r), ioc_remain(r));
  _iamf_obu_raw_parse_body(r, &header, param, &obu);

  return obu;
}

void iamf_obu_free(iamf_obu_t *obu) {
  if (!obu) return;

  trace("freeing obu type %s<%d>", iamf_obu_type_string(obu->obu_type),
        obu->obu_type);
  switch (obu->obu_type) {
    case ck_iamf_obu_sequence_header:
      iamf_sequence_header_obu_free((iamf_sequence_header_obu_t *)obu);
      break;
    case ck_iamf_obu_codec_config:
      iamf_codec_config_obu_free((iamf_codec_config_obu_t *)obu);
      break;
    case ck_iamf_obu_audio_element:
      iamf_audio_element_obu_free((iamf_audio_element_obu_t *)obu);
      break;
    case ck_iamf_obu_mix_presentation:
      iamf_mix_presentation_obu_free((iamf_mix_presentation_obu_t *)obu);
      break;
    case ck_iamf_obu_parameter_block:
      iamf_parameter_block_obu_free((iamf_parameter_block_obu_t *)obu);
      break;
    case ck_iamf_obu_temporal_delimiter:
      iamf_temporal_delimiter_obu_free((iamf_temporal_delimiter_obu_t *)obu);
      break;
    case ck_iamf_obu_metadata:
      iamf_metadata_obu_free((iamf_metadata_obu_t *)obu);
      break;
    default:
      if (_iamf_obu_type_is_audio_frame(obu->obu_type))
        iamf_audio_frame_obu_free((iamf_audio_frame_obu_t *)obu);
      break;
  }
}

int iamf_obu_is_descriptor(iamf_obu_t *obu) {
  return obu->obu_type == ck_iamf_obu_sequence_header ||
         obu->obu_type == ck_iamf_obu_codec_config ||
         obu->obu_type == ck_iamf_obu_audio_element ||
         obu->obu_type == ck_iamf_obu_metadata ||
         obu->obu_type == ck_iamf_obu_mix_presentation;
}

int iamf_obu_is_redundant_copy(iamf_obu_t *obu) {
  return obu->obu_flags & ck_iamf_obu_flag_redundant;
}

const iamf_profile_info_t *iamf_profile_info_get(iamf_profile_t profile) {
  static const iamf_profile_info_t _profile_limit[def_iamf_profile_count] = {
      {1, 16, 16, 1, 1},   // simple profile
      {2, 18, 16, 1, 1},   // base profile
      {28, 28, 25, 1, 1},  // base-enhanced profile
      {18, 18, 16, 2, 2},  // base-advanced profile
      {18, 18, 16, 2, 2},  // advanced profile 1
      {28, 28, 25, 2, 2},  // advanced profile 2
  };

  if (profile < def_iamf_profile_count) return &_profile_limit[profile];
  return 0;
}

iamf_tag_t *iamf_tag_new(io_context_t *ior) {
  io_context_t *r = ior;
  iamf_tag_t *tag = def_mallocz(iamf_tag_t, 1);

  if (tag) {
    ior_string(r, tag->name, sizeof(string128_t));
    ior_string(r, tag->value, sizeof(string128_t));
  }

  return tag;
}
