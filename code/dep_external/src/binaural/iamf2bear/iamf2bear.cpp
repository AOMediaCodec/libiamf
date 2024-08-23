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
 * @file bear.cpp
 * @brief API implementation for IAMF decoder to BEAR renderer Interface.
 * @version 0.1
 * @date Created 03/03/2023
 **/
#define _CRT_SECURE_NO_WARNINGS
#define DLL_EXPORTS

#if defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
#endif
#include <memory.h>
#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif
#include <stdio.h>

#include "bear/api.hpp"
#include "bear/variable_block_size.hpp"
#include "ear/ear.hpp"
#include "iamf_bear_api.h"
using namespace bear;
using namespace ear;

#if defined(_WIN32)
// EXPORT_API can be used to define the dllimport storage-class attribute.
#ifdef DLL_EXPORTS
#define EXPORT_API __declspec(dllexport)
#else
#define EXPORT_API __declspec(dllimport)
#endif
#else
#define EXPORT_API
#endif

// max input audio element number is 28
#define MAX_INPUT_SOURCES 28
// max total number of input channels is number channels of IAMF_916 source
#define MAX_INPUT_CHANNELS 16
#define MAX_PATH 260

struct BearAPIImplement {
  Config *m_pConfig[MAX_INPUT_SOURCES];
  Renderer *m_pRenderer[MAX_INPUT_SOURCES];
  DirectSpeakersInput *m_pDirectSpeakersInput[MAX_INPUT_SOURCES];
  float **in[MAX_INPUT_SOURCES];
  char tf_data[MAX_PATH];
};

typedef enum {
  BS2051_A = 0x020,        // 2ch output
  BS2051_B = 0x050,        // 6ch output
  BS2051_C = 0x250,        // 8ch output
  BS2051_D = 0x450,        // 10ch output
  BS2051_E = 0x451,        // 11ch output
  BS2051_F = 0x370,        // 12ch output
  BS2051_G = 0x490,        // 14ch output
  BS2051_H = 0x9A3,        // 24ch output
  BS2051_I = 0x070,        // 8ch output
  BS2051_J = 0x470,        // 12ch output
  IAMF_STEREO = 0x200,     // 2ch input
  IAMF_51 = 0x510,         // 6ch input
  IAMF_512 = 0x512,        // 8ch input
  IAMF_514 = 0x514,        // 10ch input
  IAMF_71 = 0x710,         // 8ch input
  IAMF_714 = 0x714,        // 12ch input
  IAMF_MONO = 0x100,       // 1ch input, AOM only
  IAMF_712 = 0x712,        // 10ch input, AOM only
  IAMF_312 = 0x312,        // 6ch input, AOM only
  IAMF_BINAURAL = 0x1020,  // binaural input/output AOM only
  IAMF_916 = 0x916         // 16ch input/output, AOM only
} IAMF_SOUND_SYSTEM;

typedef enum {
  SP_MP000 = 0x00000001,
  SP_MP030 = 0x00000002,
  SP_MM030 = 0x00000004,
  SP_MP045 = 0x00000008,
  SP_MM045 = 0x00000010,
  SP_MP060 = 0x00000020,
  SP_MM060 = 0x00000040,
  SP_MP090 = 0x00000080,
  SP_MM090 = 0x00000100,
  SP_MP110 = 0x00000200,
  SP_MM110 = 0x00000400,
  SP_MP135 = 0x00000800,
  SP_MM135 = 0x00001000,
  SP_MP180 = 0x00002000,
  SP_TP000 = 0x00004000,
  SP_UP000 = 0x00008000,
  SP_UP030 = 0x00010000,
  SP_UM030 = 0x00020000,
  SP_UP045 = 0x00040000,
  SP_UM045 = 0x00080000,
  SP_UP090 = 0x00100000,
  SP_UM090 = 0x00200000,
  SP_UP110 = 0x00400000,
  SP_UM110 = 0x00800000,
  SP_UP135 = 0x01000000,
  SP_UM135 = 0x02000000,
  SP_UP180 = 0x04000000,
  SP_BP000 = 0x08000000,
  SP_BP045 = 0x10000000,
  SP_BM045 = 0x20000000,
  SP_LFE1 = 0x40000000,
  SP_LFE2 = 0x80000000
} BS2051_SPLABEL;

