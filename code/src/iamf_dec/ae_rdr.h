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
 * @file ae_rdr.h
 * @brief Rendering APIs.
 * @version 0.1
 * @date Created 03/03/2023
 **/

#ifndef AE_RDR_H_
#define AE_RDR_H_

#include <stdint.h>

#include "arch.h"

#define BS2051_MAX_CHANNELS 24  // BS2051_H
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
  IAMF_MONO = 0x100,       // 1ch input/output, AOM only
  IAMF_712 = 0x712,        // 10ch input/output, AOM only
  IAMF_312 = 0x312,        // 6ch input/output, AOM only
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

#ifndef DISABLE_LFE_HOA
#define DISABLE_LFE_HOA 1
#endif

// BINAURAL feature
#ifndef ENABLE_HOA_TO_BINAURAL
#define ENABLE_HOA_TO_BINAURAL 0
#endif

#ifndef ENABLE_MULTICHANNEL_TO_BINAURAL
#define ENABLE_MULTICHANNEL_TO_BINAURAL 0
#endif

#if ENABLE_HOA_TO_BINAURAL || ENABLE_MULTICHANNEL_TO_BINAURAL

/// maximum number of audio element == maximum source id array size
#define N_SOURCE_ELM 28

typedef struct {
#if ENABLE_MULTICHANNEL_TO_BINAURAL
  int m2b_init;
  void *m2b_api;
  int m2b_elm_id[N_SOURCE_ELM];
  int m2b_source_id[N_SOURCE_ELM];
#endif
#if ENABLE_HOA_TO_BINAURAL
  int h2b_init;
  void *h2b_api;
  int h2b_elm_id[N_SOURCE_ELM];
  int h2b_amb_id[N_SOURCE_ELM];
  int h2b_inchs[N_SOURCE_ELM];
#endif
} binaural_filter_t;
#endif

typedef struct {
  int init;
  float c, a1, a2, a3, b1, b2;
  float input_history[2], output_history[2];
} lfe_filter_t;

typedef struct {
  IAMF_SOUND_SYSTEM system;
  int lfe1;
  int lfe2;
} IAMF_PREDEFINED_SP_LAYOUT;

typedef struct {
  IAMF_SOUND_SYSTEM system;
  BS2051_SPLABEL sp_flags;  // subset of BS2051_SPLABEL
} IAMF_CUSTOM_SP_LAYOUT;

typedef struct {
  int sp_type;  // 0: predefined_sp, 1: custom_sp
  union {
    IAMF_PREDEFINED_SP_LAYOUT *predefined_sp;
    IAMF_CUSTOM_SP_LAYOUT *custom_sp;
  } sp_layout;
  lfe_filter_t lfe_f;  // for H2M lfe
#if ENABLE_HOA_TO_BINAURAL || ENABLE_MULTICHANNEL_TO_BINAURAL
  binaural_filter_t binaural_f;
#endif
} IAMF_SP_LAYOUT;

typedef enum {
  IAMF_ZOA = 0,
  IAMF_FOA = 1,
  IAMF_SOA = 2,
  IAMF_TOA = 3,
  IAMF_H4A = 4
} HOA_ORDER;

typedef struct {
  HOA_ORDER order;
  int lfe_on;  // HOA lfe on/off
} IAMF_HOA_LAYOUT;

struct m2m_rdr_t {
  IAMF_SOUND_SYSTEM in;
  IAMF_SOUND_SYSTEM out;
  float *mat;
  int m;
  int n;
};

struct h2m_rdr_t {
  HOA_ORDER in;
  IAMF_SOUND_SYSTEM out;
  int channels;
  int lfe1;
  int lfe2;
  float *mat;
  int m;
  int n;
};

// Predefined Multichannel to Multichannel
/**
 * @brief     Get the ear render conversion matrix of predefined multichannel to
 * multichannel according to predefined direct speaker input and output layout.
 * @param     [in] in : predefined direct speaker input channel layout.
 * @param     [in] out : predefined direct speaker output channel layout
 * @param     [in] outMatrix : conversion matrix.
 * @return    @0: success,@others: fail
 */
int IAMF_element_renderer_get_M2M_matrix(IAMF_SP_LAYOUT *in,
                                         IAMF_SP_LAYOUT *out,
                                         struct m2m_rdr_t *outMatrix);

/**
 * @brief     Get the ear render conversion matrix of custom multichannel to
 * multichannel according to predefined direct speaker input and custom output
 * layout.
 * @param     [in] in : custom speaker input layout.
 * @param     [in] out : predefined speaker output layout
 * @param     [in] outMatrix : conversion matrix.
 * @param     [in] chmap : speaker subset channel list in input layout.
 * @return    @0: success,@others: fail
 */
int IAMF_element_renderer_get_M2M_custom_matrix(IAMF_SP_LAYOUT *in,
                                                IAMF_SP_LAYOUT *out,
                                                struct m2m_rdr_t *outMatrix,
                                                int *chmap);

/**
 * @brief     Predefined Multichannel to Multichannel Renderer.
 * @param     [in] arch : architecture-specific callbacks.
 * @param     [in] in : the pcm signal of input.
 * @param     [in] out : the pcm signal of output
 * @param     [in] nsamples : the processed samples of pcm signal.
 * @return    @0: success,@others: fail
 */
