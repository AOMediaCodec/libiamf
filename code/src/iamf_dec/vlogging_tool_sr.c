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
 * @file vlogging_tool_sr.h
 * @brief verification log generator.
 * @version 0.1
 * @date Created 03/29/2023
 **/

#if defined(_WIN32)
#include <fcntl.h>
#include <io.h>
#else
#include <unistd.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "iamf_utils.h"
#include "obu/iamf_obu_all.h"
#include "vlogging_tool_sr.h"

#define LOG_BUFFER_SIZE 100000

typedef struct vlogdata {
  int log_type;
  uint64_t key;
  struct vlogdata* prev;
  struct vlogdata* next;
  int is_longtext;
  char* ltext;
  char text[256];
} VLOG_DATA;

typedef struct vlog_file {
  FILE* f;
  char file_name[FILENAME_MAX];
  int is_open;
  VLOG_DATA* head[MAX_LOG_TYPE];
} VLOG_FILE;

static VLOG_FILE log_file = {
    0,
};

static uint32_t get_4cc_codec_id(char a, char b, char c, char d) {
  return ((a) | (b << 8) | (c << 16) | (d << 24));
}

static uint32_t htonl2(uint32_t x) {
  return (x >> 24) | ((x >> 8) & 0x0000FF00) | ((x << 8) & 0x00FF0000) |
         (x << 24);
}

static uint64_t htonll2(uint64_t x) {
  return ((((uint64_t)htonl2((uint32_t)x & 0xFFFFFFFF)) << 32) +
          htonl2((uint32_t)(x >> 32)));
}

