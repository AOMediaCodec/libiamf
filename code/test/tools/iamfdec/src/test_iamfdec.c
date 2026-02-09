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

/*
AOM-IAMF Standard Deliverable Status:
This software module is out of scope and not part of the IAMF Final Deliverable.
*/

/**
 * @file test_iamfdec.c
 * @brief IAMF decode test.
 * @version 2.0.0
 * @date Created 03/03/2023
 **/

#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "IAMF_decoder.h"
#include "dep_wavwriter.h"
#include "iamfdec_private.h"
#include "mp4iamfpar.h"

#ifndef SUPPORT_VERIFIER
#define SUPPORT_VERIFIER 0
#endif
#if SUPPORT_VERIFIER
#include "vlogging_tool_sr.h"
#endif

typedef struct Decoder {
  FILE *f;
  FILE *wav_f;

  IAMF_DecoderHandle dec;

  int channels;
  uint32_t rate;
  IA_Profile profile;
} decoder_t;

static int parse_element_gain_offsets(const char *ego_str,
                                      decoder_args_t *das) {
#if defined(_WIN32)
  char *str_copy = _strdup(ego_str);
#else
  char *str_copy = strdup(ego_str);
#endif
  char *token = NULL, *saveptr = NULL;
  char *colon_pos = NULL;

  if (!str_copy || !das) return -1;

  das->element_gain_count = 0;
#if defined(_WIN32)
  token = strtok_s(str_copy, ",", &saveptr);
#else
  token = strtok_r(str_copy, ",", &saveptr);
#endif
  while (token != NULL &&
         das->element_gain_count < def_max_element_gain_offsets) {
    colon_pos = strchr(token, ':');
    if (!colon_pos) {
      fprintf(stderr,
              "Error: Invalid format in -ego parameter. Expected id:gain "
              "format.\n");
      free(str_copy);
      return -1;
    }

    *colon_pos = '\0';
    colon_pos++;

    das->element_gain_offsets[das->element_gain_count].element_id =
        strtoul(token, NULL, 10);
    das->element_gain_offsets[das->element_gain_count].gain_offset =
        strtof(colon_pos, 0);
    das->element_gain_count++;

    fprintf(stdout, "Element gain offset: id=%u, gain=%.1f dB\n",
            das->element_gain_offsets[das->element_gain_count - 1].element_id,
            das->element_gain_offsets[das->element_gain_count - 1].gain_offset);
#if defined(_WIN32)
    token = strtok_s(NULL, ",", &saveptr);
#else
    token = strtok_r(NULL, ",", &saveptr);
#endif
  }

  free(str_copy);
  return 0;
}

static int apply_element_gain_offset(IAMF_DecoderHandle handle,
                                     IAMF_StreamInfo *info,
                                     decoder_args_t *das) {
  if (!handle || !info || !das || das->element_gain_count == 0) {
    return 0;
  }

  for (uint32_t idx = 0; idx < das->element_gain_count; idx++) {
    uint32_t element_id = das->element_gain_offsets[idx].element_id;
    float gain_offset = das->element_gain_offsets[idx].gain_offset;
    iamf_element_presentation_info_t *element = NULL;
    int element_found = 0;

    for (uint32_t i = 0; i < info->iamf_stream_info.mix_presentation_count;
         i++) {
      iamf_mix_presentation_info_t *mix =
          &info->iamf_stream_info.mix_presentations[i];
      for (uint32_t j = 0; j < mix->num_audio_elements; j++) {
        if (mix->elements[j].eid == element_id) {
          element = &mix->elements[j];
          element_found = 1;
          break;
        }
      }
      if (element_found) break;
    }

    if (!element_found) {
      fprintf(stderr, "Element ID %u not found in stream\n", element_id);
      continue;
    }

    if (!element->gain_offset_range) {
      fprintf(stderr, "Element ID %u does not support gain offset adjustment\n",
              element_id);
      continue;
    }

    if (gain_offset < element->gain_offset_range->min ||
        gain_offset > element->gain_offset_range->max) {
      fprintf(stderr,
              "Element gain offset %.1f dB is out of range [%.1f, %.1f] dB for "
              "element %u\n",
              gain_offset, element->gain_offset_range->min,
              element->gain_offset_range->max, element_id);
      continue;
    }

    IAMF_decoder_set_audio_element_gain_offset(handle, element_id, gain_offset);

    fprintf(stdout, "Successfully set element %u gain offset to %.1f dB\n",
            element_id, gain_offset);
  }

  return 0;
}

static void iamf_decoder_output_info(IAMF_DecoderHandle handle,
                                     decoder_args_t *das,
                                     IAMF_StreamInfo *stream_info);

