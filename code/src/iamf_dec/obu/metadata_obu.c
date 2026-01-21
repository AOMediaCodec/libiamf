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
 * @file metadata_obu.c
 * @brief Metadata OBU implementation.
 * @version 2.0.0
 * @date Created 12/11/2025
 **/

#include "metadata_obu.h"

#include <stdbool.h>
#include <string.h>

#include "iamf_private_definitions.h"
#include "iamf_string.h"

#ifdef SUPPORT_VERIFIER
#include "vlogging_tool_sr.h"
#endif

#undef def_log_tag
#define def_log_tag "OBU_MD"
#define def_metadata_str "Metadata OBU"

static void _metadata_itut_t35_delete(metadata_itut_t35_t *metadata);

static metadata_itut_t35_t *_metadata_itut_t35_new(io_context_t *ior) {
  io_context_t *r = ior;
  metadata_itut_t35_t *metadata = 0;

  metadata = (metadata_itut_t35_t *)def_malloc(metadata_itut_t35_t, 1);
  if (!metadata) {
    def_err_msg_enomem("Metadata ITU-T T.35", def_metadata_str);
    return 0;
  }

  metadata->metadata.obu.obu_type = ck_iamf_obu_metadata;
  metadata->metadata.metadata_type = ck_iamf_metadata_type_itut_t35;

  // Read country code
  metadata->itu_t_t35_country_code = ior_8(r);
  info("ITU-T T.35 Country Code: 0x%02X", metadata->itu_t_t35_country_code);

  // If country code is 0xFF, read country code extension byte
  if (metadata->itu_t_t35_country_code == 0xFF) {
    metadata->itu_t_t35_country_code_extension_byte = ior_8(r);
    info("ITU-T T.35 Country Code Extension Byte: 0x%02X",
         metadata->itu_t_t35_country_code_extension_byte);
  } else {
    metadata->itu_t_t35_country_code_extension_byte = 0;
  }

  // Calculate and read payload data
  metadata->itu_t_t35_payload_size = ioc_remain(r);

  if (metadata->itu_t_t35_payload_size > 0) {
    metadata->itu_t_t35_payload_bytes =
        (uint8_t *)def_malloc(uint8_t, metadata->itu_t_t35_payload_size);
    if (!metadata->itu_t_t35_payload_bytes) {
      def_err_msg_enomem("ITU-T T.35 payload bytes", def_metadata_str);
      _metadata_itut_t35_delete(metadata);
      return 0;
    }

    // Read payload data
    ior_read(r, metadata->itu_t_t35_payload_bytes,
             metadata->itu_t_t35_payload_size);
    info("ITU-T T.35 Payload Size: %u bytes", metadata->itu_t_t35_payload_size);
  } else {
    metadata->itu_t_t35_payload_bytes = 0;
    info("ITU-T T.35 Payload Size: 0 bytes");
  }

  return metadata;
}

void _metadata_itut_t35_delete(metadata_itut_t35_t *metadata) {
  if (!metadata) return;

  def_free(metadata->itu_t_t35_payload_bytes);
  metadata->itu_t_t35_payload_bytes = 0;
  metadata->itu_t_t35_payload_size = 0;
  def_free(metadata);
}

static void _metadata_iamf_tags_delete(metadata_iamf_tags *metadata);

static metadata_iamf_tags *_metadata_iamf_tags_new(io_context_t *ior) {
  io_context_t *r = ior;
  metadata_iamf_tags *metadata = 0;
  int n = 0;

  metadata = (metadata_iamf_tags *)def_malloc(metadata_iamf_tags, 1);
  if (!metadata) {
    def_err_msg_enomem("Metadata IAMF Tags", def_metadata_str);
    return 0;
  }

  metadata->metadata.obu.obu_type = ck_iamf_obu_metadata;
  metadata->metadata.metadata_type = ck_iamf_metadata_type_iamf_tags;

  n = ior_8(r);
  info("IAMF Tags count: %d", n);
  if (n > 0) {
    metadata->tags = array_new(n);
    if (!metadata->tags) {
      def_err_msg_enomem("IAMF Tags array", def_metadata_str);
      _metadata_iamf_tags_delete(metadata);
      return 0;
    }

    for (int i = 0; i < n; i++) {
      iamf_tag_t *tag = iamf_tag_new(r);
      if (!tag) {
        def_err_msg_enomem("IAMF Tag", def_metadata_str);
        _metadata_iamf_tags_delete(metadata);
        return 0;
      }
      def_value_wrap_ptr(array_at(metadata->tags, i)) = tag;
    }
  }

  return metadata;
}

void _metadata_iamf_tags_delete(metadata_iamf_tags *metadata) {
  if (!metadata) return;

  array_free(metadata->tags, free);
  def_free(metadata);
}

iamf_metadata_obu_t *iamf_metadata_obu_new(io_context_t *ior) {
  io_context_t *r = ior;
  iamf_metadata_obu_t *obu = 0;
  uint32_t metadata_type;

  if (!ior) {
    error("Invalid I/O context");
    return 0;
  }

  metadata_type = ior_leb128_u32(r);
  info("New Metadata OBU, type: %u", metadata_type);

  switch (metadata_type) {
    case ck_iamf_metadata_type_itut_t35: {
      metadata_itut_t35_t *itu_t35_metadata = _metadata_itut_t35_new(r);
      obu = (iamf_metadata_obu_t *)itu_t35_metadata;
      break;
    }

    case ck_iamf_metadata_type_iamf_tags: {
      metadata_iamf_tags *tags_metadata = _metadata_iamf_tags_new(r);
      obu = (iamf_metadata_obu_t *)tags_metadata;
      break;
    }

    default:
      if (metadata_type >= ck_iamf_metadata_type_reserved_future_start) {
        warning("Reserved or future metadata type: %u. Skipping payload.",
                metadata_type);
      } else if ((metadata_type >= ck_iamf_metadata_type_reserved_start &&
                  metadata_type <= ck_iamf_metadata_type_reserved_end) ||
                 (metadata_type >=
                      ck_iamf_metadata_type_unregistered_user_private_start &&
                  metadata_type <=
                      ck_iamf_metadata_type_unregistered_user_private_end)) {
        warning("Unsupported or reserved metadata type: %u. Skipping payload.",
                metadata_type);
      } else {
        error("Invalid metadata type: %u", metadata_type);
        iamf_metadata_obu_free(obu);
        return 0;
      }
      break;
  }

#if SUPPORT_VERIFIER
  vlog_obu(ck_iamf_obu_metadata, obu, 0, 0);
#endif

  iamf_metadata_obu_display(obu);

  return obu;
}

