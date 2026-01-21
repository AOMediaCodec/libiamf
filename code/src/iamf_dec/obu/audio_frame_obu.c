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
 * @file audio_frame_obu.c
 * @brief Audio frame OBU implementation.
 * @version 2.0.0
 * @date Created 12/11/2025
 **/

#include "audio_frame_obu.h"

#include "clog.h"
#include "iamf_private_definitions.h"
#include "iamf_string.h"

#ifdef SUPPORT_VERIFIER
#include "vlogging_tool_sr.h"
#endif

#undef def_log_tag
#define def_log_tag "OBU_AF"

iamf_audio_frame_obu_t *iamf_audio_frame_obu_new(io_context_t *ior,
                                                 iamf_obu_header_t *header) {
  iamf_audio_frame_obu_t *obu = 0;
  io_context_t *r = ior;
  uint32_t size = 0;

  obu = def_mallocz(iamf_audio_frame_obu_t, 1);
  if (!obu) {
    def_err_msg_enomem("Audio Frame OBU", def_iamf_ustr);
    return 0;
  }

  obu->obu.obu_type = header->obu_type;

  if (obu->obu.obu_type == ck_iamf_obu_audio_frame) {
    obu->audio_substream_id = ior_leb128_u32(r);
  } else {
    obu->audio_substream_id = obu->obu.obu_type - ck_iamf_obu_audio_frame_id0;
  }
  obu->num_samples_to_trim_at_end = header->num_samples_to_trim_at_end;
  obu->num_samples_to_trim_at_start = header->num_samples_to_trim_at_start;
  size = ioc_remain(r);

  obu->audio_frame = buffer_wrap_new(size);
  if (!obu->audio_frame) {
    iamf_audio_frame_obu_free(obu);
    def_err_msg_enomem("audio frame", "Audio Frame OBU");
    return 0;
  }

  ior_read(r, obu->audio_frame->data, size);

  debug(
      "Audio Frame OBU: audio_substream_id=%u, num_samples_to_trim_at_start=%u,"
      "num_samples_to_trim_at_end=%u, size=%u",
      obu->audio_substream_id, obu->num_samples_to_trim_at_start,
      obu->num_samples_to_trim_at_end, size);

#if SUPPORT_VERIFIER
  vlog_obu(ck_iamf_obu_audio_frame, obu, obu->num_samples_to_trim_at_start,
           obu->num_samples_to_trim_at_end);
#endif

  // iamf_audio_frame_obu_display(obu);

  return obu;
}

void iamf_audio_frame_obu_free(iamf_audio_frame_obu_t *obu) {
  if (!obu) return;
  if (obu->audio_frame) buffer_wrap_free(obu->audio_frame);
  free(obu);
}

void iamf_audio_frame_obu_display(iamf_audio_frame_obu_t *obu) {
  if (!obu) {
    debug("Audio Frame OBU: NULL pointer");
    return;
  }

  debug("Audio Frame OBU:");
  debug("  OBU Type: %d (%s)", obu->obu.obu_type,
        iamf_obu_type_string(obu->obu.obu_type));
  debug("  Audio Substream ID: %u", obu->audio_substream_id);
  debug("  Num Samples to Trim at Start: %u",
        obu->num_samples_to_trim_at_start);
  debug("  Num Samples to Trim at End: %u", obu->num_samples_to_trim_at_end);

  if (obu->audio_frame) {
    debug("  Audio Frame Size: %u bytes", obu->audio_frame->size);
  } else {
    debug("  Audio Frame: NULL");
  }
}