static void print_usage(char *argv[]) {
  fprintf(stderr, "Usage:\n");
  fprintf(stderr, "%s <options> <input file>\n", argv[0]);
  fprintf(stderr, "options:\n");
  fprintf(stderr, "-i[0-1]    0 : IAMF bitstream input.(default)\n");
  fprintf(stderr, "           1 : MP4 input.\n");
  fprintf(stderr, "-o[1-3]    1 : Output IAMF stream info.\n");
  fprintf(stderr,
          "           2 : WAVE output, same path as binary.(default)\n");
  fprintf(stderr,
          "           3 [path]\n"
          "             : WAVE output, user setting path.\n");
  fprintf(
      stderr,
      "-r [rate]    : Audio signal sampling rate, 48000 is the default. \n");
#if SUPPORT_VERIFIER
  fprintf(stderr, "-v <file>    : verification log generation.\n");
#endif
  fprintf(stderr,
          "-s[0~14,b]   : Output layout, the sound system A~J and extensions "
          "(Upper + Middle + Bottom).\n");
  fprintf(stderr, "           0 : Sound system A (0+2+0)\n");
  fprintf(stderr, "           1 : Sound system B (0+5+0)\n");
  fprintf(stderr, "           2 : Sound system C (2+5+0)\n");
  fprintf(stderr, "           3 : Sound system D (4+5+0)\n");
  fprintf(stderr, "           4 : Sound system E (4+5+1)\n");
  fprintf(stderr, "           5 : Sound system F (3+7+0)\n");
  fprintf(stderr, "           6 : Sound system G (4+9+0)\n");
  fprintf(stderr, "           7 : Sound system H (9+10+3)\n");
  fprintf(stderr, "           8 : Sound system I (0+7+0)\n");
  fprintf(stderr, "           9 : Sound system J (4+7+0)\n");
  fprintf(stderr, "          10 : Sound system extension 712 (2+7+0)\n");
  fprintf(stderr, "          11 : Sound system extension 312 (2+3+0)\n");
  fprintf(stderr, "          12 : Sound system mono (0+1+0)\n");
  fprintf(stderr, "          13 : Sound system extension 916 (6+9+0)\n");
  fprintf(stderr, "          14 : Sound system extension 7154 (5+7+4)\n");
  fprintf(stderr, "           b : Binaural.\n");
  fprintf(stderr, "-p [dB]      : Peak threshold in dB.\n");
  fprintf(stderr, "-l [LKFS]    : Normalization loudness(<0) in LKFS.\n");
  fprintf(stderr, "-d [bits]    : Bit depth of pcm output.\n");
  fprintf(stderr, "-mp [id]     : Set mix presentation id.\n");
  fprintf(stderr, "-disable_limiter\n             : Disable peak limiter.\n");
  fprintf(stderr,
          "-profile [n] : Set IAMF profile (0=SIMPLE, 1=BASE, 2=BASE_ENHANCED, "
          "3=BASE_ADVANCED, 4=ADVANCED_1, 5=ADVANCED_2).\n");
  fprintf(stderr,
          "-ego id1:gain1,id2:gain2,...\n             : Set element gain "
          "offsets in dB for multiple audio elements.\n");
  fprintf(stderr, "-ht          : Enable head tracking.\n");
  fprintf(stderr, "-hr w,x,y,z  : Set head rotation quaternion.\n");
}

static uint32_t sound_system_layout_check(uint32_t ss) {
  return ss < SOUND_SYSTEM_END;
}

static int build_file_name(const char *path, layout_t *layout, char *n,
                           uint32_t size) {
  int ret = 0;
  const char *s = 0, *d;

  if (layout->type == 2) {
    snprintf(n, size, "ss%d_", layout->ss);
    ret = strlen(n);
  } else if (layout->type == 3) {
    snprintf(n, size, "binaural_");
    ret = strlen(n);
  } else {
    fprintf(stdout, "Invalid output layout type %d.\n", layout->type);
    return -1;
  }

#if defined(_WIN32)
  s = strrchr(path, '\\');
#else
  s = strrchr(path, '/');
#endif
  if (!s)
    s = path;
  else
    ++s;

  d = strrchr(path, '.');
  if (d) {
    int nn = d - s < size - ret - 1 ? d - s : size - ret - 1;
    strncpy(n + ret, s, nn);
    ret += nn;
  }
  n[ret] = 0;

  return ret;
}

static const char *sound_system_string(IAMF_SoundSystem ss) {
  static const char *sss[] = {
      "sound system A",    "sound system B",       "sound system C",
      "sound system D",    "sound system E",       "sound system F",
      "sound system G",    "sound system H",       "sound system I",
      "sound system J",    "sound system EXT 712", "sound system EXT 312",
      "sound system MONO", "sound system EXT 916", "sound system EXT 7154",
  };

  if (sound_system_layout_check(ss)) return sss[ss];
  return "Invalid sound system.";
}

