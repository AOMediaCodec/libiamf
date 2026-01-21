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
 * @file metadata_obu.h
 * @brief Metadata OBU APIs.
 * @version 2.0.0
 * @date Created 12/11/2025
 **/

#ifndef __METADATA_OBU_H__
#define __METADATA_OBU_H__

#include "iamf_obu.h"

/**
 * Metadata OBU.
 * */

typedef enum EIamfMetadataType {
  ck_iamf_metadata_type_reserved_0 = 0,
  ck_iamf_metadata_type_itut_t35 = 1,
  ck_iamf_metadata_type_iamf_tags = 2,
  ck_iamf_metadata_type_reserved_start = 3,
  ck_iamf_metadata_type_reserved_end = 63,
  ck_iamf_metadata_type_unregistered_user_private_start = 64,
  ck_iamf_metadata_type_unregistered_user_private_end = 127,
  ck_iamf_metadata_type_reserved_future_start = 128
} iamf_metadata_type_t;

typedef struct IamfMetadataObu {
  iamf_obu_t obu;
  iamf_metadata_type_t metadata_type;
} iamf_metadata_obu_t;

typedef struct MetadataITUTT35 {
  iamf_metadata_obu_t metadata;
  uint8_t itu_t_t35_country_code;
  uint8_t
      itu_t_t35_country_code_extension_byte;  // Present if
                                              // itu_t_t35_country_code == 0xFF
  uint8_t *itu_t_t35_payload_bytes;
  uint32_t itu_t_t35_payload_size;
} metadata_itut_t35_t;

typedef struct MetadataIamfTags {
  iamf_metadata_obu_t metadata;
  array_t *tags;  // array of iamf_tag_t *'s.
} metadata_iamf_tags;

iamf_metadata_obu_t *iamf_metadata_obu_new(io_context_t *ior);
void iamf_metadata_obu_free(iamf_metadata_obu_t *obu);
void iamf_metadata_obu_display(iamf_metadata_obu_t *obu);

#endif  // __METADATA_OBU_H__
