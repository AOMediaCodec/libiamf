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
 * @file iamf_obu_parser.c
 * @brief IAMF OBU Parser module implementation.
 * @version 2.0.0
 * @date Created 12/11/2025
 **/

#include "iamf_obu_parser.h"

#include <stdlib.h>
#include <string.h>

#include "clog.h"
#include "iamf_private_definitions.h"
#include "obu/iamf_obu_all.h"

#undef def_log_tag
#define def_log_tag "IAMF_PRS"

typedef struct IamfObuParser {
  iamf_profile_t profile;
  fun_obu_process_t obu_process;
  iamf_parser_state_t state;
  void *user_data;
  iamf_obu_extra_parameters_t extra_params;
} iamf_obu_parser_t;

// --- Forward declarations ---
static iamf_parser_state_t iamf_obu_parser_process_single_obu(
    iamf_obu_parser_t *parser, iamf_obu_raw_t *raw_obu);

// --- API Implementations ---

iamf_obu_parser_t *iamf_obu_parser_create(
    fun_obu_process_t obu_process, void *user_data,
    const iamf_obu_extra_parameters_t *extra_params) {
  iamf_obu_parser_t *parser = def_mallocz(iamf_obu_parser_t, 1);
  if (!parser) {
    error("iamf_obu_parser_create: Failed to allocate memory for parser");
    return 0;
  }

  parser->obu_process = obu_process;
  parser->state = ck_iamf_parser_state_idle;
  parser->user_data = user_data;

  // Initialize and copy extra parameters if provided
  if (extra_params)
    memcpy(&parser->extra_params, extra_params,
           sizeof(iamf_obu_extra_parameters_t));

  parser->profile = def_iamf_profile_default;
  return parser;
}

void iamf_obu_parser_destroy(iamf_obu_parser_t *parser) { def_free(parser); }

uint32_t iamf_obu_parser_parse(iamf_obu_parser_t *parser, const uint8_t *data,
                               uint32_t size) {
  uint8_t *current_ptr = (uint8_t *)data;
  uint32_t remaining_bytes = size;
  iamf_parser_state_t state = ck_iamf_parser_state_run;

  if (!parser || !data || !size) {
    error(
        "iamf_obu_parser_parse: Invalid arguments (parser=%p, data=%p, "
        "size=%u)",
        parser, data, size);
    return 0;
  }

  while (remaining_bytes > 0) {
    iamf_obu_raw_t raw_obu;
    uint32_t obu_size =
        iamf_obu_raw_split(current_ptr, remaining_bytes, &raw_obu);
    if (!obu_size) break;

    state = iamf_obu_parser_process_single_obu(parser, &raw_obu);
    if (state == ck_iamf_parser_state_switch) {
      debug("parsed OBU to the next step.");
      break;
    }

    current_ptr += obu_size;
    remaining_bytes -= obu_size;

    if (state == ck_iamf_parser_state_stop) break;
  }

  if (state != parser->state) parser->state = state;

  return current_ptr - data;
}

iamf_parser_state_t iamf_obu_parser_get_state(iamf_obu_parser_t *parser) {
  return parser ? parser->state : ck_iamf_parser_state_idle;
}

// Internal function: process a single OBU
static iamf_parser_state_t iamf_obu_parser_process_single_obu(
    iamf_obu_parser_t *parser, iamf_obu_raw_t *raw_obu) {
  iamf_obu_t *obu = 0;

  if (iamf_obu_raw_is_reserved_obu(raw_obu, parser->profile))
    return ck_iamf_parser_state_run;

  obu = iamf_obu_new(raw_obu, &parser->extra_params);

  if (!obu) {
    debug("iamf_obu_parser_process_single_obu: iamf_obu_new failed");
    return ck_iamf_parser_state_run;
  } else if (obu->obu_type == ck_iamf_obu_sequence_header) {
    iamf_sequence_header_obu_t *sho = def_iamf_sequence_header_obu_ptr(obu);
    if (parser->profile > sho->additional_profile ||
        (parser->profile < sho->additional_profile &&
         sho->additional_profile <= def_iamf_profile_default)) {
      info("iamf_obu_parser_process_single_obu: Profile changed from %u to %u",
           parser->profile, sho->additional_profile);
      parser->profile = sho->additional_profile;
    }
  }

  return parser->obu_process(obu, parser->user_data);
}