#define def_path_length 128
#define def_fclose(f) \
  if (f) {            \
    fclose(f);        \
    f = 0;            \
  }

static int decoder_init(decoder_t *decoder, decoder_args_t *das) {
  int ret = 0;

  char out[def_path_length];

  memset(decoder, 0, sizeof(decoder_t));

  decoder->rate = das->rate;
  if (das->output_path) {
    ret = strlen(das->output_path);
    strncpy(out, das->output_path, def_path_length);
  } else {
    ret = build_file_name(das->path, das->layout, out, def_path_length - 4);
    if (ret < 0) return ret;
    snprintf(out + ret, def_path_length - ret, "%s", ".wav");
  }

  decoder->dec = IAMF_decoder_open();
  if (!decoder->dec) {
    fprintf(stderr, "IAMF decoder can't created.\n");
    return -1;
  }

  if (IAMF_decoder_set_profile(decoder->dec, das->profile) != IAMF_OK) {
    fprintf(stderr, "Failed to set profile %d, use default profile %d\n",
            das->profile, def_default_profile);
    IAMF_decoder_set_profile(decoder->dec, def_default_profile);
    das->profile = def_default_profile;
  }

  if (das->flags & def_flag_disable_limiter)
    IAMF_decoder_peak_limiter_enable(decoder->dec, 0);
  else
    IAMF_decoder_peak_limiter_set_threshold(decoder->dec, das->peak);
  IAMF_decoder_set_normalization_loudness(decoder->dec, das->loudness);
  IAMF_decoder_set_bit_depth(decoder->dec, das->bit_depth);

  if (decoder->rate > 0 &&
      IAMF_decoder_set_sampling_rate(decoder->dec, decoder->rate) != IAMF_OK) {
    fprintf(stderr, "Invalid sampling rate %u\n", decoder->rate);
    return -1;
  }

  if (das->layout->type == 2) {
    IAMF_decoder_output_layout_set_sound_system(decoder->dec, das->layout->ss);
    decoder->channels =
        IAMF_layout_sound_system_channels_count(das->layout->ss);
    fprintf(stdout, "%s has %d channels\n",
            sound_system_string(das->layout->ss), decoder->channels);
  } else {
    IAMF_decoder_output_layout_set_binaural(decoder->dec);
    decoder->channels = IAMF_layout_binaural_channels_count();
    fprintf(stdout, "Binaural has %d channels\n", decoder->channels);

    // Apply head tracking settings for binaural output
    if (das->flags & def_flag_enable_head_tracking) {
      IAMF_decoder_enable_head_tracking(decoder->dec, 1);
      fprintf(stdout, "Head tracking enabled for binaural output\n");

      // Set head rotation if provided (only effective when head tracking is
      // enabled)
      if (das->head_rotation_w != 0.0f || das->head_rotation_x != 0.0f ||
          das->head_rotation_y != 0.0f || das->head_rotation_z != 0.0f) {
        IAMF_decoder_set_head_rotation(
            decoder->dec, das->head_rotation_w, das->head_rotation_x,
            das->head_rotation_y, das->head_rotation_z);
        fprintf(stdout, "Head rotation set for binaural output\n");
      }
    }
  }

  decoder->f = fopen(das->path, "rb");
  if (!decoder->f) {
    fprintf(stderr, "%s can't opened.\n", das->path);
    return -1;
  }

  if (!decoder->rate) decoder->rate = def_default_sampling_rate;
  decoder->wav_f = (FILE *)dep_wav_write_open(
      out, decoder->rate, das->bit_depth, decoder->channels);
  if (!decoder->wav_f) {
    fprintf(stderr, "%s can't opened.\n", out);
    return -1;
  }

  return 0;
}

static void decoder_cleanup(decoder_t *decoder) {
  def_fclose(decoder->f);

  if (decoder->wav_f) dep_wav_write_close(decoder->wav_f);
  if (decoder->dec) IAMF_decoder_close(decoder->dec);
}