BS2051_SPLABEL iamf_stereo_spl[] = {SP_MP030, SP_MM030};
BS2051_SPLABEL iamf_51_spl[] = {SP_MP030, SP_MM030, SP_MP000,
                                SP_LFE1,  SP_MP110, SP_MM110};
BS2051_SPLABEL iamf_512_spl[] = {SP_MP030, SP_MM030, SP_MP000, SP_LFE1,
                                 SP_MP110, SP_MM110, SP_UP030, SP_UM030};
BS2051_SPLABEL iamf_514_spl[] = {SP_MP030, SP_MM030, SP_MP000, SP_LFE1,
                                 SP_MP110, SP_MM110, SP_UP030, SP_UM030,
                                 SP_UP110, SP_UM110};
BS2051_SPLABEL iamf_71_spl[] = {SP_MP030, SP_MM030, SP_MP000, SP_LFE1,
                                SP_MP090, SP_MM090, SP_MP135, SP_MM135};
BS2051_SPLABEL iamf_714_spl[] = {SP_MP030, SP_MM030, SP_MP000, SP_LFE1,
                                 SP_MP090, SP_MM090, SP_MP135, SP_MM135,
                                 SP_UP045, SP_UM045, SP_UP135, SP_UM135};
BS2051_SPLABEL iamf_mono_spl[] = {SP_MP000};
BS2051_SPLABEL iamf_712_spl[] = {SP_MP030, SP_MM030, SP_MP000, SP_LFE1,
                                 SP_MP090, SP_MM090, SP_MP135, SP_MM135,
                                 SP_UP045, SP_UM045};
BS2051_SPLABEL iamf_312_spl[] = {SP_MP030, SP_MM030, SP_MP000,
                                 SP_LFE1,  SP_UP030, SP_UM030};
BS2051_SPLABEL iamf_916_spl[] = {SP_MP060, SP_MM060, SP_MP000, SP_LFE1,
                                 SP_MP135, SP_MM135, SP_MP030, SP_MM030,
                                 SP_MP090, SP_MM090, SP_UP045, SP_UM045,
                                 SP_UP135, SP_UM135, SP_UP090, SP_UM090};

struct iamf_splabel_per_layout_t {
  IAMF_SOUND_SYSTEM system;
  int channels;
  BS2051_SPLABEL *sp_labels;
} iamf_splabel_per_layout[] = {
    {IAMF_STEREO, 2, iamf_stereo_spl}, {IAMF_51, 6, iamf_51_spl},
    {IAMF_512, 8, iamf_512_spl},       {IAMF_514, 10, iamf_514_spl},
    {IAMF_71, 8, iamf_71_spl},         {IAMF_714, 12, iamf_714_spl},
    {IAMF_MONO, 1, iamf_mono_spl},     {IAMF_712, 10, iamf_712_spl},
    {IAMF_312, 6, iamf_312_spl},       {IAMF_BINAURAL, 2, iamf_stereo_spl},
    {IAMF_916, 16, iamf_916_spl}};

Layout layout010 = Layout{
    "0+1+0",
    std::vector<Channel>{
        Channel{"M+000",
                PolarPosition{0.0, 0.0},
                PolarPosition{0.0, 0.0},
                std::make_pair(0.0, 0.0),
                std::make_pair(0.0, 0.0),
                false},
    }};

