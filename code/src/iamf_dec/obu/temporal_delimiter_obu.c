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
 * @file temporal_delimiter_obu.c
 * @brief Temporal delimiter OBU implementation.
 * @version 2.0.0
 * @date Created 12/11/2025
 **/

#include "temporal_delimiter_obu.h"

#include "iamf_private_definitions.h"
#include "iamf_string.h"

#undef def_log_tag
#define def_log_tag "OBU_TD"

iamf_temporal_delimiter_obu_t *iamf_temporal_delimiter_obu_new(
    io_context_t *ior, iamf_obu_header_t *header) {
  iamf_temporal_delimiter_obu_t *obu = 0;

  obu = def_mallocz(iamf_temporal_delimiter_obu_t, 1);
  if (!obu) {
    def_err_msg_enomem("Temporal Delimiter OBU", def_iamf_ustr);
    return 0;
  }

  obu->obu.obu_type = ck_iamf_obu_temporal_delimiter;
  obu->key_frame = !header->is_not_key_frame;

  // iamf_temporal_delimiter_obu_display(obu);

  return obu;
}

void iamf_temporal_delimiter_obu_free(iamf_temporal_delimiter_obu_t *obu) {
  if (!obu) return;

  debug("Free iamf_temporal_delimiter_obu_t %p", obu);

  def_free(obu);
}

void iamf_temporal_delimiter_obu_display(iamf_temporal_delimiter_obu_t *obu) {
  if (!obu) {
    debug("IAMF Temporal Delimiter OBU is NULL, cannot display.");
    return;
  }

  debug("Displaying IAMF Temporal Delimiter OBU:");
  debug("  obu_type: %u (%s)", obu->obu.obu_type,
        iamf_obu_type_string(obu->obu.obu_type));
  debug("  key_frame: %s", obu->key_frame ? "Yes" : "No");

  debug("Finished displaying IAMF Temporal Delimiter OBU.");
}