static int bs_input_wav_output(decoder_args_t *das) {
  decoder_t decoder;
  uint8_t block[def_block_size];
  int used = 0, end = 0;
  int ret = 0;
  int state = 0;
  uint32_t rsize = 0;
  void *pcm = NULL;
  int count = 0, samples = 0;
  uint64_t frsize = 0, fsize = 0;
  uint32_t size;

  if (!das->path) return -1;

  ret = decoder_init(&decoder, das);
  if (ret < 0) goto end;

  fseek(decoder.f, 0L, SEEK_END);
  fsize = ftell(decoder.f);
  fseek(decoder.f, 0L, SEEK_SET);

  do {
    ret = 0;
    if (def_block_size != used) {
      ret = fread(block + used, 1, def_block_size - used, decoder.f);
      if (ret < 0) {
        fprintf(stderr, "file read error : %d (%s).\n", ret, strerror(ret));
        break;
      }
      if (!ret) end = 1;
    }

    frsize += ret;
    // fprintf(stdout, "Read FILE ========== read %d and count %" PRIu64"\n",
    // ret, frsize);
    size = used + ret;
    used = 0;
    if (state <= 0) {
      if (end) break;
      rsize = 0;
      if (das->mix_presentation_id != def_default_mix_presentation_id)
        IAMF_decoder_set_mix_presentation_id(decoder.dec,
                                             das->mix_presentation_id);
      ret = IAMF_decoder_configure(decoder.dec, block + used, size - used,
                                   &rsize);
      if (ret == IAMF_OK) {
        state = 1;
        IAMF_StreamInfo *info = IAMF_decoder_get_stream_info(decoder.dec);
        if (apply_element_gain_offset(decoder.dec, info, das) != 0) {
          fprintf(stderr, "Failed to apply element gain offset\n");
          if (info) IAMF_decoder_free_stream_info(info);
          ret = -1;
          break;
        }

        iamf_decoder_output_info(decoder.dec, das, info);

        if (!pcm)
          pcm = (void *)malloc(das->bit_depth / 8 * info->max_frame_size *
                               decoder.channels);
        if (info) {
          IAMF_decoder_free_stream_info(info);
          info = 0;
        }
      } else if (ret != IAMF_ERR_BUFFER_TOO_SMALL) {
        fprintf(stderr, "errno: %d, fail to configure decoder.\n", ret);
        break;
      } else if (!rsize) {
        fprintf(stderr, "errno: %d, buffer is too small.\n", ret);
        break;
      }
      /* fprintf(stdout, "header length %d, ret %d\n", rsize, ret); */
      used += rsize;
    }
    if (state > 0) {
      while (1) {
        rsize = 0;
        if (!end)
          ret = IAMF_decoder_decode(decoder.dec, block + used, size - used,
                                    &rsize, pcm);
        else
          ret = IAMF_decoder_decode(decoder.dec, (const uint8_t *)NULL, 0,
                                    &rsize, pcm);

        /* fprintf(stdout, "read packet size %d\n", rsize); */
        if (ret > 0) {
          ++count;
          samples += ret;
          /* fprintf(stderr, "===================== Get %d frame and size %d\n",
           * count, ret); */
          dep_wav_write_data(decoder.wav_f, (unsigned char *)pcm,
                             (das->bit_depth / 8) * ret * decoder.channels);
        }

        if (end) break;

        used += rsize;

        if (ret == IAMF_ERR_INVALID_STATE) {
          state = ret;
          printf("state change to invalid, need reconfigure.\n");
        }

        if (ret < 0 || used >= size || !rsize) {
          break;
        }
      }
    }

    if (end) break;

    memmove(block, block + used, size - used);
    used = size - used;

  } while (1);

end:
  fprintf(stderr, "===================== Get %d frames\n", count);
  fprintf(stderr, "===================== Get %d samples\n", samples);

  if (fsize != frsize)
    fprintf(stderr,
            "file is read %" PRIu64 " (vs %" PRIu64
            "), not completely. return value %d\n",
            frsize, fsize, ret);

  if (pcm) free(pcm);
  decoder_cleanup(&decoder);

  return ret;
}

static const char *profile_string(IA_Profile profile) {
  switch (profile) {
    case IA_PROFILE_SIMPLE:
      return "SIMPLE";
    case IA_PROFILE_BASE:
      return "BASE";
    case IA_PROFILE_BASE_ENHANCED:
      return "BASE_ENHANCED";
    case IA_PROFILE_BASE_ADVANCED:
      return "BASE_ADVANCED";
    case IA_PROFILE_ADVANCED_1:
      return "ADVANCED_1";
    case IA_PROFILE_ADVANCED_2:
      return "ADVANCED_2";
    default:
      return "UNKNOWN";
  }
}

static const char *codec_string(IAMF_CodecID codec_id) {
  switch (codec_id) {
    case IAMF_CODEC_OPUS:
      return "Opus";
    case IAMF_CODEC_AAC:
      return "AAC";
    case IAMF_CODEC_FLAC:
      return "FLAC";
    case IAMF_CODEC_PCM:
      return "PCM";
    default:
      return "UNKNOWN";
  }
}

static const char *headphones_rendering_mode_string(
    IAMF_HeadphonesRenderingMode mode) {
  switch (mode) {
    case HEADPHONES_RENDERING_MODE_WORLD_LOCKED_RESTRICTED:
      return "WORLD_LOCKED_RESTRICTED";
    case HEADPHONES_RENDERING_MODE_WORLD_LOCKED:
      return "WORLD_LOCKED";
    case HEADPHONES_RENDERING_MODE_HEAD_LOCKED:
      return "HEAD_LOCKED";
    case HEADPHONES_RENDERING_MODE_RESERVED:
      return "RESERVED";
    default:
      return "UNKNOWN";
  }
}

