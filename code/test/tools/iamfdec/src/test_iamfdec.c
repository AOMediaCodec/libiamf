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

static void print_usage(char *argv[]) {
  fprintf(stderr, "Usage:\n");
  fprintf(stderr, "%s <options> <input file>\n", argv[0]);
  fprintf(stderr, "options:\n");
  fprintf(stderr, "-i[0-1]    0 : IAMF bitstream input.(default)\n");
  fprintf(stderr, "           1 : MP4 input.\n");
  fprintf(stderr,
          "-o[2-3]    2 : WAVE output, same path as binary.(default)\n");
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
  fprintf(stderr,
          "-m           : Generate a metadata file with the suffix .met .\n");
  fprintf(stderr, "-disable_limiter\n             : Disable peak limiter.\n");
  fprintf(stderr,
          "-profile [n] : Set IAMF profile (0=SIMPLE, 1=BASE, 2=BASE_ENHANCED, "
          "3=BASE_ADVANCED, 4=ADVANCED_1, 5=ADVANCED_2).\n");
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
      } else if (argv[args][1] == 'h') {
        print_usage(argv);
        return 0;
      } else if (argv[args][1] == 't') {
        if (!strcmp(argv[args], "-ts")) {
          das.st = strtoul(argv[++args], NULL, 10);
          fprintf(stdout, "Start time : %us\n", das.st);
        }
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
      } else if (!strcmp(argv[args], "-profile")) {
        int profile_val = atoi(argv[++args]);
        if (profile_val > IA_PROFILE_NONE &&
            profile_val <= IA_PROFILE_ADVANCED_2) {
          das.profile = (IA_Profile)profile_val;
          fprintf(stdout, "Profile set to %d\n", profile_val);
        } else {
          fprintf(stderr, "Invalid profile value %d\n", profile_val);
          return -1;
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

  if (output_mode == 1) {
    if (input_mode > 1) {
      fprintf(stderr, "ERROR: -o1 is only valid when input mode is 0 or 1.\n");
      return -1;
    }
  }

#if SUPPORT_VERIFIER
  if (das.flags & def_flag_vlog) vlog_file_open(vlog_file);
#endif
  if (target.type) {
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