Layout layout230 = Layout{
    "2+3+0",
    std::vector<Channel>{
        Channel{"M+030",
                PolarPosition{30.0, 0.0},
                PolarPosition{30.0, 0.0},
                std::make_pair(30.0, 30.0),
                std::make_pair(0.0, 0.0),
                false},
        Channel{"M-030",
                PolarPosition{-30.0, 0.0},
                PolarPosition{-30.0, 0.0},
                std::make_pair(-30.0, -30.0),
                std::make_pair(0.0, 0.0),
                false},
        Channel{"M+000",
                PolarPosition{0.0, 0.0},
                PolarPosition{0.0, 0.0},
                std::make_pair(0.0, 0.0),
                std::make_pair(0.0, 0.0),
                false},
        Channel{"LFE1",
                PolarPosition{45.0, -30.0},
                PolarPosition{45.0, -30.0},
                std::make_pair(-180.0, 180.0),
                std::make_pair(-90.0, 90.0),
                true},
        Channel{"U+030",
                PolarPosition{30.0, 30.0},
                PolarPosition{30.0, 30.0},
                std::make_pair(30.0, 45.0),
                std::make_pair(30.0, 55.0),
                false},
        Channel{"U-030",
                PolarPosition{-30.0, 30.0},
                PolarPosition{-30.0, 30.0},
                std::make_pair(-45.0, -30.0),
                std::make_pair(30.0, 55.0),
                false},
    }};

Layout layout270 = Layout{
    "2+7+0",
    std::vector<Channel>{
        Channel{"M+030",
                PolarPosition{30.0, 0.0},
                PolarPosition{30.0, 0.0},
                std::make_pair(30.0, 45.0),
                std::make_pair(0.0, 0.0),
                false},
        Channel{"M-030",
                PolarPosition{-30.0, 0.0},
                PolarPosition{-30.0, 0.0},
                std::make_pair(-45.0, -30.0),
                std::make_pair(0.0, 0.0),
                false},
        Channel{"M+000",
                PolarPosition{0.0, 0.0},
                PolarPosition{0.0, 0.0},
                std::make_pair(0.0, 0.0),
                std::make_pair(0.0, 0.0),
                false},
        Channel{"LFE1",
                PolarPosition{45.0, -30.0},
                PolarPosition{45.0, -30.0},
                std::make_pair(-180.0, 180.0),
                std::make_pair(-90.0, 90.0),
                true},
        Channel{"M+090",
                PolarPosition{90.0, 0.0},
                PolarPosition{90.0, 0.0},
                std::make_pair(85.0, 110.0),
                std::make_pair(0.0, 0.0),
                false},
        Channel{"M-090",
                PolarPosition{-90.0, 0.0},
                PolarPosition{-90.0, 0.0},
                std::make_pair(-110.0, -85.0),
                std::make_pair(0.0, 0.0),
                false},
        Channel{"M+135",
                PolarPosition{135.0, 0.0},
                PolarPosition{135.0, 0.0},
                std::make_pair(120.0, 150.0),
                std::make_pair(0.0, 0.0),
                false},
        Channel{"M-135",
                PolarPosition{-135.0, 0.0},
                PolarPosition{-135.0, 0.0},
                std::make_pair(-150.0, -120.0),
                std::make_pair(0.0, 0.0),
                false},
        Channel{"U+045",
                PolarPosition{45.0, 30.0},
                PolarPosition{45.0, 30.0},
                std::make_pair(30.0, 45.0),
                std::make_pair(30.0, 55.0),
                false},
        Channel{"U-045",
                PolarPosition{-45.0, 30.0},
                PolarPosition{-45.0, 30.0},
                std::make_pair(-45.0, -30.0),
                std::make_pair(30.0, 55.0),
                false},
    }};

