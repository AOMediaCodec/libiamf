/*
BSD 3-Clause Clear License The Clear BSD License

Copyright (c) 2023, Alliance for Open Media.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/**
 * @file audio_true_peak_meter.h
 * @brief Audio true peak calculation and process
 * @version 0.1
 * @date Created 3/3/2023
**/

#ifndef _AUDIO_TRUEPEAK_METER_H_
#define _AUDIO_TRUEPEAK_METER_H_

#include "audio_defines.h"

#ifndef countof
#define countof(x) (sizeof(x) / sizeof(x[0]))
#endif

#define MAX_PHASES 12

typedef struct SamplePhaseFilters {
  int numofphases;
  int numofcoeffs;
  float* in_buffer;

  float phase_filters[4][MAX_PHASES];
  float* phase_filters_p[4];

} SamplePhaseFilters;

SamplePhaseFilters* sample_phase_filters_create(void);
int sample_phase_filters_init(SamplePhaseFilters*);
void sample_phase_filters_destroy(SamplePhaseFilters*);
void sample_phase_filters_reset_states(SamplePhaseFilters*);
void sample_phase_filters_next_over_sample(SamplePhaseFilters*, float sample,
                                           float* results);

typedef struct AudioTruePeakMeter {
  SamplePhaseFilters phasefilters;
} AudioTruePeakMeter;

AudioTruePeakMeter* audio_true_peak_meter_create(void);
int audio_true_peak_meter_init(AudioTruePeakMeter*);
int audio_true_peak_meter_deinit(AudioTruePeakMeter*);
void audio_true_peak_meter_destroy(AudioTruePeakMeter*);
void audio_true_peak_meter_reset_states(AudioTruePeakMeter*);
float audio_true_peak_meter_next_true_peak(AudioTruePeakMeter*, float sample);

#endif
