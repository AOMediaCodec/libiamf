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
 * @file temporal_delimiter_obu.h
 * @brief Temporal delimiter OBU APIs.
 * @version 2.0.0
 * @date Created 12/11/2025
 **/

#ifndef __TEMPORAL_DELIMITER_OBU_H__
#define __TEMPORAL_DELIMITER_OBU_H__

#include "iamf_obu.h"

#define def_temporal_delimiter_obu_ptr(a) \
  def_ptr(iamf_temporal_delimiter_obu, a)

/**
 * Temporal Delimiter OBU.
 * */

typedef struct IamfTemporalDelimiterObu {
  iamf_obu_t obu;
  uint8_t key_frame;
} iamf_temporal_delimiter_obu_t;

iamf_temporal_delimiter_obu_t *iamf_temporal_delimiter_obu_new(
    io_context_t *ior, iamf_obu_header_t *header);
void iamf_temporal_delimiter_obu_free(iamf_temporal_delimiter_obu_t *obu);
void iamf_temporal_delimiter_obu_display(iamf_temporal_delimiter_obu_t *obu);

#endif  // __TEMPORAL_DELIMITER_OBU_H__
