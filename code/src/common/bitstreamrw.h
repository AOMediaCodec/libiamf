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
 * @file bitstreamrw.h
 * @brief bitstream writer and reader function
 * @version 0.1
 * @date Created 3/3/2023
**/

#ifndef BITSTREAM_RW_H_
#define BITSTREAM_RW_H_

#include <stdint.h>

#define _CHAR_BIT 8

typedef struct {
  uint8_t *m_ptr;
  int m_size;
  int m_posBase;
  int m_posInBase;
} bitstream_t;

#ifdef __cplusplus
extern "C" {
#endif

void bs_init(bitstream_t *bs, uint8_t *data_buf, int msize);
uint32_t bs_available(bitstream_t *bs);
int bs_getbit(bitstream_t *bs);
uint32_t bs_getbits(bitstream_t *bs, uint32_t num);
uint32_t bs_get_variable_bits(bitstream_t *bs, uint32_t n_bits);
uint32_t bs_showbits(bitstream_t *bs, uint32_t num);
uint32_t bs_skippadbits(bitstream_t *bs);
int bs_setbit(bitstream_t *bs, int bit);
void bs_setbits(bitstream_t *bs, uint32_t num, int nbits);
uint32_t bs_set_variable_bits(bitstream_t *bs, uint32_t num, uint32_t n_bits);
uint32_t bs_setpadbits(bitstream_t *bs);

uint32_t get_uint32be(void *buf, int offset);
int32_t get_int32be(void *buf, int offset);
uint16_t get_uint16be(void *buf, int offset);
int16_t get_int16be(void *buf, int offset);
uint8_t get_uint8(void *buf, int offset);
int8_t get_int8(void *buf, int offset);

void set_int32be(int32_t u32, void *buf, int offset);
void set_uint32be(uint32_t u32, void *buf, int offset);
void set_int16be(int16_t u16, void *buf, int offset);
void set_uint16be(uint16_t u16, void *buf, int offset);
void set_int8(int8_t i8, void *buf, int offset);
void set_uint8(uint8_t u8, void *buf, int offset);

#ifdef __cplusplus
}
#endif

#endif