void iamf_metadata_obu_free(iamf_metadata_obu_t *obu) {
  if (!obu) return;

  debug("Free iamf_metadata_obu_t %p", obu);

  switch (obu->metadata_type) {
    case ck_iamf_metadata_type_itut_t35: {
      metadata_itut_t35_t *itu_t35_metadata = (metadata_itut_t35_t *)obu;
      _metadata_itut_t35_delete(itu_t35_metadata);
      break;
    }
    case ck_iamf_metadata_type_iamf_tags: {
      metadata_iamf_tags *tags_metadata = (metadata_iamf_tags *)obu;
      _metadata_iamf_tags_delete(tags_metadata);
      break;
    }
    default:
      // For other types, just free the basic structure
      def_free(obu);
      break;
  }
}

void iamf_metadata_obu_display(iamf_metadata_obu_t *obu) {
  if (!obu) {
    debug("IAMF Metadata OBU is 0, cannot display.");
    return;
  }

  debug("Displaying IAMF Metadata OBU:");
  debug("  obu_type: %u (%s)", obu->obu.obu_type,
        iamf_obu_type_string(obu->obu.obu_type));
  debug("  metadata_type: %u (%s)", obu->metadata_type,
        iamf_metadata_type_string(obu->metadata_type));

  switch (obu->metadata_type) {
    case ck_iamf_metadata_type_itut_t35: {
      metadata_itut_t35_t *itu_t35_metadata = (metadata_itut_t35_t *)obu;
      debug("  ITU-T T.35 Metadata:");
      debug("    country_code: 0x%02X",
            itu_t35_metadata->itu_t_t35_country_code);
      if (itu_t35_metadata->itu_t_t35_country_code == 0xFF) {
        debug("    country_code_extension_byte: 0x%02X",
              itu_t35_metadata->itu_t_t35_country_code_extension_byte);
      }
      debug("    payload_size: %u bytes",
            itu_t35_metadata->itu_t_t35_payload_size);
      if (itu_t35_metadata->itu_t_t35_payload_bytes &&
          itu_t35_metadata->itu_t_t35_payload_size > 0) {
        uint32_t display_size = itu_t35_metadata->itu_t_t35_payload_size;
        char buffer[256];
        int buffer_pos = 0;

        for (uint32_t i = 0; i < display_size; i++) {
          if (i % 8 == 0) {
            buffer_pos +=
                snprintf(buffer + buffer_pos, sizeof(buffer) - buffer_pos,
                         "      %04x: ", i);
          }
          buffer_pos +=
              snprintf(buffer + buffer_pos, sizeof(buffer) - buffer_pos,
                       "%02x ", itu_t35_metadata->itu_t_t35_payload_bytes[i]);
          if (i % 8 == 7 || i == display_size - 1) {
            // Add newline and output the complete line
            buffer_pos += snprintf(buffer + buffer_pos,
                                   sizeof(buffer) - buffer_pos, "\n");
            debug("%s", buffer);
            // Reset buffer for next line
            buffer[0] = '\0';
            buffer_pos = 0;
          }
        }

        for (uint32_t i = 0; i < display_size; i++) {
          if (i % 8 == 0) {
            buffer_pos +=
                snprintf(buffer + buffer_pos, sizeof(buffer) - buffer_pos,
                         "      %04x: ", i);
          }
          buffer_pos +=
              snprintf(buffer + buffer_pos, sizeof(buffer) - buffer_pos, "%c ",
                       itu_t35_metadata->itu_t_t35_payload_bytes[i]);
          if (i % 8 == 7 || i == display_size - 1) {
            // Add newline and output the complete line
            buffer_pos += snprintf(buffer + buffer_pos,
                                   sizeof(buffer) - buffer_pos, "\n");
            debug("%s", buffer);
            // Reset buffer for next line
            buffer[0] = '\0';
            buffer_pos = 0;
          }
        }
      }
      break;
    }

    case ck_iamf_metadata_type_iamf_tags: {
      metadata_iamf_tags *tags_metadata = (metadata_iamf_tags *)obu;
      debug("  IAMF Tags Metadata:");
      if (tags_metadata->tags) {
        uint32_t num_tags = array_size(tags_metadata->tags);
        debug("    Number of tags: %u", num_tags);
        for (uint32_t i = 0; i < num_tags; ++i) {
          value_wrap_t *v = array_at(tags_metadata->tags, i);
          iamf_tag_t *tag = (iamf_tag_t *)v->ptr;
          if (tag) {
            debug("    Tag [%u]:", i);
            debug("      name: %s", tag->name);
            debug("      value: %s", tag->value);
          }
        }
      } else {
        debug("    No tags.");
      }
      break;
    }

    default:
      debug("  Unknown or unsupported metadata type: %u", obu->metadata_type);
      break;
  }

  debug("Finished displaying IAMF Metadata OBU.");
}