static const char *binaural_filter_profile_string(
    IAMF_BinauralFilterProfile profile) {
  switch (profile) {
    case BINAURAL_FILTER_PROFILE_AMBIENT:
      return "AMBIENT";
    case BINAURAL_FILTER_PROFILE_DIRECT:
      return "DIRECT";
    case BINAURAL_FILTER_PROFILE_REVERBERANT:
      return "REVERBERANT";
    case BINAURAL_FILTER_PROFILE_RESERVED:
      return "RESERVED";
    default:
      return "UNKNOWN";
  }
}

void iamf_decoder_output_info(IAMF_DecoderHandle handle, decoder_args_t *das,
                              IAMF_StreamInfo *stream_info) {
  if (!handle) {
    fprintf(stdout, "Invalid decoder handle.\n");
    return;
  }

  IA_Profile current_profile = IAMF_decoder_get_profile(handle);
  fprintf(stdout, "Current IAMF Profile: %d (%s)\n", current_profile,
          profile_string(current_profile));

  int64_t mix_presentation_id = IAMF_decoder_get_mix_presentation_id(handle);
  if (mix_presentation_id >= 0) {
    fprintf(stdout, "Selected Mix Presentation ID: %" PRId64 "\n",
            mix_presentation_id);
  } else {
    fprintf(stdout, "Selected Mix Presentation ID: None (default)\n");
  }

  if (!(das->flags & def_flag_disable_limiter)) {
    float limiter_threshold = IAMF_decoder_peak_limiter_get_threshold(handle);
    fprintf(stdout, "Peak Limiter Threshold: %.2f dB\n", limiter_threshold);
  }

  if (stream_info && mix_presentation_id >= 0) {
    iamf_mix_presentation_info_t *target_mix = NULL;
    for (uint32_t i = 0;
         i < stream_info->iamf_stream_info.mix_presentation_count; i++) {
      if (stream_info->iamf_stream_info.mix_presentations[i].id ==
          (uint32_t)mix_presentation_id) {
        target_mix = &stream_info->iamf_stream_info.mix_presentations[i];
        break;
      }
    }

    if (target_mix) {
      for (uint32_t j = 0; j < target_mix->num_audio_elements; j++) {
        iamf_element_presentation_info_t *element = &target_mix->elements[j];
        float current_gain = 0.0f;

        if (!element->gain_offset_range) continue;
        if (IAMF_decoder_get_audio_element_gain_offset(
                handle, element->eid, &current_gain) == IAMF_OK) {
          fprintf(stdout, "  Element %u: Current gain offset = %.2f dB",
                  element->eid, current_gain);

          if (element->gain_offset_range) {
            fprintf(stdout, " (range: %.1f to %.1f dB)",
                    element->gain_offset_range->min,
                    element->gain_offset_range->max);
          }
          fprintf(stdout, "\n");
        }
      }
    }
  }
}

static void print_complete_stream_info(IAMF_StreamInfo *info) {
  if (!info) {
    fprintf(stdout, "No stream information available.\n");
    return;
  }

  fprintf(stdout, "\n=== IAMF Stream Information ===\n");
  fprintf(stdout, "Maximum frame size: %u samples\n", info->max_frame_size);

  fprintf(stdout, "\n--- Profile Information ---\n");
  fprintf(stdout, "Primary profile: %d (%s)\n",
          info->iamf_stream_info.primary_profile,
          profile_string(info->iamf_stream_info.primary_profile));
  fprintf(stdout, "Additional profile: %d (%s)\n",
          info->iamf_stream_info.additional_profile,
          profile_string(info->iamf_stream_info.additional_profile));

  fprintf(stdout, "\n--- Codec Information ---\n");
  for (int i = 0;
       i < IAMF_MAX_CODECS && info->iamf_stream_info.codec_ids[i] != 0; i++) {
    fprintf(stdout, "Codec %d: %d (%s)\n", i,
            info->iamf_stream_info.codec_ids[i],
            codec_string(info->iamf_stream_info.codec_ids[i]));
  }

  fprintf(stdout, "\n--- Audio Parameters ---\n");
  fprintf(stdout, "Sampling rate: %u Hz\n",
          info->iamf_stream_info.sampling_rate);
  fprintf(stdout, "Samples per channel per frame: %u\n",
          info->iamf_stream_info.samples_per_channel_in_frame);

  fprintf(stdout, "\n--- Mix Presentations ---\n");
  fprintf(stdout, "Number of mix presentations: %u\n",
          info->iamf_stream_info.mix_presentation_count);

  if (info->iamf_stream_info.mix_presentations) {
    for (uint32_t i = 0; i < info->iamf_stream_info.mix_presentation_count;
         i++) {
      iamf_mix_presentation_info_t *mix =
          &info->iamf_stream_info.mix_presentations[i];
      fprintf(stdout, "\n  Mix Presentation %u:\n", mix->id);
      fprintf(stdout, "    Number of audio elements: %u\n",
              mix->num_audio_elements);

      if (mix->elements) {
        for (uint32_t j = 0; j < mix->num_audio_elements; j++) {
          iamf_element_presentation_info_t *elem = &mix->elements[j];
          fprintf(stdout, "    Audio Element %u:\n", elem->eid);
          fprintf(stdout, "      Headphones rendering mode: %d (%s)\n",
                  elem->mode, headphones_rendering_mode_string(elem->mode));
          fprintf(stdout, "      Binaural filter profile: %d (%s)\n",
                  elem->profile, binaural_filter_profile_string(elem->profile));
          if (elem->gain_offset_range) {
            fprintf(stdout, "      Gain offset range: %.1f dB to %.1f dB\n",
                    elem->gain_offset_range->min, elem->gain_offset_range->max);
          }
        }
      }
    }
  }

  fprintf(stdout, "=== End of Stream Information ===\n\n");
}