int IAMF_element_renderer_render_M2M(const Arch *arch,
                                     struct m2m_rdr_t *m2mMatrix, float *in[],
                                     float *out[], int nsamples);

/**
 * @brief     Custom Multichannel to Multichannel Renderer.
 * @param     [in] arch : architecture-specific callbacks.
 * @param     [in] in : the pcm signal of input.
 * @param     [in] out : the pcm signal of output
 * @param     [in] nsamples : the processed samples of pcm signal.
 * @param     [in] chmap : speaker subset channel list in input layout.
 * @return    @0: success,@others: fail
 */
int IAMF_element_renderer_render_M2M_custom(const Arch *arch,
                                            struct m2m_rdr_t *m2mMatrix,
                                            float *in[], float *out[],
                                            int nsamples, int *chmap);
// HOA to Multichannel
#if DISABLE_LFE_HOA == 0
void lfefilter_init(lfe_filter_t *lfe_f, float cutoff_freq,
                    float sampling_rate);
#endif
/**
 * @brief     Get the ear render conversion matrix of hoa to multichannel
 *            according to hoa input and direct speaker output layout.
 * @param     [in] in : HOA channel layout.
 * @param     [in] out : direct speaker output channel layout
 * @param     [in] outMatrix : conversion matrix.
 * @return    @0: success,@others: fail
 */
int IAMF_element_renderer_get_H2M_matrix(IAMF_HOA_LAYOUT *in,
                                         IAMF_PREDEFINED_SP_LAYOUT *out,
                                         struct h2m_rdr_t *outMatrix);

/**
 * @brief     Hoa to Multichannel Renderer.
 * @param     [in] arch : architecture-specific callbacks.
 * @param     [in] in : the pcm signal of input.
 * @param     [in] out : the pcm signal of output
 * @param     [in] nsamples : the processed samples of pcm signal.
 * @param     [in] lfe : the filter to prcoess lfe channel.
 * @return    @0: success,@others: fail
 */
int IAMF_element_renderer_render_H2M(const Arch *arch,
                                     struct h2m_rdr_t *h2mMatrix, float *in[],
                                     float *out[], int nsamples,
                                     lfe_filter_t *lfe);

#if ENABLE_MULTICHANNEL_TO_BINAURAL
// Multichannel to Binaural(BEAR)
/**
 * @brief     Initialize the conversion filter of multichannel to binaural
 *            according to predefined direct speaker input layout.
 * @param     [in] binaural_f : the binaural filter.
 * @param     [in] in_layout : predefined direct speaker input channel layout
 * @param     [in] elm_id : the element id.
 * @param     [in] frame_size : the size of one pcm frame.
 * @param     [in] sample_rate : the sample rate of pcm signal.
 */
void IAMF_element_renderer_init_M2B(binaural_filter_t *binaural_f,
                                    uint32_t in_layout, int sp_flags,
                                    uint64_t elm_id, int frame_size,
                                    int sample_rate);

/**
 * @brief     De-initialize the conversion filter of multichannel to binaural.
 * @param     [in] binaural_f : the binaural filter.
 * @param     [in] elm_id : the element id.
 */
void IAMF_element_renderer_deinit_M2B(binaural_filter_t *binaural_f,
                                      uint64_t elm_id);

/**
 * @brief     Multichannel to Binaural Renderer.
 * @param     [in] binaural_f : the binaural filter.
 * @param     [in] elm_id : the element id.
 * @param     [in] in : the input pcm signal.
 * @param     [in] in : the output pcm signal.
 */
int IAMF_element_renderer_render_M2B(binaural_filter_t *binaural_f,
                                     uint64_t elm_id, float *in[], float *out[],
                                     int nsamples);
#endif

#if ENABLE_HOA_TO_BINAURAL
// HOA to Binaural(Resonance)
/**
 * @brief     Initialize the conversion filter of hoa to binaural according to
 * hoa input.
 * @param     [in] binaural_f : the binaural filter.
 * @param     [in] in_channels : the channels of input hoa.
 * @param     [in] elm_id : the element id.
 * @param     [in] frame_size : the size of one pcm frame.
 * @param     [in] sample_rate : the sample rate of pcm signal.
 */
void IAMF_element_renderer_init_H2B(binaural_filter_t *binaural_f,
                                    int in_channels, uint64_t elm_id,
                                    int frame_size, int sample_rate);

/**
 * @brief     De-initialize the conversion filter of hoa to binaural.
 * @param     [in] binaural_f : the binaural filter.
 * @param     [in] elm_id : the element id.
 */
void IAMF_element_renderer_deinit_H2B(binaural_filter_t *binaural_f,
                                      uint64_t elm_id);

/**
 * @brief     HOA to Binaural Renderer.
 * @param     [in] binaural_f : the binaural filter.
 * @param     [in] elm_id : the element id.
 * @param     [in] in : the input pcm signal.
 * @param     [in] in : the output pcm signal.
 */
int IAMF_element_renderer_render_H2B(binaural_filter_t *binaural_f,
                                     uint64_t elm_id, float *in[], float *out[],
                                     int nsamples);
#endif

#endif
