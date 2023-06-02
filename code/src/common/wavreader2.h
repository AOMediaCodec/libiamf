/* ------------------------------------------------------------------
 * Copyright (C) 2009 Martin Storsjo
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 * -------------------------------------------------------------------
 */

#ifndef WAVREADER2_H
#define WAVREADER2_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef WAVE_FORMAT_PCM2
#define WAVE_FORMAT_PCM2 1
#define WAVE_FORMAT_FLOAT2 3
#endif

void* wav_read_open2(const char* filename);
void wav_read_close2(void* obj);
int wav_get_header2(void* obj, int* format, int* channels, int* sample_rate,
                    int* bits_per_sample, unsigned int* data_length);
int wav_read_data2(void* obj, unsigned char* data, unsigned int length);

#ifdef __cplusplus
}
#endif

#endif