static int stream_info_output(decoder_args_t *das, int input_mode) {
  IAMF_DecoderHandle dec;
  IAMF_StreamInfo *info = NULL;
  int ret = 0;

  // Initialize decoder and set profile
  dec = IAMF_decoder_open();
  if (!dec) {
    fprintf(stderr, "IAMF decoder can't created.\n");
    return -1;
  }

  IAMF_decoder_output_layout_set_sound_system(dec, SOUND_SYSTEM_A);

  if (input_mode == 0) {
    decoder_t decoder;
    uint8_t block[def_block_size];
    int used = 0, end = 0, state = 0;
    uint32_t rsize = 0, size;

    memset(&decoder, 0, sizeof(decoder_t));
    decoder.dec = dec;
    decoder.f = fopen(das->path, "rb");
    if (!decoder.f) {
      fprintf(stderr, "%s can't opened.\n", das->path);
      IAMF_decoder_close(dec);
      return -1;
    }

    do {
      ret = 0;
      if (def_block_size != used) {
        ret = fread(block + used, 1, def_block_size - used, decoder.f);
        if (ret < 0) {
          fprintf(stderr, "file read error : %d (%s).\n", ret, strerror(ret));
          break;
        }
        if (!ret) end = 1;
      }

      size = used + ret;
      used = 0;
      if (state <= 0) {
        if (end) break;
        rsize = 0;
        ret = IAMF_decoder_configure(dec, block + used, size - used, &rsize);
        if (ret == IAMF_OK) {
          info = IAMF_decoder_get_stream_info(dec);
          break;
        } else if (ret != IAMF_ERR_BUFFER_TOO_SMALL) {
          fprintf(stderr, "errno: %d, fail to configure decoder.\n", ret);
          break;
        } else if (!rsize) {
          fprintf(stderr, "errno: %d, buffer is too small.\n", ret);
          break;
        }
        used += rsize;
      }

      if (end) break;
      memmove(block, block + used, size - used);
      used = size - used;
    } while (1);

    fclose(decoder.f);
  } else {
    // MP4 input
    MP4IAMFParser mp4par;
    IAMFHeader *header = 0;
    uint8_t *block = 0;
    uint32_t size = 0;

    memset(&mp4par, 0, sizeof(mp4par));
    mp4_iamf_parser_init(&mp4par);
    mp4_iamf_parser_set_logger(&mp4par, 0);
    ret = mp4_iamf_parser_open_audio_track(&mp4par, das->path, &header);

    if (ret <= 0) {
      fprintf(stderr, "mp4iamfpar can not open mp4 file(%s)\n", das->path);
      goto mp4_end;
    }

    ret = iamf_header_read_description_OBUs(header, &block, &size);
    if (!ret) {
      fprintf(stderr, "fail to copy description obu.\n");
      goto mp4_end;
    }

    ret = IAMF_decoder_configure(dec, block, ret, 0);
    if (ret != IAMF_OK) {
      fprintf(stderr, "fail to configure.\n");
      goto mp4_end;
    }

    info = IAMF_decoder_get_stream_info(dec);

  mp4_end:
    if (block) free(block);
    mp4_iamf_parser_close(&mp4par);
  }

  if (info) {
    print_complete_stream_info(info);
    IAMF_decoder_free_stream_info(info);
  } else {
    fprintf(stderr, "Failed to get stream info.\n");
  }

  IAMF_decoder_close(dec);
  return ret;
}

