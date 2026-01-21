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
 * @file ia_sequence_header_obu.c
 * @brief IAMF sequence header OBU implementation.
 * @version 2.0.0
 * @date Created 12/11/2025
 **/

#include "ia_sequence_header_obu.h"

#include "iamf_private_definitions.h"
#include "iamf_string.h"
#include "iamf_types.h"

#ifdef SUPPORT_VERIFIER
#include "vlogging_tool_sr.h"
#endif

#undef def_log_tag
#define def_log_tag "OBU_SH"

static int _obu_sh_check(iamf_sequence_header_obu_t *obu);

iamf_sequence_header_obu_t *iamf_sequence_header_obu_new(io_context_t *ioc) {
  iamf_sequence_header_obu_t *obu = 0;
  io_context_t *r = ioc;

  obu = def_mallocz(iamf_sequence_header_obu_t, 1);
  if (!obu) {
    def_err_msg_enomem("IA Sequence Header OBU", def_iamf_ustr);
    return 0;
  }

  obu->obu.obu_type = ck_iamf_obu_sequence_header;

  ior_read(r, (uint8_t *)&obu->iamf_code, 4);
  obu->primary_profile = ior_8(r);
  obu->additional_profile = ior_8(r);

  info("Ia code(%.4s), primary profile(%u), additional profile(%u).",
       (char *)&obu->iamf_code, obu->primary_profile, obu->additional_profile);

#if SUPPORT_VERIFIER
  vlog_obu(ck_iamf_obu_sequence_header, obu, 0, 0);
#endif

  iamf_sequence_header_obu_display(obu);

  if (_obu_sh_check(obu) != def_pass) {
    iamf_sequence_header_obu_free(obu);
    obu = 0;
  }
  return obu;
}

void iamf_sequence_header_obu_free(iamf_sequence_header_obu_t *obu) {
  free(obu);
}

void iamf_sequence_header_obu_display(iamf_sequence_header_obu_t *obu) {
  if (!obu) {
    warning("IAMF Sequence Header OBU is NULL, cannot display.");
    return;
  }

  debug("Displaying IAMF Sequence Header OBU:");
  debug("  iamf_code: 0x%08x ('%.4s')", obu->iamf_code,
        (char *)&obu->iamf_code);
  debug("  primary_profile: %u (%s)", obu->primary_profile,
        iamf_profile_type_string(obu->primary_profile));
  debug("  additional_profile: %u (%s)", obu->additional_profile,
        iamf_profile_type_string(obu->primary_profile));

  debug("Finished displaying IAMF Sequence Header OBU.");
}

static int _obu_sh_valid_profile(uint8_t primary, uint8_t addional) {
  return primary < def_iamf_profile_count && primary <= addional;
}

int _obu_sh_check(iamf_sequence_header_obu_t *obu) {
  union {
    uint32_t _id;
    uint8_t _4cc[4];
  } code = {._4cc = {'i', 'a', 'm', 'f'}};

  if (obu->iamf_code != code._id) {
    error("Invalid iamf code(%.4s).", (char *)&obu->iamf_code);
    return def_error;
  }

  if (!_obu_sh_valid_profile(obu->primary_profile, obu->additional_profile)) {
    error("Invalid primary profile(%u) or additional profile(%u).",
          obu->primary_profile, obu->additional_profile);
    return def_error;
  }

  return def_pass;
}