Layout layout690 = Layout{
    "6+9+0",
    std::vector<Channel>{
        Channel{"M+060",
                PolarPosition{60.0, 0.0},
                PolarPosition{60.0, 0.0},
                std::make_pair(45.0, 60.0),
                std::make_pair(0.0, 5.0),
                false},
        Channel{"M-060",
                PolarPosition{-60.0, 0.0},
                PolarPosition{-60.0, 0.0},
                std::make_pair(-60.0, -45.0),
                std::make_pair(0.0, 5.0),
                false},
        Channel{"M+000",
                PolarPosition{0.0, 0.0},
                PolarPosition{0.0, 0.0},
                std::make_pair(0.0, 0.0),
                std::make_pair(0.0, 5.0),
                false},
        Channel{"LFE1",
                PolarPosition{45.0, -30.0},
                PolarPosition{45.0, -30.0},
                std::make_pair(30.0, 90.0),
                std::make_pair(-30.0, -15.0),
                true},
        Channel{"M+135",
                PolarPosition{135.0, 0.0},
                PolarPosition{135.0, 0.0},
                std::make_pair(110.0, 135.0),
                std::make_pair(0.0, 15.0),
                false},
        Channel{"M-135",
                PolarPosition{-135.0, 0.0},
                PolarPosition{-135.0, 0.0},
                std::make_pair(-135.0, -110.0),
                std::make_pair(0.0, 15.0),
                false},
        Channel{"M+030",
                PolarPosition{30.0, 0.0},
                PolarPosition{30.0, 0.0},
                std::make_pair(22.5, 30.0),
                std::make_pair(0.0, 5.0),
                false},
        Channel{"M-030",
                PolarPosition{-30.0, 0.0},
                PolarPosition{-30.0, 0.0},
                std::make_pair(-30.0, -22.5),
                std::make_pair(0.0, 5.0),
                false},
        Channel{"M+090",
                PolarPosition{90.0, 0.0},
                PolarPosition{90.0, 0.0},
                std::make_pair(90.0, 90.0),
                std::make_pair(0.0, 15.0),
                false},
        Channel{"M-090",
                PolarPosition{-90.0, 0.0},
                PolarPosition{-90.0, 0.0},
                std::make_pair(-90.0, -90.0),
                std::make_pair(0.0, 15.0),
                false},
        Channel{"U+045",
                PolarPosition{45.0, 30.0},
                PolarPosition{45.0, 30.0},
                std::make_pair(45.0, 60.0),
                std::make_pair(30.0, 45.0),
                false},
        Channel{"U-045",
                PolarPosition{-45.0, 30.0},
                PolarPosition{-45.0, 30.0},
                std::make_pair(-60.0, -45.0),
                std::make_pair(30.0, 45.0),
                false},
        Channel{"U+135",
                PolarPosition{135.0, 30.0},
                PolarPosition{135.0, 30.0},
                std::make_pair(110.0, 135.0),
                std::make_pair(30.0, 45.0),
                false},
        Channel{"U-135",
                PolarPosition{-135.0, 30.0},
                PolarPosition{-135.0, 30.0},
                std::make_pair(-135.0, -110.0),
                std::make_pair(30.0, 45.0),
                false},
        Channel{"U+090",
                PolarPosition{90.0, 30.0},
                PolarPosition{90.0, 30.0},
                std::make_pair(90.0, 90.0),
                std::make_pair(30.0, 45.0),
                false},
        Channel{"U-090",
                PolarPosition{-90.0, 30.0},
                PolarPosition{-90.0, 30.0},
                std::make_pair(-90.0, -90.0),
                std::make_pair(30.0, 45.0),
                false}}};

static int getmodulepath(char *path, int buffsize) {
  int count = 0, i = 0;
#if defined(_WIN32)
  count = GetModuleFileName(NULL, path, buffsize);
#elif defined(__APPLE__)
  uint32_t size = MAX_PATH;
  _NSGetExecutablePath(path, &size);
  count = size;
#else
  count = readlink("/proc/self/exe", path, buffsize);
#endif
  for (i = count - 1; i >= 0; --i) {
    if (path[i] == '\\' || path[i] == '/') {
      path[i] = '\0';
      return (strlen(path));
    }
  }
  return (0);
}