static int mp4_input_wav_output2(decoder_args_t *das) {
  MP4IAMFParser mp4par;
  IAMFHeader *header = 0;
  decoder_t decoder;
  uint8_t *block = 0;
  uint32_t size = 0;
  int end = 0;
  int ret = 0;
  void *pcm = NULL;
  int count = 0, samples = 0;
  int entno = 0;
  int64_t sample_offs;

  if (!das->path) return -1;

  ret = decoder_init(&decoder, das);
  if (ret < 0) goto end;

  memset(&mp4par, 0, sizeof(mp4par));
  mp4_iamf_parser_init(&mp4par);
  mp4_iamf_parser_set_logger(&mp4par, 0);
  ret = mp4_iamf_parser_open_audio_track(&mp4par, das->path, &header);

  if (ret <= 0) {
    fprintf(stderr, "mp4opusdemuxer can not open mp4 file(%s)\n", das->path);
    goto end;
  }

  ret = iamf_header_read_description_OBUs(header, &block, &size);
  if (!ret) {
    fprintf(stderr, "fail to copy description obu.\n");
    goto end;
  }

  if (das->mix_presentation_id != def_default_mix_presentation_id)
    IAMF_decoder_set_mix_presentation_id(decoder.dec, das->mix_presentation_id);
  ret = IAMF_decoder_configure(decoder.dec, block, ret, 0);
  if (ret != IAMF_OK) {
    fprintf(stderr, "fail to configure.\n");
    goto end;
  }

  IAMF_StreamInfo *info = IAMF_decoder_get_stream_info(decoder.dec);
  if (apply_element_gain_offset(decoder.dec, info, das) != 0) {
    fprintf(stderr, "Failed to apply element gain offset\n");
    if (info) IAMF_decoder_free_stream_info(info);
    ret = -1;
    goto end;
  }

  iamf_decoder_output_info(decoder.dec, das, info);

  pcm = (void *)malloc(das->bit_depth / 8 * info->max_frame_size *
                       decoder.channels);
  if (!pcm) {
    ret = errno;
    fprintf(stderr, "error no(%d):fail to malloc memory for pcm.", ret);
    goto end;
  }
  if (block) free(block);
  if (ret != IAMF_OK) {
    fprintf(stderr, "errno: %d, fail to configure decoder.\n", ret);
    goto end;
  }

  if (info) {
    IAMF_decoder_free_stream_info(info);
    info = 0;
  }

  do {
    if (mp4_iamf_parser_read_packet(&mp4par, 0, &block, &size, &sample_offs,
                                    &entno) < 0) {
      end = 1;
    }

    if (!end)
      ret = IAMF_decoder_decode(decoder.dec, block, size, 0, pcm);
    else
      ret = IAMF_decoder_decode(decoder.dec, (const uint8_t *)NULL, 0, 0, pcm);

    if (block) free(block);
    block = 0;
    if (ret > 0) {
      ++count;
      samples += ret;
      /* fprintf(stderr, */
      /* "===================== Get %d frame and size %d, offset %" PRId64"\n",
       */
      /* count, ret, sample_offs); */
      dep_wav_write_data(decoder.wav_f, (unsigned char *)pcm,
                         (das->bit_depth / 8) * ret * decoder.channels);
    }
    if (end) break;
  } while (1);
end:
  fprintf(stderr, "===================== Get %d frames\n", count);
  fprintf(stderr, "===================== Get %d samples\n", samples);

  if (block) free(block);
  if (pcm) free(pcm);
  mp4_iamf_parser_close(&mp4par);
  decoder_cleanup(&decoder);

  return ret;
}

