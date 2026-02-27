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
 * @file ia_sequence_header_obu.h
 * @brief IAMF sequence header OBU APIs.
 * @version 2.0.0
 * @date Created 12/11/2025
 **/

#ifndef __IA_SEQUENCE_HEADER_OBU_H__
#define __IA_SEQUENCE_HEADER_OBU_H__

#include "iamf_obu.h"

/**
 * IAMF Sequence Header OBU.
 * */

#define def_iamf_sequence_header_obu_ptr(a) ((iamf_sequence_header_obu_t *)a)

typedef struct IamfSequenceHeaderObu {
  iamf_obu_t obu;

  uint32_t iamf_code;
  uint32_t primary_profile;
  uint32_t additional_profile;
} iamf_sequence_header_obu_t;

iamf_sequence_header_obu_t *iamf_sequence_header_obu_new(io_context_t *ioc);
void iamf_sequence_header_obu_free(iamf_sequence_header_obu_t *obu);
#endif  // __IA_SEQUENCE_HEADER_OBU_H__