extern "C" EXPORT_API void *CreateBearAPI(char *tf_data_path) {
  int i;
  struct BearAPIImplement *thiz =
      (struct BearAPIImplement *)malloc(sizeof(struct BearAPIImplement));
  char mod_path[MAX_PATH];

  if (thiz) {
    if (tf_data_path == NULL || strlen(tf_data_path) >= sizeof(thiz->tf_data)) {
      if (getmodulepath(mod_path, sizeof(mod_path)) > 0) {
        strcpy(thiz->tf_data, mod_path);
#if defined(_WIN32)
        strcat(thiz->tf_data, "\\default.tf");
#else
        strcat(thiz->tf_data, "/default.tf");
#endif
      } else {
        free(thiz);
        return (NULL);
      }
    } else {
      strcpy(thiz->tf_data, tf_data_path);
    }
#ifdef __linux__
    if (access(thiz->tf_data, F_OK) == -1) {
      fprintf(stderr, "%s is not found.\n", thiz->tf_data);
      free(thiz);
      return (NULL);
    }
#elif _WIN32
    DWORD attrib = GetFileAttributes(thiz->tf_data);
    if (attrib == INVALID_FILE_ATTRIBUTES) {
      fprintf(stderr, "%s is not found.\n", thiz->tf_data);
      free(thiz);
      return (NULL);
    } else if (attrib & FILE_ATTRIBUTE_DIRECTORY) {
      fprintf(stderr, "%s is not found.\n", thiz->tf_data);
      free(thiz);
      return (NULL);
    }
#endif
    for (i = 0; i < MAX_INPUT_SOURCES; i++) {
      thiz->m_pConfig[i] = NULL;
      thiz->m_pRenderer[i] = NULL;
      thiz->m_pDirectSpeakersInput[i] = NULL;
    }
    return ((void *)thiz);
  } else {
    return (NULL);
  }
}

extern "C" EXPORT_API void DestroyBearAPI(void *pv_thiz) {
  int i;
  BearAPIImplement *thiz = (BearAPIImplement *)pv_thiz;

  if (pv_thiz) {
    for (i = 0; i < MAX_INPUT_SOURCES; i++) {
      if (thiz->m_pConfig[i]) delete thiz->m_pConfig[i];
      if (thiz->m_pConfig[i]) delete thiz->m_pRenderer[i];
      if (thiz->m_pConfig[i]) delete thiz->m_pDirectSpeakersInput[i];
    }
    return free(pv_thiz);
  }
}