int main(int argc, char *argv[]) {
  int args;
  int output_mode = 2;
  int input_mode = 0;
  int sound_system = -1;
  char *f = 0;
  layout_t target;
  decoder_args_t das;

  memset(&das, 0, sizeof(das));
  das.layout = &target;
  das.peak = -1.f;
  das.loudness = .0f;
  das.bit_depth = 16;
  das.mix_presentation_id = -1;
  das.rate = def_default_sampling_rate;
  das.profile = def_default_profile;

  if (argc < 2) {
    print_usage(argv);
    return -1;
  }

#if SUPPORT_VERIFIER
  char *vlog_file = 0;
#endif

  memset(&target, 0, sizeof(layout_t));
  args = 1;
  fprintf(stdout, "IN ARGS:\n");
  while (args < argc) {
    if (argv[args][0] == '-') {
      if (argv[args][1] == 'o') {
        output_mode = atoi(argv[args] + 2);
        fprintf(stdout, "Output mode %d\n", output_mode);
        if (output_mode == 3) {
          das.output_path = argv[++args];
        }
      } else if (argv[args][1] == 'i') {
        input_mode = atoi(argv[args] + 2);
        fprintf(stdout, "Input mode %d\n", input_mode);
      } else if (argv[args][1] == 's') {
        if (argv[args][2] == 'b') {
          target.type = 3;
          fprintf(stdout, "Output binaural\n");
        } else {
          sound_system = atoi(argv[args] + 2);
          fprintf(stdout, "Output sound system %d\n", sound_system);
          if (sound_system_layout_check(sound_system)) {
            target.type = 2;
            target.ss = sound_system;
          } else {
            fprintf(stderr, "Invalid output layout of sound system %d\n",
                    sound_system);
          }
        }
      } else if (!strcmp(argv[args], "-p")) {
        das.peak = strtof(argv[++args], 0);
        fprintf(stdout, "Peak threshold value : %g db\n", das.peak);
      } else if (!strcmp(argv[args], "-l")) {
        das.loudness = strtof(argv[++args], 0);
        fprintf(stdout, "Normalization loudness value : %g LKFS\n",
                das.loudness);
      } else if (!strcmp(argv[args], "-d")) {
        das.bit_depth = strtof(argv[++args], 0);
        fprintf(stdout, "Bit depth of pcm output : %u bit\n", das.bit_depth);
      } else if (argv[args][1] == 'v') {
#if SUPPORT_VERIFIER
        das.flags |= def_flag_vlog;
        vlog_file = argv[++args];
        fprintf(stdout, "Verification log file : %s\n", vlog_file);
#endif
      } else if (!strcmp(argv[args], "-h")) {
        print_usage(argv);
        return 0;
      } else if (!strcmp(argv[args], "-ht")) {
        das.flags |= def_flag_enable_head_tracking;
        fprintf(stdout, "Head tracking enabled\n");
      } else if (!strcmp(argv[args], "-r")) {
        das.rate = strtoul(argv[++args], NULL, 10);
        fprintf(stdout, "sampling rate : %u\n", das.rate);
      } else if (!strcmp(argv[args], "-mp")) {
        das.mix_presentation_id = strtoull(argv[++args], NULL, 10);
        fprintf(stdout, "select mix presentation id %" PRId64 "\n",
                das.mix_presentation_id);
      } else if (!strcmp(argv[args], "-disable_limiter")) {
        das.flags |= def_flag_disable_limiter;
        fprintf(stdout, "Disable peak limiter\n");
      } else if (!strcmp(argv[args], "-ego")) {
        parse_element_gain_offsets(argv[++args], &das);
      } else if (!strcmp(argv[args], "-hr")) {
        char *rotation_str = argv[++args];
        sscanf(rotation_str, "%f,%f,%f,%f", &das.head_rotation_w,
               &das.head_rotation_x, &das.head_rotation_y,
               &das.head_rotation_z);
        fprintf(stdout, "Head rotation: w=%f, x=%f, y=%f, z=%f\n",
                das.head_rotation_w, das.head_rotation_x, das.head_rotation_y,
                das.head_rotation_z);
      } else if (!strcmp(argv[args], "-profile")) {
        int profile_val = atoi(argv[++args]);
        if (profile_val > IA_PROFILE_NONE &&
            profile_val <= IA_PROFILE_ADVANCED_2) {
          das.profile = (IA_Profile)profile_val;
          fprintf(stdout, "Profile set to %d\n", profile_val);
        } else {
          fprintf(stderr, "Invalid profile value %d\n", profile_val);
        }
      }
    } else {
      f = argv[args];
    }
    args++;
  }

  if (!f) {
    fprintf(stderr, "Please input file.\n");
    print_usage(argv);
    return -1;
  } else {
    fprintf(stdout, "file : %s\n", f);
    das.path = f;
  }

  if (input_mode != 1 && das.st) {
    fprintf(stderr, "ERROR: -st is valid when mp4 file is used as input.\n");
    print_usage(argv);
    return -1;
  }

  if (!output_mode || output_mode > 3) {
    fprintf(stderr, "invalid output mode %d\n", output_mode);
    print_usage(argv);
    return -1;
  }

  char *supported_codecs = IAMF_decoder_get_codec_capability();
  if (supported_codecs) {
    fprintf(stdout, "codec capability: %s\n", supported_codecs);
    free(supported_codecs);
  }

  if (output_mode == 1 && input_mode > 1) {
    fprintf(stderr, "ERROR: -o1 is only valid when input mode is 0 or 1.\n");
    return -1;
  }

#if SUPPORT_VERIFIER
  if (das.flags & def_flag_vlog) vlog_file_open(vlog_file);
#endif

  if (output_mode == 1) {
    stream_info_output(&das, input_mode);
  } else if (target.type) {
    // WAVE output modes (2 and 3)
    if (!input_mode) {
      bs_input_wav_output(&das);
    } else if (input_mode == 1) {
      mp4_input_wav_output2(&das);
    } else {
      fprintf(stderr, "invalid input mode %d\n", input_mode);
    }
  } else {
    print_usage(argv);
    fprintf(stderr, "invalid output sound system %d\n", sound_system);
  }
#if SUPPORT_VERIFIER
  if (das.flags & def_flag_vlog) vlog_file_close();
#endif
  return 0;
}
