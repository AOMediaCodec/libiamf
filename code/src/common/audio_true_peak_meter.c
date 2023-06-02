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
 * @file audio_true_peak_meter.c
 * @brief Audio true peak calculation and process
 * @version 0.1
 * @date Created 3/3/2023
**/

#include <math.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//#include "os_support.h"
//#include "opus_defines.h"

#include "audio_true_peak_meter.h"

SamplePhaseFilters* sample_phase_filters_create(void) {
  SamplePhaseFilters* thisp = NULL;
  thisp = (SamplePhaseFilters*)malloc(sizeof(SamplePhaseFilters));
  if (thisp && sample_phase_filters_init(thisp) != 0) {
    free(thisp);
    thisp = NULL;
  }
  return thisp;
}

int sample_phase_filters_init(SamplePhaseFilters* thisp) {
  int ret = 0;

  float fillter_array0[] = {
      0.0017089843750,  0.0109863281250,  -0.0196533203125, 0.0332031250000,
      -0.0594482421875, 0.1373291015625,  0.9721679687500,  -0.1022949218750,
      0.0476074218750,  -0.0266113281250, 0.0148925781250,  -0.0083007812500};
  float fillter_array1[] = {
      -0.0291748046875, 0.0292968750000,  -0.0517578125000, 0.0891113281250,
      -0.1665039062500, 0.4650878906250,  0.7797851562500,  -0.2003173828125,
      0.1015625000000,  -0.0582275390625, 0.0330810546875,  -0.0189208984375};
  float fillter_array2[] = {
      -0.0189208984375, 0.0330810546875,  -0.0582275390625, 0.1015625000000,
      -0.2003173828125, 0.7797851562500,  0.4650878906250,  -0.1665039062500,
      0.0891113281250,  -0.0517578125000, 0.0292968750000,  -0.0291748046875};
  float fillter_array3[] = {
      -0.0083007812500, 0.0148925781250,  -0.0266113281250, 0.0476074218750,
      -0.1022949218750, 0.9721679687500,  0.1373291015625,  -0.0594482421875,
      0.0332031250000,  -0.0196533203125, 0.0109863281250,  0.0017089843750};

  memcpy(thisp->phase_filters[0], fillter_array0, sizeof(fillter_array0));
  memcpy(thisp->phase_filters[1], fillter_array1, sizeof(fillter_array1));
  memcpy(thisp->phase_filters[2], fillter_array2, sizeof(fillter_array2));
  memcpy(thisp->phase_filters[3], fillter_array3, sizeof(fillter_array3));

  for (int i = 0; i < 4; i++) {
    thisp->phase_filters_p[i] = thisp->phase_filters[i];
  }
  thisp->numofphases = countof(thisp->phase_filters_p);
  thisp->numofcoeffs = countof(thisp->phase_filters[0]);
  thisp->in_buffer = (float*)malloc(sizeof(float) * thisp->numofcoeffs);
  if (thisp->in_buffer) {
    for (int i = 0; i < thisp->numofcoeffs; i++) thisp->in_buffer[i] = 0;
  } else
    ret = -1;

  return ret;
}

int sample_phase_filters_deinit(SamplePhaseFilters* thisp) {
  int ret = 0;
  if (thisp->in_buffer) free(thisp->in_buffer);
  return ret;
}

void sample_phase_filters_destroy(SamplePhaseFilters* thisp) {
  if (thisp->in_buffer) free(thisp->in_buffer);
  free(thisp);
}

void sample_phase_filters_reset_states(SamplePhaseFilters* thisp) {
  for (int i = 0; i < thisp->numofcoeffs; i++) thisp->in_buffer[i] = 0;
}

void sample_phase_filters_next_over_sample(SamplePhaseFilters* thisp,
                                           float sample, float* results) {
  // shift buffer
  for (int i = thisp->numofcoeffs - 1; i > 0; i--) {
    thisp->in_buffer[i] = thisp->in_buffer[i - 1];
  }
  thisp->in_buffer[0] = sample;

  //  calc
  for (int phase = 0; phase < thisp->numofphases; phase++) {
    float* filterPhase = thisp->phase_filters_p[phase];
    float sum = 0;
    for (int coeff = 0; coeff < thisp->numofcoeffs; coeff++) {
      sum += thisp->in_buffer[coeff] * filterPhase[coeff];
    }
    results[phase] = sum;
  }
}

AudioTruePeakMeter* audio_true_peak_meter_create(void) {
  AudioTruePeakMeter* thisp = NULL;
  thisp = (AudioTruePeakMeter*)malloc(sizeof(AudioTruePeakMeter));
  if (thisp) memset(thisp, 0x00, sizeof(AudioTruePeakMeter));
  return thisp;
}

int audio_true_peak_meter_init(AudioTruePeakMeter* thisp) {
  return sample_phase_filters_init(&thisp->phasefilters);
}

int audio_true_peak_meter_deinit(AudioTruePeakMeter* thisp) {
  return sample_phase_filters_deinit(&thisp->phasefilters);
}

void audio_true_peak_meter_destroy(AudioTruePeakMeter* thisp) {
  sample_phase_filters_deinit(&thisp->phasefilters);
  free(thisp);
}

void audio_true_peak_meter_reset_states(AudioTruePeakMeter* thisp) {
  sample_phase_filters_reset_states(&thisp->phasefilters);
}

float audio_true_peak_meter_next_true_peak(AudioTruePeakMeter* thisp,
                                           float sample) {
  float over_sampled_buffer[4] = {0};

  sample_phase_filters_next_over_sample(&thisp->phasefilters, sample,
                                        over_sampled_buffer);
  float max_linear = -3.4e34;
  float absValue;
  for (int i = 0; i < 4; i++) {
    absValue = fabs(over_sampled_buffer[i]);
    if (absValue > max_linear) max_linear = absValue;
  }
  return max_linear;
}