extern "C" EXPORT_API int ConfigureBearDirectSpeakerChannel(
    void *pv_thiz, int layout, int sp_flags, size_t nsample_per_frame,
    int sample_rate) {
  int source_id = -1;
  int is, ch, m;
  BearAPIImplement *thiz = (BearAPIImplement *)pv_thiz;
  ear::Layout baselayout;
  ear::Layout earlayout;
  int channels;
  std::vector<Channel> channel_list;
  int chmap[MAX_INPUT_CHANNELS] = {
      0,
  };

  if (pv_thiz) {
    for (is = 0; is < MAX_INPUT_SOURCES; is++) {
      if (!thiz->m_pConfig[is]) break;
    }
    if (is < MAX_INPUT_SOURCES) {
      thiz->m_pConfig[is] = new Config();
      switch (layout) {
        case IAMF_STEREO:
          baselayout = ear::getLayout("0+2+0");
          channels = 2;
          break;
        case IAMF_51:
          baselayout = ear::getLayout("0+5+0");
          channels = 6;
          break;
        case IAMF_512:
          baselayout = ear::getLayout("2+5+0");
          channels = 8;
          break;
        case IAMF_514:
          baselayout = ear::getLayout("4+5+0");
          channels = 10;
          break;
        case IAMF_71:
          baselayout = ear::getLayout("0+7+0");
          channels = 8;
          break;
        case IAMF_714:
          baselayout = ear::getLayout("4+7+0");
          channels = 12;
          break;
        case IAMF_MONO:
          baselayout = layout010;  // extended
          channels = 1;
          break;
        case IAMF_712:
          baselayout = layout270;  // extended
          channels = 10;
          break;
        case IAMF_312:
          baselayout = layout230;  // extended
          channels = 6;
          break;
        case IAMF_916:
          baselayout = layout690;  // extended
          channels = 16;
          break;
      }
      if (sp_flags == 0) {  // predefinded layout(input)
        earlayout = baselayout;
      } else {  // custom speaker layout(input)
        for (int i = 0; i < sizeof(iamf_splabel_per_layout) /
                                sizeof(struct iamf_splabel_per_layout_t);
             i++) {
          if (layout == iamf_splabel_per_layout[i].system) {
            m = 0;
            for (ch = 0; ch < iamf_splabel_per_layout[i].channels; ch++) {
              if (sp_flags & iamf_splabel_per_layout[i].sp_labels[ch]) {
                chmap[m] = ch;
                m++;
              }
            }
            break;
          }
        }
        channels = m;
        for (int i = 0; i < channels; i++) {
          Channel chx = baselayout.channels()[chmap[i]];
          channel_list.push_back(chx);
        }
        earlayout = Layout("extended_layout", channel_list);
      }

      thiz->m_pConfig[is]->set_num_direct_speakers_channels(channels);
      thiz->m_pConfig[is]->set_period_size(nsample_per_frame);
      thiz->m_pConfig[is]->set_sample_rate(sample_rate);
      thiz->m_pConfig[is]->set_data_path(thiz->tf_data);

      thiz->m_pRenderer[is] = new Renderer(*thiz->m_pConfig[is]);
      thiz->m_pDirectSpeakersInput[is] = new DirectSpeakersInput();

      for (ch = 0; ch < earlayout.channels().size(); ch++) {
        double Azi = earlayout.channels().at(ch).polarPosition().azimuth;
        double El = earlayout.channels().at(ch).polarPosition().elevation;
        double Dist = earlayout.channels().at(ch).polarPosition().distance;
        thiz->m_pDirectSpeakersInput[is]->type_metadata.position =
            ear::PolarSpeakerPosition(Azi, El, Dist);
        thiz->m_pRenderer[is]->add_direct_speakers_block(
            ch, *thiz->m_pDirectSpeakersInput[is]);
      }

      source_id = is;
    }
  }

  return (source_id);
}

extern "C" EXPORT_API int SetBearDirectSpeakerChannel(void *pv_thiz,
                                                      int source_id,
                                                      float **in) {
  BearAPIImplement *thiz = (BearAPIImplement *)pv_thiz;
  if (0 <= source_id && source_id < MAX_INPUT_SOURCES) {
    if (pv_thiz && thiz->m_pConfig[source_id] != NULL) {
      thiz->in[source_id] = in;
      return (0);
    }
  }
  return (-1);
}

extern "C" EXPORT_API void DestroyBearChannel(void *pv_thiz, int source_id) {
  BearAPIImplement *thiz = (BearAPIImplement *)pv_thiz;

  if (pv_thiz) {
    if (0 <= source_id && source_id < MAX_INPUT_SOURCES) {
      if (thiz->m_pConfig[source_id]) {
        if (thiz->m_pConfig[source_id]) delete thiz->m_pConfig[source_id];
        if (thiz->m_pConfig[source_id]) delete thiz->m_pRenderer[source_id];
        if (thiz->m_pConfig[source_id])
          delete thiz->m_pDirectSpeakersInput[source_id];
        thiz->m_pConfig[source_id] = NULL;
        thiz->m_pRenderer[source_id] = NULL;
        thiz->m_pDirectSpeakersInput[source_id] = NULL;
        thiz->in[source_id] = NULL;
      }
    }
  }
}

extern "C" EXPORT_API int GetBearRenderedAudio(void *pv_thiz, int source_id,
                                               float **out) {
  BearAPIImplement *thiz = (BearAPIImplement *)pv_thiz;

  if (pv_thiz) {
    if (0 <= source_id && source_id < MAX_INPUT_SOURCES &&
        thiz->in[source_id] != NULL) {
      if (thiz->m_pConfig[source_id]) {
        thiz->m_pRenderer[source_id]->process(NULL, thiz->in[source_id], NULL,
                                              out);
        return (0);
      }
    }
  }
  return (-1);
}