int vlog_file_open(const char* log_file_name) {
  int i;
  unsigned char bom[] = {0xEF, 0xBB, 0xBF};
  if (log_file.is_open && strcmp(log_file_name, log_file.file_name) == 0) {
    return 0;
  }

#if defined(_WIN32)
  if (_access(log_file_name, 0) != -1) {
#else
  if (access(log_file_name, 0) != -1) {
#endif
    // already exist
    if (remove(log_file_name) == -1) return -1;
  }

  {
    log_file.f = fopen(log_file_name, "a");
    fwrite(bom, 1, 3, log_file.f);
    strcpy(log_file.file_name, log_file_name);
    log_file.is_open = 1;

    for (i = 0; i < MAX_LOG_TYPE; i++) {
      log_file.head[i] = NULL;
    }

    return 0;
  }
}

int vlog_file_close() {
  int i;
  VLOG_DATA *t_logdata, *t_logdata_free;
  int print_order[MAX_LOG_TYPE] = {LOG_MP4BOX, LOG_OBU, LOG_DECOP};

  if (log_file.f && log_file.is_open) {
    for (i = 0; i < MAX_LOG_TYPE; i++) {
      t_logdata = log_file.head[print_order[i]];
      while (t_logdata) {
        if (t_logdata->is_longtext) {
          fwrite(t_logdata->ltext, 1, strlen(t_logdata->ltext), log_file.f);
          free(t_logdata->ltext);
          t_logdata_free = t_logdata;
          t_logdata = t_logdata->next;
          free(t_logdata_free);
        } else {
          fwrite(t_logdata->text, 1, strlen(t_logdata->text), log_file.f);
          t_logdata_free = t_logdata;
          t_logdata = t_logdata->next;
          free(t_logdata_free);
        }
      }
      log_file.head[print_order[i]] = NULL;
    }

    fclose(log_file.f);
    log_file.f = NULL;
    memset(log_file.file_name, 0, sizeof(log_file.file_name));
    log_file.is_open = 0;

    return 0;
  }

  return -1;
}

int is_vlog_file_open() {
  if (!log_file.f || !log_file.is_open) return 0;
  return (1);
}

int vlog_print(LOG_TYPE type, uint64_t key, const char* format, ...) {
  if (!log_file.f || !log_file.is_open) return -1;

  va_list args;
  int len = 0;
  char* buffer = NULL;
  VLOG_DATA *t_logdata, *logdata_new;

  va_start(args, format);
#if defined(_WIN32)
  len = _vscprintf(format, args) + 1;  // terminateing '\0'
#else
  len = vprintf(format, args) + 1;
#endif

  buffer = (char*)malloc(len * sizeof(char));
  if (buffer) {
#if defined(_WIN32)
    vsprintf_s(buffer, len, format, args);
#else
    vsnprintf(buffer, len, format, args);
#endif
  }
  va_end(args);

  logdata_new = malloc(sizeof(VLOG_DATA));
  if (logdata_new) {
    // set up new logdata
    logdata_new->log_type = type;
    logdata_new->key = key;
    if (len < sizeof(logdata_new->text)) {  // short text log
      strcpy(logdata_new->text, buffer);
      free(buffer);
      logdata_new->is_longtext = 0;
      logdata_new->ltext = NULL;
    } else {  // longtext log
      logdata_new->is_longtext = 1;
      logdata_new->ltext = buffer;
      logdata_new->text[0] = 0;
    }

    // add new logdata into logdata chain
    t_logdata = log_file.head[type];
    if (log_file.head[type] == NULL) {  // head is empty
      logdata_new->prev = NULL;
      logdata_new->next = NULL;
      log_file.head[type] = logdata_new;
    } else {  // head is ocuppied.
      if (logdata_new->key <
          t_logdata->key)  // t_logdata == log_file.head[log_type];
      {                    // head is needed to update
        logdata_new->next = t_logdata->next;
        t_logdata->prev = logdata_new;
        log_file.head[type] = logdata_new;
      } else {  // body or tail is needed to update
        while (t_logdata) {
          if (logdata_new->key < t_logdata->key) {  // add ne logdata into body
            logdata_new->next = t_logdata;
            t_logdata->prev->next = logdata_new;
            logdata_new->prev = t_logdata->prev;
            t_logdata->prev = logdata_new;
            break;
          } else {
            if (t_logdata->next) {  // skip this t_logdata
              t_logdata = t_logdata->next;
            } else {  // add new logdata into tail
              logdata_new->prev = t_logdata;
              t_logdata->next = logdata_new;
              logdata_new->next = NULL;
              break;
            }
          }
        }
      }
    }
  }

  return 0;
}

int write_prefix(LOG_TYPE type, char* buf) {
  int len = 0;

  switch (type) {
    case LOG_OBU:
      len = sprintf(buf, "#0\n");
      break;
    case LOG_MP4BOX:
      len = sprintf(buf, "#1\n");
      break;
    case LOG_DECOP:
      len = sprintf(buf, "#2\n");
      break;
    default:
      break;
  }

  return len;
}

int write_postfix(LOG_TYPE type, char* buf) {
  int len = 0;

  switch (type) {
    case LOG_OBU:
    case LOG_MP4BOX:
    case LOG_DECOP:
      len = sprintf(buf, "##\n");
      break;
    default:
      break;
  }

  return len;
}

int write_yaml_form(char* log, uint8_t indent, const char* format, ...) {
  int ret = 0;
  for (uint8_t i = 0; i < indent; ++i) {
    ret += sprintf(log + ret, "  ");
  }

  va_list args;
  int len = 0;

  va_start(args, format);
#if defined(_WIN32)
  len = _vscprintf(format, args) + 1;  // terminateing '\0'
  vsprintf_s(log + ret, len, format, args);
#else
  len = vprintf(format, args) + 1;
  va_start(args, format);
  vsnprintf(log + ret, len, format, args);
#endif
  va_end(args);

  ret += len - 1;
  ret += sprintf(log + ret, "\n");

  return ret;
}

#if defined(SUPPORT_VERIFIER)
/**
 * @brief Writes IAMF sequence header OBU information to log
 *
 * This function logs the sequence header OBU data including IAMF code,
 * primary profile, and additional profile information in YAML format.
 *
 * @param idx The index/ID of the OBU being logged
 * @param obu Pointer to the IAMF sequence header OBU data
 * @param log Buffer to write the log output to
 *
 * @note This function performs input validation and will handle NULL pointers
 *       gracefully by writing an error message to the log.
 */
static void write_sequence_header_log(uint64_t idx, void* obu, char* log) {
  // Input validation
  if (!obu) {
    log += write_prefix(LOG_OBU, log);
    log += write_yaml_form(log, 0, "IaSequenceHeaderOBU_%u:", idx);
    log += write_yaml_form(log, 0, "- error: \"NULL OBU pointer\"");
    write_postfix(LOG_OBU, log);
    return;
  }

  if (!log) {
    return;  // Cannot log if buffer is NULL
  }

  // Cast to the correct structure type
  iamf_sequence_header_obu_t* sequence_header =
      (iamf_sequence_header_obu_t*)obu;

  // Write log header
  log += write_prefix(LOG_OBU, log);
  log += write_yaml_form(log, 0, "IaSequenceHeaderOBU_%u:", idx);

  // Write IAMF code with proper byte order conversion
  uint32_t iamf_code_host = htonl2(sequence_header->iamf_code);
  log += write_yaml_form(log, 0, "- ia_code: %u", iamf_code_host);

  // Write profile information
  log += write_yaml_form(log, 1, "primary_profile: %u",
                         sequence_header->primary_profile);
  log += write_yaml_form(log, 1, "additional_profile: %u",
                         sequence_header->additional_profile);

  // Write log footer
  write_postfix(LOG_OBU, log);
}

/**
 * @brief Writes IAMF codec config OBU information to log
 *
 * This function logs the codec config OBU data including codec ID, samples per
 * frame, and decoder-specific configuration information in YAML format. It
 * supports multiple codec types (AAC, FLAC, Opus, LPCM) with detailed parsing
 * of their respective decoder configurations.
 *
 * @param idx The index/ID of the OBU being logged
 * @param obu Pointer to the IAMF codec config OBU data
 * @param log Buffer to write the log output to
 *
 * @note This function performs input validation and will handle NULL pointers
 *       gracefully by writing an error message to the log.
 */
static void write_codec_config_log(uint64_t idx, void* obu, char* log) {
  iamf_codec_config_obu_t* codec_config = (iamf_codec_config_obu_t*)obu;

  log += write_prefix(LOG_OBU, log);
  log += write_yaml_form(log, 0, "CodecConfigOBU_%llu:", idx);
  log += write_yaml_form(log, 0, "- codec_config_id: %u",
                         codec_config->codec_config_id);
  log += write_yaml_form(log, 1, "codec_config:");
  log +=
      write_yaml_form(log, 2, "codec_id: %u", htonl2(codec_config->codec_id));
  log += write_yaml_form(log, 2, "num_samples_per_frame: %u",
                         codec_config->num_samples_per_frame);
  log += write_yaml_form(log, 2, "audio_roll_distance: %d",
                         codec_config->audio_roll_distance);

  // NOTE: Self parsing

  if (codec_config->codec_id == get_4cc_codec_id('m', 'p', '4', 'a') ||
      codec_config->codec_id == get_4cc_codec_id('e', 's', 'd', 's')) {
    log += write_yaml_form(log, 2, "decoder_config_aac:");

    io_context_t io_ctx;
    bits_io_context_t bits_ctx;
    ioc_init(&io_ctx, codec_config->decoder_config->data,
             codec_config->decoder_config->size);
    bits_ioc_init(&bits_ctx, &io_ctx);

    // decoder_config_descriptor_tag
    // object_type_indication
    // stream_type
    // upstream
    // reserved
    // buffer_size_db
    // max_bitrate
    // average_bit_rate
    // decoder_specific_info
    //    - decoder_specific_info_descriptor_tag
    //    - audio_object_type
    //    - sample_frequency_index
    //    - sampleing_frequency
    //    - channel_configuration
    // ga_specific_config
    //    - frame_length_flag
    //    - depends_on_core_coder
    //    - extension_flag

    log += write_yaml_form(log, 3, "decoder_config_descriptor_tag: %u",
                           bits_ior_le32(&bits_ctx, 8));
    log += write_yaml_form(log, 3, "object_type_indication: %u",
                           bits_ior_le32(&bits_ctx, 8));
    log +=
        write_yaml_form(log, 3, "stream_type: %u", bits_ior_le32(&bits_ctx, 6));
    log += write_yaml_form(log, 3, "upstream: %u", bits_ior_le32(&bits_ctx, 1));
    bits_ior_le32(&bits_ctx, 1);  // reserved

    bits_ior_le32(&bits_ctx, 24);  // buffer_size_db
    bits_ior_le32(&bits_ctx, 32);  // max_bitrate
    bits_ior_le32(&bits_ctx, 32);  // average_bit_rate

    log += write_yaml_form(log, 3, "decoder_specific_info:");
    log += write_yaml_form(log, 4, "decoder_specific_info_descriptor_tag: %u",
                           bits_ior_le32(&bits_ctx, 8));
    log += write_yaml_form(log, 4, "audio_object_type: %u",
                           bits_ior_le32(&bits_ctx, 5));
    uint8_t sample_frequency_index =
        bits_ior_le32(&bits_ctx, 4);  // sample_frequency_index
    if (sample_frequency_index == 0xf) {
      bits_ior_le32(&bits_ctx, 24);  // sampling_frequency
    }
    log += write_yaml_form(log, 4, "channel_configuration: %u",
                           bits_ior_le32(&bits_ctx, 4));

    log += write_yaml_form(log, 3, "ga_specific_config:");
    log += write_yaml_form(log, 4, "frame_length_flag: %u",
                           bits_ior_le32(&bits_ctx, 1));
    log += write_yaml_form(log, 4, "depends_on_core_coder: %u",
                           bits_ior_le32(&bits_ctx, 1));
    log += write_yaml_form(log, 4, "extension_flag: %u",
                           bits_ior_le32(&bits_ctx, 1));
  } else if (codec_config->codec_id == get_4cc_codec_id('f', 'L', 'a', 'C')) {
    // "fLaC", METADATA_BLOCK

    log += write_yaml_form(log, 2, "decoder_config_flac:");
    log += write_yaml_form(log, 3, "metadata_blocks:");

    io_context_t io_ctx;
    bits_io_context_t bits_ctx;
    ioc_init(&io_ctx, codec_config->decoder_config->data,
             codec_config->decoder_config->size);
    bits_ioc_init(&bits_ctx, &io_ctx);

    uint8_t last_metadata_block_flag = 0;
    uint8_t block_type = 0;
    uint32_t metadata_data_block_length = 0;

    do {
      last_metadata_block_flag = bits_ior_le32(&bits_ctx, 1);
      block_type = bits_ior_le32(&bits_ctx, 7);
      metadata_data_block_length = bits_ior_le32(&bits_ctx, 24);

      log += write_yaml_form(log, 4, "- header:");
      log += write_yaml_form(log, 6, "last_metadata_block_flag: %u",
                             last_metadata_block_flag);
      log += write_yaml_form(log, 6, "block_type: %u", block_type);
      log += write_yaml_form(log, 6, "metadata_data_block_length: %lu",
                             metadata_data_block_length);

      // STREAM_INFO
      if (block_type == 0) {
        // <16>
        // <16>
        // <24>
        // <24>
        // <20>
        // <3>
        // <5>
        // <36>
        // <128>

        uint16_t minimum_block_size = bits_ior_le32(&bits_ctx, 16);
        uint16_t maximum_block_size = bits_ior_le32(&bits_ctx, 16);
        uint32_t minumum_frame_size = bits_ior_le32(&bits_ctx, 24);
        uint32_t maximum_frame_size = bits_ior_le32(&bits_ctx, 24);
        uint32_t sample_rate = bits_ior_le32(&bits_ctx, 20);
        uint8_t number_of_channels = bits_ior_le32(&bits_ctx, 3);
        uint8_t bits_per_sample = bits_ior_le32(&bits_ctx, 5);
        uint64_t total_samples_in_stream = 0;
        uint8_t md5_signature[16] = {
            0,
        };

        uint64_t be_value = 0;  // uint8_t be_value[8] = { 0, };
        ior_read(&io_ctx, (uint8_t*)&be_value, 4);
        be_value <<= 32;
        // ior_read(&io_ctx, (uint8_t*)&be_value + 4, 4);

        // htonll2
        total_samples_in_stream = htonll2(be_value);

        ior_read(&io_ctx, (uint8_t*)md5_signature, 16);

        log += write_yaml_form(log, 5, "stream_info:");

        log += write_yaml_form(log, 6, "minimum_block_size: %u",
                               minimum_block_size);
        log += write_yaml_form(log, 6, "maximum_block_size: %u",
                               maximum_block_size);
        log += write_yaml_form(log, 6, "minimum_frame_size: %lu",
                               minumum_frame_size);
        log += write_yaml_form(log, 6, "maximum_frame_size: %lu",
                               maximum_frame_size);
        log += write_yaml_form(log, 6, "sample_rate: %lu", sample_rate);
        log += write_yaml_form(log, 6, "number_of_channels: %u",
                               number_of_channels);
        log += write_yaml_form(log, 6, "bits_per_sample: %u", bits_per_sample);
        log += write_yaml_form(log, 6, "total_samples_in_stream: %llu",
                               total_samples_in_stream);

        char hex_string[34] = {
            0,
        };
        int pos = 0;
        for (int j = 0; j < 16; ++j) {
          pos += sprintf(hex_string + pos, "%X", md5_signature[j]);
        }
        log += write_yaml_form(log, 6, "md5_signature: %s", hex_string);
      }
    } while (last_metadata_block_flag == 0);
  } else if (codec_config->codec_id == get_4cc_codec_id('O', 'p', 'u', 's') ||
             codec_config->codec_id == get_4cc_codec_id('d', 'O', 'p', 's')) {
    log += write_yaml_form(log, 2, "decoder_config_opus:");

    io_context_t io_ctx;
    bits_io_context_t bits_ctx;
    ioc_init(&io_ctx, codec_config->decoder_config->data,
             codec_config->decoder_config->size);
    bits_ioc_init(&bits_ctx, &io_ctx);

    // <8>
    uint8_t version = bits_ior_le32(&bits_ctx, 8);
    uint8_t channel_count = bits_ior_le32(&bits_ctx, 8);
    uint16_t pre_skip = bits_ior_le32(&bits_ctx, 16);
    uint32_t input_sample_rate = bits_ior_le32(&bits_ctx, 32);
    uint16_t output_gain = (int16_t)bits_ior_le32(&bits_ctx, 16);
    uint8_t mapping_family = bits_ior_le32(&bits_ctx, 8);

    log += write_yaml_form(log, 3, "version: %u", version);
    log += write_yaml_form(log, 3, "output_channel_count: %u", channel_count);
    log += write_yaml_form(log, 3, "pre_skip: %u", pre_skip);
    log += write_yaml_form(log, 3, "input_sample_rate: %lu", input_sample_rate);
    log += write_yaml_form(log, 3, "output_gain: %d", output_gain);
    log += write_yaml_form(log, 3, "mapping_family: %u", mapping_family);

  } else if (codec_config->codec_id == get_4cc_codec_id('i', 'p', 'c', 'm')) {
    log += write_yaml_form(log, 2, "decoder_config_lpcm:");

    io_context_t io_ctx, *r;
    ioc_init(&io_ctx, codec_config->decoder_config->data,
             codec_config->decoder_config->size);

    r = &io_ctx;
    uint8_t sample_format_flags = ior_8(r);
    uint8_t sample_size = ior_8(r);
    uint32_t sample_rate = ior_b32(r);

    log +=
        write_yaml_form(log, 3, "sample_format_flags: %u", sample_format_flags);
    log += write_yaml_form(log, 3, "sample_size: %u", sample_size);
    log += write_yaml_form(log, 3, "sample_rate: %lu", sample_rate);
  }

  write_postfix(LOG_OBU, log);
}

/**
 * @brief Writes IAMF audio element OBU information to log
 *
 * This function logs the audio element OBU data including element ID, type,
 * codec configuration, substream IDs, and parameters in YAML format. It
 * supports both channel-based and scene-based audio elements with their
 * respective configuration details.
 *
 * @param idx The index/ID of the OBU being logged
 * @param obu Pointer to the IAMF audio element OBU data
 * @param log Buffer to write the log output to
 *
 * @note This function performs input validation and will handle NULL pointers
 *       gracefully by writing an error message to the log.
 */
static void write_audio_element_log(uint64_t idx, void* obu, char* log) {
  iamf_audio_element_obu_t* audio_element = (iamf_audio_element_obu_t*)obu;

  log += write_prefix(LOG_OBU, log);
  log += write_yaml_form(log, 0, "AudioElementOBU_%u:", idx);
  log += write_yaml_form(log, 0, "- audio_element_id: %u",
                         audio_element->audio_element_id);
  log += write_yaml_form(log, 1, "audio_element_type: %u",
                         audio_element->audio_element_type);
  log += write_yaml_form(log, 1, "codec_config_id: %u",
                         audio_element->codec_config_id);
  log += write_yaml_form(log, 1, "num_substreams: %u",
                         audio_element->audio_substream_ids
                             ? array_size(audio_element->audio_substream_ids)
                             : 0);

  log += write_yaml_form(log, 1, "audio_substream_ids:");
  if (audio_element->audio_substream_ids) {
    int32_t substream_count = array_size(audio_element->audio_substream_ids);
    for (int32_t i = 0; i < substream_count; ++i) {
      uint32_t substream_id =
          def_value_wrap_i32(array_at(audio_element->audio_substream_ids, i));
      log += write_yaml_form(log, 1, "- %u", substream_id);
    }
  }
  log += write_yaml_form(
      log, 1, "num_parameters: %u",
      audio_element->parameters ? array_size(audio_element->parameters) : 0);
  if (audio_element->parameters && array_size(audio_element->parameters) > 0) {
    log += write_yaml_form(log, 1, "audio_element_params:");
    int32_t param_count = array_size(audio_element->parameters);
    for (int32_t i = 0; i < param_count; ++i) {
      parameter_base_t* param = def_value_wrap_type_ptr(
          parameter_base_t, array_at(audio_element->parameters, i));
      if (!param) continue;
      log +=
          write_yaml_form(log, 1, "- param_definition_type: %u", param->type);

      if (param->type == ck_iamf_parameter_type_demixing) {
        demixing_info_parameter_base_t* dp =
            (demixing_info_parameter_base_t*)param;
        log += write_yaml_form(log, 2, "demixing_param:");

        log += write_yaml_form(log, 3, "param_definition:");
        log +=
            write_yaml_form(log, 4, "parameter_id: %lu", dp->base.parameter_id);
        log += write_yaml_form(log, 4, "parameter_rate: %lu",
                               dp->base.parameter_rate);
        log += write_yaml_form(log, 4, "param_definition_mode: %u",
                               dp->base.param_definition_mode);
        if (dp->base.param_definition_mode == 0) {
          log += write_yaml_form(log, 4, "duration: %lu", dp->base.duration);
          int32_t subblock_count = dp->base.subblock_durations
                                       ? array_size(dp->base.subblock_durations)
                                   : dp->base.constant_subblock_duration > 0
                                       ? 1
                                       : 0;
          log += write_yaml_form(log, 4, "num_subblocks: %lu", subblock_count);
          log += write_yaml_form(log, 4, "constant_subblock_duration: %lu",
                                 dp->base.constant_subblock_duration);
          if (dp->base.constant_subblock_duration == 0 &&
              dp->base.subblock_durations) {
            log += write_yaml_form(log, 4, "subblock_durations:");
            for (int32_t j = 0; j < subblock_count; ++j) {
              uint32_t duration =
                  def_value_wrap_i32(array_at(dp->base.subblock_durations, j));
              log += write_yaml_form(log, 4, "- %u", duration);
            }
          }
        }
        log += write_yaml_form(log, 3, "default_demixing_info_parameter_data:");
        log += write_yaml_form(log, 4, "dmixp_mode: %lu", dp->dmixp_mode);
        log += write_yaml_form(log, 3, "default_w: %lu", dp->default_w);
      } else if (param->type == ck_iamf_parameter_type_recon_gain) {
        recon_gain_parameter_base_t* rg = (recon_gain_parameter_base_t*)param;
        log += write_yaml_form(log, 2, "recon_gain_param:");
        log += write_yaml_form(log, 3, "param_definition:");
        log += write_yaml_form(log, 4, "parameter_id: %u", param->parameter_id);
        log += write_yaml_form(log, 4, "parameter_rate: %u",
                               param->parameter_rate);
        log += write_yaml_form(log, 4, "param_definition_mode: %u",
                               param->param_definition_mode);
        if (param->param_definition_mode == 0) {
          log += write_yaml_form(log, 4, "duration: %lu", rg->base.duration);
          int32_t subblock_count = rg->base.subblock_durations
                                       ? array_size(rg->base.subblock_durations)
                                   : rg->base.constant_subblock_duration > 0
                                       ? 1
                                       : 0;
          log += write_yaml_form(log, 4, "num_subblocks: %lu", subblock_count);
          log += write_yaml_form(log, 4, "constant_subblock_duration: %u",
                                 param->constant_subblock_duration);
          if (param->constant_subblock_duration == 0 &&
              param->subblock_durations) {
            log += write_yaml_form(log, 4, "subblock_durations:");
            for (int32_t j = 0; j < subblock_count; ++j) {
              uint32_t duration =
                  def_value_wrap_i32(array_at(param->subblock_durations, j));
              log += write_yaml_form(log, 4, "- %u", duration);
            }
          }
        }
      }
    }
  }

  if (audio_element->audio_element_type ==
      ck_audio_element_type_channel_based) {
    log += write_yaml_form(log, 1, "scalable_channel_layout_config:");
    channel_based_audio_element_obu_t* channel_element =
        (channel_based_audio_element_obu_t*)audio_element;
    int32_t layer_count =
        channel_element->channel_audio_layer_configs
            ? array_size(channel_element->channel_audio_layer_configs)
            : 0;
    log += write_yaml_form(log, 2, "num_layers: %u", layer_count);
    log += write_yaml_form(log, 2, "channel_audio_layer_configs:");
    for (int32_t i = 0; i < layer_count; ++i) {
      obu_channel_layer_config_t* layer_config = def_value_wrap_type_ptr(
          obu_channel_layer_config_t,
          array_at(channel_element->channel_audio_layer_configs, i));
      log += write_yaml_form(log, 2, "- loudspeaker_layout: %u",
                             layer_config->loudspeaker_layout);
      log += write_yaml_form(log, 3, "output_gain_is_present_flag: %u",
                             layer_config->output_gain_is_present_flag);
      log += write_yaml_form(log, 3, "recon_gain_is_present_flag: %u",
                             layer_config->recon_gain_is_present_flag);
      log += write_yaml_form(log, 3, "substream_count: %u",
                             layer_config->substream_count);
      log += write_yaml_form(log, 3, "coupled_substream_count: %u",
                             layer_config->coupled_substream_count);
      if (layer_config->output_gain_is_present_flag) {
        log += write_yaml_form(log, 3, "output_gain_flag: %u",
                               layer_config->output_gain_info.output_gain_flag);
        log += write_yaml_form(log, 3, "output_gain: %d",
                               layer_config->output_gain_info.output_gain);
      }
      if (!i && layer_config->loudspeaker_layout == 0xf)
        log += write_yaml_form(log, 3, "expanded_loudspeaker_layout: %u",
                               layer_config->expanded_loudspeaker_layout);
    }
  } else if (audio_element->audio_element_type ==
             ck_audio_element_type_scene_based) {
    log += write_yaml_form(log, 1, "ambisonics_config:");
    scene_based_audio_element_obu_t* scene_element =
        (scene_based_audio_element_obu_t*)audio_element;
    log += write_yaml_form(log, 2, "ambisonics_mode: %u",
                           scene_element->ambisonics_mode);
    if (scene_element->ambisonics_mode == ck_ambisonics_mode_mono) {
      log += write_yaml_form(log, 2, "ambisonics_mono_config:");
      log += write_yaml_form(log, 3, "output_channel_count: %u",
                             scene_element->output_channel_count);
      log += write_yaml_form(log, 3, "substream_count: %u",
                             scene_element->substream_count);
      log += write_yaml_form(log, 3, "channel_mapping:");
      for (uint32_t i = 0; i < scene_element->mapping_size; ++i) {
        log +=
            write_yaml_form(log, 3, "- %u", scene_element->channel_mapping[i]);
      }
    } else if (scene_element->ambisonics_mode ==
               ck_ambisonics_mode_projection) {
      log += write_yaml_form(log, 2, "ambisonics_projection_config:");
      log += write_yaml_form(log, 3, "output_channel_count: %u",
                             scene_element->output_channel_count);
      log += write_yaml_form(log, 3, "substream_count: %u",
                             scene_element->substream_count);
      log += write_yaml_form(log, 3, "coupled_substream_count: %u",
                             scene_element->coupled_substream_count);
      log += write_yaml_form(log, 3, "demixing_matrix:");
      for (int i = 0; i < scene_element->mapping_size; i += 2) {
        log += write_yaml_form(log, 3, "- %d",
                               scene_element->demixing_matrix[i] << 8 |
                                   scene_element->demixing_matrix[i + 1]);
      }
    }
  } else if (audio_element->audio_element_type ==
             ck_audio_element_type_object_based) {
    log += write_yaml_form(log, 1, "objects_config:");
    object_based_audio_element_obu_t* object_element =
        (object_based_audio_element_obu_t*)audio_element;
    log +=
        write_yaml_form(log, 2, "num_objects: %u", object_element->num_objects);
  }

  write_postfix(LOG_OBU, log);
}

/**
 * @brief Writes IAMF mix presentation OBU information to log
 *
 * This function logs the mix presentation OBU data including mix presentation
 * ID, annotations, sub-mixes, audio elements, layouts, and loudness information
 * in YAML format.
 *
 * @param idx The index/ID of the OBU being logged
 * @param obu Pointer to the IAMF mix presentation OBU data
 * @param log Buffer to write the log output to
 *
 * @note This function performs input validation and will handle NULL pointers
 *       gracefully by writing an error message to the log.
 */
static void write_mix_presentation_log(uint64_t idx, void* obu, char* log) {
  iamf_mix_presentation_obu_t* mix_presentation =
      (iamf_mix_presentation_obu_t*)obu;

  log += write_prefix(LOG_OBU, log);
  log += write_yaml_form(log, 0, "MixPresentationOBU_%llu:", idx);
  log += write_yaml_form(log, 0, "- mix_presentation_id: %u",
                         mix_presentation->mix_presentation_id);
  log +=
      write_yaml_form(log, 1, "count_label: %u",
                      mix_presentation->annotations_languages
                          ? array_size(mix_presentation->annotations_languages)
                          : 0);
  log += write_yaml_form(log, 1, "annotations_language:");
  if (mix_presentation->annotations_languages) {
    int32_t label_count = array_size(mix_presentation->annotations_languages);
    for (int32_t i = 0; i < label_count; ++i) {
      string128_t* lang = def_value_wrap_type_ptr(
          string128_t, array_at(mix_presentation->annotations_languages, i));
      log += write_yaml_form(log, 1, "- \"%s\"", *lang);
    }
  }
  log += write_yaml_form(log, 1, "localized_presentation_annotations:");
  if (mix_presentation->localized_presentation_annotations) {
    int32_t annotation_count =
        array_size(mix_presentation->localized_presentation_annotations);
    for (int32_t i = 0; i < annotation_count; ++i) {
      string128_t* annotation = def_value_wrap_type_ptr(
          string128_t,
          array_at(mix_presentation->localized_presentation_annotations, i));
      log += write_yaml_form(log, 1, "- \"%s\"", *annotation);
    }
  }
  log += write_yaml_form(log, 1, "num_sub_mixes: %u",
                         mix_presentation->sub_mixes
                             ? array_size(mix_presentation->sub_mixes)
                             : 0);
  log += write_yaml_form(log, 1, "sub_mixes:");
  if (mix_presentation->sub_mixes) {
    int32_t submix_count = array_size(mix_presentation->sub_mixes);
    for (int32_t i = 0; i < submix_count; ++i) {
      obu_sub_mix_t* submix = def_value_wrap_type_ptr(
          obu_sub_mix_t, array_at(mix_presentation->sub_mixes, i));
      log += write_yaml_form(log, 1, "- num_audio_elements: %u",
                             submix->audio_element_configs
                                 ? array_size(submix->audio_element_configs)
                                 : 0);
      log += write_yaml_form(log, 2, "audio_elements:");
      if (submix->audio_element_configs) {
        int32_t element_count = array_size(submix->audio_element_configs);
        for (int32_t j = 0; j < element_count; ++j) {
          obu_audio_element_config_t* element_config = def_value_wrap_type_ptr(
              obu_audio_element_config_t,
              array_at(submix->audio_element_configs, j));
          log += write_yaml_form(log, 2, "- audio_element_id: %u",
                                 element_config->element_id);
          log += write_yaml_form(log, 3, "localized_element_annotations:");
          if (element_config->localized_element_annotations) {
            int32_t annotation_count =
                array_size(element_config->localized_element_annotations);
            for (int32_t k = 0; k < annotation_count; ++k) {
              string128_t* element_annotation = def_value_wrap_type_ptr(
                  string128_t,
                  array_at(element_config->localized_element_annotations, k));
              log += write_yaml_form(log, 3, "- \"%s\"", *element_annotation);
            }
          }

          log += write_yaml_form(log, 3, "rendering_config:");
          log += write_yaml_form(
              log, 4, "headphones_rendering_mode: %u",
              element_config->rendering_config.headphones_rendering_mode);
          log += write_yaml_form(
              log, 4, "binaural_filter_profile: %u",
              element_config->rendering_config.binaural_filter_profile);
          // log += write_yaml_form(
          //     log, 4, "rendering_config_extension_size: %u",
          //     element_config->rendering_config.rendering_config_extension_size);

          if (element_config->rendering_config.parameters) {
            log +=
                write_yaml_form(log, 4, "rendering_config_param_definitions:");
            parameter_base_t* param = def_value_wrap_type_ptr(
                parameter_base_t,
                array_at(element_config->rendering_config.parameters, 0));

            if (param) {
              if (param->type == ck_iamf_parameter_type_polar) {
                polars_parameter_base_t* polar_param =
                    (polars_parameter_base_t*)param;
                log += write_yaml_form(log, 5, "- param_definition_type: %u",
                                       param->type);
                log += write_yaml_form(log, 6, "polar_param_definition:");
                log += write_yaml_form(log, 7, "param_definition:");
                log += write_yaml_form(log, 8, "parameter_id: %u",
                                       param->parameter_id);
                log += write_yaml_form(log, 8, "parameter_rate: %u",
                                       param->parameter_rate);
                log += write_yaml_form(log, 8, "param_definition_mode: %u",
                                       param->param_definition_mode);
                log += write_yaml_form(log, 7, "default_azimuth: %f",
                                       polar_param->default_polars[0].azimuth);
                log +=
                    write_yaml_form(log, 7, "default_elevation: %f",
                                    polar_param->default_polars[0].elevation);
                log += write_yaml_form(
                    log, 7, "default_distance: %f",
                    polar_param->default_polars[0].distance * 127);
              } else if (param->type == ck_iamf_parameter_type_cartesian_8) {
                cartesians_parameter_base_t* cart_param =
                    (cartesians_parameter_base_t*)param;
                log += write_yaml_form(log, 5, "- param_definition_type: %u",
                                       param->type);
                log += write_yaml_form(log, 6, "cart8_param_definition:");
                log += write_yaml_form(log, 7, "param_definition:");
                log += write_yaml_form(log, 8, "parameter_id: %u",
                                       param->parameter_id);
                log += write_yaml_form(log, 8, "parameter_rate: %u",
                                       param->parameter_rate);
                log += write_yaml_form(log, 8, "param_definition_mode: %u",
                                       param->param_definition_mode);
                log +=
                    write_yaml_form(log, 7, "default_x: %f",
                                    cart_param->default_cartesians[0].x * 127);
                log +=
                    write_yaml_form(log, 7, "default_y: %f",
                                    cart_param->default_cartesians[0].y * 127);
                log +=
                    write_yaml_form(log, 7, "default_z: %f",
                                    cart_param->default_cartesians[0].z * 127);
              } else if (param->type == ck_iamf_parameter_type_cartesian_16) {
                cartesians_parameter_base_t* cart_param =
                    (cartesians_parameter_base_t*)param;
                log += write_yaml_form(log, 5, "- param_definition_type: %u",
                                       param->type);
                log += write_yaml_form(log, 6, "cart16_param_definition:");
                log += write_yaml_form(log, 7, "param_definition:");
                log += write_yaml_form(log, 8, "parameter_id: %u",
                                       param->parameter_id);
                log += write_yaml_form(log, 8, "parameter_rate: %u",
                                       param->parameter_rate);
                log += write_yaml_form(log, 8, "param_definition_mode: %u",
                                       param->param_definition_mode);
                log += write_yaml_form(
                    log, 7, "default_x: %f",
                    cart_param->default_cartesians[0].x * 32767);
                log += write_yaml_form(
                    log, 7, "default_y: %f",
                    cart_param->default_cartesians[0].y * 32767);
                log += write_yaml_form(
                    log, 7, "default_z: %f",
                    cart_param->default_cartesians[0].z * 32767);
              } else if (param->type == ck_iamf_parameter_type_dual_polar) {
                polars_parameter_base_t* polar_param =
                    (polars_parameter_base_t*)param;
                log += write_yaml_form(log, 5, "- param_definition_type: %u",
                                       param->type);
                log += write_yaml_form(log, 6, "dual_polar_param_definition:");
                log += write_yaml_form(log, 7, "param_definition:");
                log += write_yaml_form(log, 8, "parameter_id: %u",
                                       param->parameter_id);
                log += write_yaml_form(log, 8, "parameter_rate: %u",
                                       param->parameter_rate);
                log += write_yaml_form(log, 8, "param_definition_mode: %u",
                                       param->param_definition_mode);
                log += write_yaml_form(log, 7, "default_first_azimuth: %f",
                                       polar_param->default_polars[0].azimuth);
                log +=
                    write_yaml_form(log, 7, "default_first_elevation: %f",
                                    polar_param->default_polars[0].elevation);
                log += write_yaml_form(
                    log, 7, "default_first_distance: %f",
                    polar_param->default_polars[0].distance * 127);
                log += write_yaml_form(log, 7, "default_second_azimuth: %f",
                                       polar_param->default_polars[1].azimuth);
                log +=
                    write_yaml_form(log, 7, "default_second_elevation: %f",
                                    polar_param->default_polars[1].elevation);
                log += write_yaml_form(
                    log, 7, "default_second_distance: %f",
                    polar_param->default_polars[1].distance * 127);
              } else if (param->type ==
                         ck_iamf_parameter_type_dual_cartesian_8) {
                cartesians_parameter_base_t* cart_param =
                    (cartesians_parameter_base_t*)param;
                log += write_yaml_form(log, 5, "- param_definition_type: %u",
                                       param->type);
                log += write_yaml_form(log, 6, "dual_cart8_param_definition:");
                log += write_yaml_form(log, 7, "param_definition:");
                log += write_yaml_form(log, 8, "parameter_id: %u",
                                       param->parameter_id);
                log += write_yaml_form(log, 8, "parameter_rate: %u",
                                       param->parameter_rate);
                log += write_yaml_form(log, 8, "param_definition_mode: %u",
                                       param->param_definition_mode);
                log +=
                    write_yaml_form(log, 7, "default_first_x: %f",
                                    cart_param->default_cartesians[0].x * 127);
                log +=
                    write_yaml_form(log, 7, "default_first_y: %f",
                                    cart_param->default_cartesians[0].y * 127);
                log +=
                    write_yaml_form(log, 7, "default_first_z: %f",
                                    cart_param->default_cartesians[0].z * 127);
                log +=
                    write_yaml_form(log, 7, "default_second_x: %f",
                                    cart_param->default_cartesians[1].x * 127);
                log +=
                    write_yaml_form(log, 7, "default_second_y: %f",
                                    cart_param->default_cartesians[1].y * 127);
                log +=
                    write_yaml_form(log, 7, "default_second_z: %f",
                                    cart_param->default_cartesians[1].z * 127);
              } else if (param->type ==
                         ck_iamf_parameter_type_dual_cartesian_16) {
                cartesians_parameter_base_t* cart_param =
                    (cartesians_parameter_base_t*)param;
                log += write_yaml_form(log, 5, "- param_definition_type: %u",
                                       param->type);
                log += write_yaml_form(log, 6, "dual_cart16_param_definition:");
                log += write_yaml_form(log, 7, "param_definition:");
                log += write_yaml_form(log, 8, "parameter_id: %u",
                                       param->parameter_id);
                log += write_yaml_form(log, 8, "parameter_rate: %u",
                                       param->parameter_rate);
                log += write_yaml_form(log, 8, "param_definition_mode: %u",
                                       param->param_definition_mode);
                log += write_yaml_form(
                    log, 7, "default_first_x: %f",
                    cart_param->default_cartesians[0].x * 32767);
                log += write_yaml_form(
                    log, 7, "default_first_y: %f",
                    cart_param->default_cartesians[0].y * 32767);
                log += write_yaml_form(
                    log, 7, "default_first_z: %f",
                    cart_param->default_cartesians[0].z * 32767);
                log += write_yaml_form(
                    log, 7, "default_second_x: %f",
                    cart_param->default_cartesians[1].x * 32767);
                log += write_yaml_form(
                    log, 7, "default_second_y: %f",
                    cart_param->default_cartesians[1].y * 32767);
                log += write_yaml_form(
                    log, 7, "default_second_z: %f",
                    cart_param->default_cartesians[1].z * 32767);
              }
            }
          }

          if (element_config->rendering_config.flags &
              def_rendering_config_flag_element_gain_offset) {
            log += write_yaml_form(log, 4, "element_gain_offset_config:");
            log += write_yaml_form(
                log, 4, "- element_gain_offset_type: %u",
                element_config->rendering_config.element_gain_offset_type);
            if (element_config->rendering_config.element_gain_offset_type ==
                ck_element_gain_offset_type_value) {
              log += write_yaml_form(log, 5, "value_type:");
              log += write_yaml_form(log, 6, "element_gain_offset:");
              log += write_yaml_form(
                  log, 7, "q7_dot8: %d",
                  element_config->rendering_config.element_gain_offset.value);
            } else if (element_config->rendering_config
                           .element_gain_offset_type ==
                       ck_element_gain_offset_type_range) {
              log += write_yaml_form(log, 5, "range_type:");
              log += write_yaml_form(log, 6, "default_element_gain_offset:");
              log += write_yaml_form(
                  log, 7, "q7_dot8: %d",
                  element_config->rendering_config.element_gain_offset.value);
              log += write_yaml_form(log, 6, "min_element_gain_offset:");
              log += write_yaml_form(
                  log, 7, "q7_dot8: %d",
                  element_config->rendering_config.element_gain_offset.min);
              log += write_yaml_form(log, 6, "max_element_gain_offset:");
              log += write_yaml_form(
                  log, 7, "q7_dot8: %d",
                  element_config->rendering_config.element_gain_offset.max);
            }
          }

          log += write_yaml_form(log, 3, "element_mix_gain:");
          log += write_yaml_form(log, 4, "param_definition:");
          log += write_yaml_form(
              log, 5, "parameter_id: %u",
              element_config->element_mix_gain.base.parameter_id);
          log += write_yaml_form(
              log, 5, "parameter_rate: %u",
              element_config->element_mix_gain.base.parameter_rate);
          log += write_yaml_form(
              log, 5, "param_definition_mode: %u",
              element_config->element_mix_gain.base.param_definition_mode);
          if (element_config->element_mix_gain.base.param_definition_mode ==
              0) {
            log +=
                write_yaml_form(log, 5, "duration: %u",
                                element_config->element_mix_gain.base.duration);
            log += write_yaml_form(
                log, 5, "num_subblocks: %u",
                element_config->element_mix_gain.base.subblock_durations
                    ? array_size(element_config->element_mix_gain.base
                                     .subblock_durations)
                : element_config->element_mix_gain.base
                        .constant_subblock_duration
                    ? 1
                    : 0);
            log += write_yaml_form(log, 5, "constant_subblock_duration: %u",
                                   element_config->element_mix_gain.base
                                       .constant_subblock_duration);
            if (element_config->element_mix_gain.base
                        .constant_subblock_duration == 0 &&
                element_config->element_mix_gain.base.subblock_durations) {
              log += write_yaml_form(log, 5, "subblock_durations:");
              int32_t subblock_count = array_size(
                  element_config->element_mix_gain.base.subblock_durations);
              for (int32_t k = 0; k < subblock_count; ++k) {
                uint32_t duration = def_value_wrap_i32(array_at(
                    element_config->element_mix_gain.base.subblock_durations,
                    k));
                log += write_yaml_form(log, 6, "- %u", duration);
              }
            }
          }
          log += write_yaml_form(
              log, 4, "default_mix_gain: %d",
              element_config->element_mix_gain.default_mix_gain);
        }
      }

      log += write_yaml_form(log, 2, "output_mix_gain:");
      log += write_yaml_form(log, 3, "param_definition:");
      log += write_yaml_form(log, 4, "parameter_id: %u",
                             submix->output_mix_gain.base.parameter_id);
      log += write_yaml_form(log, 4, "parameter_rate: %u",
                             submix->output_mix_gain.base.parameter_rate);
      log +=
          write_yaml_form(log, 4, "param_definition_mode: %u",
                          submix->output_mix_gain.base.param_definition_mode);
      if (submix->output_mix_gain.base.param_definition_mode == 0) {
        log += write_yaml_form(log, 4, "duration: %u",
                               submix->output_mix_gain.base.duration);
        log += write_yaml_form(
            log, 4, "num_subblocks: %u",
            submix->output_mix_gain.base.subblock_durations
                ? array_size(submix->output_mix_gain.base.subblock_durations)
            : submix->output_mix_gain.base.constant_subblock_duration ? 1
                                                                      : 0);
        log += write_yaml_form(
            log, 4, "constant_subblock_duration: %u",
            submix->output_mix_gain.base.constant_subblock_duration);
        if (submix->output_mix_gain.base.constant_subblock_duration == 0 &&
            submix->output_mix_gain.base.subblock_durations) {
          log += write_yaml_form(log, 4, "subblock_durations:");
          int32_t subblock_count =
              array_size(submix->output_mix_gain.base.subblock_durations);
          for (int32_t k = 0; k < subblock_count; ++k) {
            uint32_t duration = def_value_wrap_i32(
                array_at(submix->output_mix_gain.base.subblock_durations, k));
            log += write_yaml_form(log, 4, "- %u", duration);
          }
        }
      }
      log += write_yaml_form(log, 3, "default_mix_gain: %d",
                             submix->output_mix_gain.default_mix_gain);

      log += write_yaml_form(
          log, 2, "num_layouts: %u",
          submix->loudness_layouts ? array_size(submix->loudness_layouts) : 0);

      log += write_yaml_form(log, 2, "layouts:");
      if (submix->loudness_layouts) {
        int32_t layout_count = array_size(submix->loudness_layouts);
        for (int32_t j = 0; j < layout_count; ++j) {
          iamf_layout_t* layout = def_value_wrap_type_ptr(
              iamf_layout_t, array_at(submix->loudness_layouts, j));
          log += write_yaml_form(log, 2, "- loudness_layout:");
          uint32_t layout_type = layout->type;
          log += write_yaml_form(log, 4, "layout_type: %lu", layout_type);

          if (layout_type == ck_iamf_layout_type_loudspeakers_ss_convention) {
            log += write_yaml_form(log, 4, "ss_layout:");
            log += write_yaml_form(log, 5, "sound_system: %u",
                                   layout->sound_system);
          }

          log += write_yaml_form(log, 3, "loudness:");
          obu_loudness_info_t* loudness = def_value_wrap_type_ptr(
              obu_loudness_info_t, array_at(submix->loudness, j));
          log += write_yaml_form(log, 4, "info_type_bit_masks: [%u]",
                                 loudness->info_type);
          log += write_yaml_form(log, 4, "integrated_loudness: %d",
                                 loudness->integrated_loudness);
          log += write_yaml_form(log, 4, "digital_peak: %d",
                                 loudness->digital_peak);
          if (loudness->info_type & 1) {
            log +=
                write_yaml_form(log, 4, "true_peak: %d", loudness->true_peak);
          }

          if (loudness->info_type & 2) {
            log += write_yaml_form(log, 4, "anchored_loudness:");
            if (loudness->anchored_loudnesses) {
              int32_t anchor_count = array_size(loudness->anchored_loudnesses);
              log += write_yaml_form(log, 5, "num_anchored_loudness: %d",
                                     anchor_count);
              if (anchor_count > 0) {
                log += write_yaml_form(log, 5, "anchor_elements:");
                for (int32_t k = 0; k < anchor_count; ++k) {
                  obu_anchored_loudness_info_t* anchor_info =
                      def_value_wrap_type_ptr(
                          obu_anchored_loudness_info_t,
                          array_at(loudness->anchored_loudnesses, k));
                  log += write_yaml_form(log, 5, "- anchor_element: %u",
                                         anchor_info->anchor_element);
                  log += write_yaml_form(log, 6, "anchored_loudness: %d",
                                         anchor_info->anchored_loudness);
                }
              }
            }
          }
        }
      }
    }
  }
  if (mix_presentation->mix_presentation_tags &&
      array_size(mix_presentation->mix_presentation_tags) > 0) {
    log += write_yaml_form(log, 1, "mix_presentation_tags:");
    log += write_yaml_form(log, 2, "num_tags: %u",
                           array_size(mix_presentation->mix_presentation_tags));
    log += write_yaml_form(log, 2, "tags:");
    int32_t tag_count = array_size(mix_presentation->mix_presentation_tags);
    for (int32_t i = 0; i < tag_count; ++i) {
      iamf_tag_t* tag = def_value_wrap_type_ptr(
          iamf_tag_t, array_at(mix_presentation->mix_presentation_tags, i));
      log += write_yaml_form(log, 2, "- tag_name: \"%s\"", tag->name);
      log += write_yaml_form(log, 3, "tag_value: \"%s\"", tag->value);
    }
  }

  log += write_yaml_form(log, 1, "optional_fields:");
  log += write_yaml_form(log, 2, "preferred_loudspeaker_renderer: %u",
                         mix_presentation->preferred_loudspeaker_renderer);
  log += write_yaml_form(log, 2, "preferred_binaural_renderer: %u",
                         mix_presentation->preferred_binaural_renderer);

  write_postfix(LOG_OBU, log);
}

/**
 * @brief Writes IAMF parameter block OBU information to log
 *
 * This function logs the parameter block OBU data including parameter ID,
 * duration, subblock information, and parameter-specific data in YAML format.
 * It supports mix gain, demixing, and reconstruction gain parameters with their
 * respective animation types and data.
 *
 * @param idx The index/ID of the OBU being logged
 * @param obu Pointer to the IAMF parameter block OBU data
 * @param log Buffer to write the log output to
 *
 * @note This function performs input validation and will handle NULL pointers
 *       gracefully by writing an error message to the log.
 */
static void write_parameter_block_log(uint64_t idx, void* obu, char* log) {
  iamf_parameter_block_obu_t* param_block = (iamf_parameter_block_obu_t*)obu;

  log += write_prefix(LOG_OBU, log);
  log += write_yaml_form(log, 0, "ParameterBlockOBU_%llu:", idx);
  log += write_yaml_form(log, 0, "- parameter_id: %u",
                         param_block->base->parameter_id);
  log += write_yaml_form(log, 1, "duration: %u", param_block->duration);
  log += write_yaml_form(
      log, 1, "num_subblocks: %u",
      param_block->subblocks ? array_size(param_block->subblocks) : 0);
  log += write_yaml_form(log, 1, "constant_subblock_duration: %u",
                         param_block->constant_subblock_duration);
  log += write_yaml_form(log, 1, "subblocks:");
  if (param_block->subblocks) {
    int32_t subblock_count = array_size(param_block->subblocks);
    for (int32_t i = 0; i < subblock_count; ++i) {
      if (param_block->base->type == ck_iamf_parameter_type_mix_gain) {
        mix_gain_parameter_subblock_t* mg_subblock =
            (mix_gain_parameter_subblock_t*)def_value_wrap_ptr(
                array_at(param_block->subblocks, i));
        log += write_yaml_form(log, 1, "- mix_gain_parameter_data:");
        log += write_yaml_form(log, 3, "subblock_duration: %u",
                               mg_subblock->base.subblock_duration);
        log += write_yaml_form(log, 3, "animation_type: %u",
                               mg_subblock->gain.animation_type);
        log += write_yaml_form(log, 3, "param_data:");
        if (mg_subblock->gain.animation_type == ck_iamf_animation_type_step) {
          log += write_yaml_form(log, 4, "step:");
          log += write_yaml_form(log, 5, "start_point_value: %d",
                                 mg_subblock->gain.data.start);
        } else if (mg_subblock->gain.animation_type ==
                   ck_iamf_animation_type_linear) {
          log += write_yaml_form(log, 4, "linear:");
          log += write_yaml_form(log, 5, "start_point_value: %d",
                                 mg_subblock->gain.data.start);
          log += write_yaml_form(log, 5, "end_point_value: %d",
                                 mg_subblock->gain.data.end);
        } else if (mg_subblock->gain.animation_type ==
                   ck_iamf_animation_type_bezier) {
          log += write_yaml_form(log, 4, "bezier:");
          log += write_yaml_form(log, 5, "start_point_value: %d",
                                 mg_subblock->gain.data.start);
          log += write_yaml_form(log, 5, "end_point_value: %d",
                                 mg_subblock->gain.data.end);
          log += write_yaml_form(log, 5, "control_point_value: %d",
                                 mg_subblock->gain.data.control);
          log += write_yaml_form(
              log, 5, "control_point_relative_time: %u",
              mg_subblock->gain.data.control_relative_time & 0xFF);
        }
      } else if (param_block->base->type == ck_iamf_parameter_type_demixing) {
        demixing_info_parameter_subblock_t* dmix_subblock =
            (demixing_info_parameter_subblock_t*)def_value_wrap_ptr(
                array_at(param_block->subblocks, i));
        log += write_yaml_form(log, 1, "- demixing_info_parameter_data:");
        log += write_yaml_form(log, 3, "subblock_duration: %u",
                               dmix_subblock->base.subblock_duration);
        log += write_yaml_form(log, 3, "dmixp_mode: %lu",
                               dmix_subblock->demixing_mode);
      } else if (param_block->base->type == ck_iamf_parameter_type_recon_gain) {
        recon_gain_parameter_subblock_t* recon_subblock =
            (recon_gain_parameter_subblock_t*)def_value_wrap_ptr(
                array_at(param_block->subblocks, i));
        log += write_yaml_form(log, 1, "- recon_gain_info_parameter_data:");
        if (recon_subblock->recon_gains) {
          int32_t recon_gain_count = array_size(recon_subblock->recon_gains);
          for (int32_t j = 0; j < recon_gain_count; ++j) {
            log += write_yaml_form(log, 3, "recon_gains_for_layer:");
            recon_gain_t* recon_gain = (recon_gain_t*)def_value_wrap_ptr(
                array_at(recon_subblock->recon_gains, j));
            if (!recon_gain) continue;
            int channels = bit1_count(recon_gain->recon_gain_flags);
            if (channels > 0) {
              uint8_t cur_channel = 0;
              uint32_t recon_channel = recon_gain->recon_gain_flags & 0xFFFF;
              if (recon_channel & 0x8000) {
                uint32_t ch = ((recon_channel & 0xFF00) >> 8);
                for (uint8_t k = 0; k < 7; ++k) {
                  if (ch & 0x01) {
                    log += write_yaml_form(log, 4, "recon_gain:");
                    log += write_yaml_form(log, 5, "key: %u", k);
                    log += write_yaml_form(
                        log, 5, "value: %d",
                        def_value_wrap_i32(
                            array_at(recon_gain->recon_gain, cur_channel++)));
                  }
                  ch >>= 1;
                }
                ch = (recon_channel & 0x00FF);
                for (uint8_t k = 7; k < 12; ++k) {
                  if (ch & 0x01) {
                    log += write_yaml_form(log, 4, "recon_gain:");
                    log += write_yaml_form(log, 5, "key: %u", k);
                    log += write_yaml_form(
                        log, 5, "value: %d",
                        def_value_wrap_i32(
                            array_at(recon_gain->recon_gain, cur_channel++)));
                  }
                  ch >>= 1;
                }
              } else {
                uint32_t ch = recon_channel & 0xFF;
                for (uint8_t k = 0; k < 7; ++k) {
                  if (ch & 0x01) {
                    log += write_yaml_form(log, 4, "recon_gain:");
                    log += write_yaml_form(log, 5, "key: %u", k);
                    log += write_yaml_form(
                        log, 5, "value: %d",
                        def_value_wrap_i32(
                            array_at(recon_gain->recon_gain, cur_channel++)));
                  }
                  ch >>= 1;
                }
              }
            }
          }
        }
      }
    }
  }

  write_postfix(LOG_OBU, log);
}

/**
 * @brief Writes IAMF audio frame OBU information to log
 *
 * This function logs the audio frame OBU data including substream ID, trim
 * information, and audio frame size in YAML format.
 *
 * @param idx The index/ID of the OBU being logged
 * @param obu Pointer to the IAMF audio frame OBU data
 * @param log Buffer to write the log output to
 * @param num_samples_to_trim_at_start Number of samples to trim from the start
 * @param num_samples_to_trim_at_end Number of samples to trim from the end
 *
 * @note This function performs input validation and will handle NULL pointers
 *       gracefully by writing an error message to the log.
 */
static void write_audio_frame_log(uint64_t idx, void* obu, char* log,
                                  uint64_t num_samples_to_trim_at_start,
                                  uint64_t num_samples_to_trim_at_end) {
  // Input validation
  if (!obu) {
    log += write_prefix(LOG_OBU, log);
    log += write_yaml_form(log, 0, "AudioFrameOBU_%u:", idx);
    log += write_yaml_form(log, 0, "- error: \"NULL OBU pointer\"");
    write_postfix(LOG_OBU, log);
    return;
  }

  if (!log) {
    return;  // Cannot log if buffer is NULL
  }

  // Cast to the correct structure type
  iamf_audio_frame_obu_t* audio_frame = (iamf_audio_frame_obu_t*)obu;

  // Write log header
  log += write_prefix(LOG_OBU, log);
  log += write_yaml_form(log, 0, "AudioFrameOBU_%u:", idx);

  // Write basic audio frame information
  log += write_yaml_form(log, 0, "- audio_substream_id: %u",
                         audio_frame->audio_substream_id);
  log += write_yaml_form(log, 1, "num_samples_to_trim_at_start: %u",
                         num_samples_to_trim_at_start);
  log += write_yaml_form(log, 1, "num_samples_to_trim_at_end: %u",
                         num_samples_to_trim_at_end);

  // Write audio frame size information
  if (audio_frame->audio_frame && audio_frame->audio_frame->data) {
    log += write_yaml_form(log, 1, "size_of_audio_frame: %u",
                           audio_frame->audio_frame->size);
  } else {
    log += write_yaml_form(log, 1, "size_of_audio_frame: 0");
  }

  // Write log footer
  write_postfix(LOG_OBU, log);
}

static void write_temporal_delimiter_block_log(uint64_t idx, void* obu,
                                               char* log) {
  log += write_prefix(LOG_OBU, log);
  log += write_yaml_form(log, 0, "TemporalDelimiterOBU_%u:", idx);
  write_postfix(LOG_OBU, log);
}

static void write_metadata_log(uint64_t idx, void* obu, char* log) {
  // Input validation
  if (!obu) {
    log += write_prefix(LOG_OBU, log);
    log += write_yaml_form(log, 0, "MetadataOBU_%u:", idx);
    log += write_yaml_form(log, 0, "- error: \"NULL OBU pointer\"");
    write_postfix(LOG_OBU, log);
    return;
  }
  if (!log) return;

  iamf_metadata_obu_t* metadata = (iamf_metadata_obu_t*)obu;

  log += write_prefix(LOG_OBU, log);
  log += write_yaml_form(log, 0, "MetadataObuOBU_%u:", idx);
  log +=
      write_yaml_form(log, 0, "- metadata type: %u", metadata->metadata_type);

  switch (metadata->metadata_type) {
    case ck_iamf_metadata_type_itut_t35: {
      metadata_itut_t35_t* itu_t35_metadata = (metadata_itut_t35_t*)metadata;
      log += write_yaml_form(log, 1, "metadata_itu_t_t35:");
      log += write_yaml_form(log, 2, "itu_t_t35_country_code: 0x%02X",
                             itu_t35_metadata->itu_t_t35_country_code);
      if (itu_t35_metadata->itu_t_t35_country_code == 0xFF) {
        log += write_yaml_form(
            log, 2, "itu_t_t35_country_code_extension_byte: %u",
            itu_t35_metadata->itu_t_t35_country_code_extension_byte);
      }
      break;
    }
    case ck_iamf_metadata_type_iamf_tags: {
      metadata_iamf_tags* tags_metadata = (metadata_iamf_tags*)metadata;
      log += write_yaml_form(log, 1, "metadata_iamf_tags:");
      if (tags_metadata->tags) {
        log += write_yaml_form(log, 2, "tags:");
        int32_t tag_count = array_size(tags_metadata->tags);
        for (int32_t i = 0; i < tag_count; ++i) {
          value_wrap_t* v = array_at(tags_metadata->tags, i);
          iamf_tag_t* tag = (iamf_tag_t*)v->ptr;
          if (tag) {
            log += write_yaml_form(log, 2, "- name: %s", tag->name);
            log += write_yaml_form(log, 3, "value: %s", tag->value);
          }
        }
      }
      break;
    }
    default:
      log += write_yaml_form(log, 1, "error: \"Unsupported metadata type: %u\"",
                             metadata->metadata_type);
      break;
  }

  write_postfix(LOG_OBU, log);
}

/**
 * @brief Main OBU logging function that dispatches to specific OBU type
 * handlers
 *
 * This function serves as the main entry point for logging IAMF OBU data. It
 * validates the log file state, determines the OBU type, and dispatches to the
 * appropriate specific logging function. It handles all supported OBU types
 * including sequence headers, codec configs, audio elements, mix presentations,
 * parameter blocks, and audio frames.
 *
 * @param obu_type The type of OBU to log (using iamf_obu_type_t enum values)
 * @param obu Pointer to the OBU data structure
 * @param num_samples_to_trim_at_start Number of samples to trim from start (for
 * audio frames)
 * @param num_samples_to_trim_at_end Number of samples to trim from end (for
 * audio frames)
 *
 * @return int Returns 0 on success, -1 if log file is not open or on error
 *
 * @note This function performs input validation and will handle unknown OBU
 * types gracefully by logging an appropriate message.
 */
int vlog_obu(uint32_t obu_type, void* obu,
             uint64_t num_samples_to_trim_at_start,
             uint64_t num_samples_to_trim_at_end) {
  // Validate log file state
  if (!is_vlog_file_open()) {
    return -1;
  }

  static uint64_t obu_count = 0;
  static char log[LOG_BUFFER_SIZE];
  uint64_t key;

  // Initialize log buffer
  log[0] = 0;
  key = obu_count;

  // Dispatch to appropriate logging function based on OBU type
  switch (obu_type) {
    case ck_iamf_obu_codec_config:
      write_codec_config_log(obu_count++, obu, log);
      break;

    case ck_iamf_obu_audio_element:
      write_audio_element_log(obu_count++, obu, log);
      break;

    case ck_iamf_obu_mix_presentation:
      write_mix_presentation_log(obu_count++, obu, log);
      break;

    case ck_iamf_obu_parameter_block:
      write_parameter_block_log(obu_count++, obu, log);
      break;

    case ck_iamf_obu_temporal_delimiter:
      write_temporal_delimiter_block_log(obu_count++, obu, log);
      break;

    case ck_iamf_obu_metadata:
      write_metadata_log(obu_count++, obu, log);
      break;

    case ck_iamf_obu_sequence_header:
      write_sequence_header_log(obu_count++, obu, log);
      break;

    default:
      // Handle audio frame OBU types (frame ID0-ID17)
      if (obu_type >= ck_iamf_obu_audio_frame &&
          obu_type <= ck_iamf_obu_audio_frame_id17) {
        write_audio_frame_log(obu_count++, obu, log,
                              num_samples_to_trim_at_start,
                              num_samples_to_trim_at_end);
      } else {
        // Handle unknown OBU types
        write_temporal_delimiter_block_log(obu_count++, obu, log);
        // Could add specific logging for unknown types here
      }
      break;
  }

  return vlog_print(LOG_OBU, key, log);
}
#endif  // SUPPORT_VERIFIER

int vlog_decop(char* decop_text) {
  if (!is_vlog_file_open()) return -1;

  static uint64_t decop_log_count = 0;
  static char log_b[LOG_BUFFER_SIZE];
  char* log = log_b;
  uint64_t key = decop_log_count++;

  log += write_prefix(LOG_DECOP, log);
  log += write_yaml_form(log, 0, "DECOP_%u:", decop_log_count);
  log += write_yaml_form(log, 0, "- text: %s", decop_text);
  write_postfix(LOG_DECOP, log);

  return vlog_print(LOG_DECOP, key, log_b);
}
