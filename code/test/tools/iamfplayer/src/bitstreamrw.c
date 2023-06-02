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
 * @file bitstreamrw.c
 * @brief bitstream reader and writer.
 * @version 0.1
 * @date Created 03/03/2023
 **/


#include "bitstreamrw.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

int bs_getbit(bitstream_t *bs);
uint32_t bs_getbits(bitstream_t *bs, uint32_t num);
uint32_t bs_showbits(bitstream_t *bs, uint32_t num);

void bs_init(bitstream_t *bs, uint8_t *data_buf, int msize) {
  bs->m_ptr = data_buf;
  bs->m_size = msize;
  bs->m_posBase = 0;
  bs->m_posInBase = _CHAR_BIT - 1;
}

uint32_t bs_available(bitstream_t *bs) {
  return (bs->m_size - bs->m_posBase - 1) * _CHAR_BIT + bs->m_posInBase + 1;
}

int bs_getbit(bitstream_t *bs) {
  int res;
  if (bs->m_posBase >= bs->m_size) {
    fprintf(stderr, "BitstreamRW: not enough data");
    assert(0);
  }

  res = bs->m_ptr[bs->m_posBase] & (1 << bs->m_posInBase);
  bs->m_posInBase--;

  if (bs->m_posInBase < 0) {
    bs->m_posInBase = _CHAR_BIT - 1;
    bs->m_posBase++;
  }

  return res;
}

uint32_t bs_getbits(bitstream_t *bs, uint32_t num) {
  int i;
  uint32_t result;
  assert(num <= 32);

  result = 0;
  for (i = 0; i < num; i++) {
    if (bs_getbit(bs)) result |= (1 << (num - i - 1));
  }
  return result;
}

uint32_t bs_showbits(bitstream_t *bs, uint32_t num) {
  assert(num <= 32);
  int posBasePrev;
  int posInBasePrev;
  uint32_t result;
  int i;

  posBasePrev = bs->m_posBase;
  posInBasePrev = bs->m_posInBase;

  result = 0;
  for (i = 0; i < num; i++) {
    if (bs_getbit(bs)) {
      result |= 1 << (num - i - 1);
    }
  }

  bs->m_posBase = posBasePrev;
  bs->m_posInBase = posInBasePrev;

  return result;
}

uint32_t bs_get_variable_bits(bitstream_t *bs, uint32_t n_bits) {
  uint32_t data32 = 0;
  uint32_t dataTemp = 0;
  uint32_t read_more = 0;
  do {
    dataTemp += bs_getbits(bs, n_bits + 1);
    read_more = dataTemp & 0x01;
    if (read_more) {
      dataTemp <<= n_bits;
    } else {
      dataTemp >>= 1;
    }
  } while (read_more);
  return data32;
}

uint32_t bs_skippadbits(bitstream_t *bs) { return (bs_setpadbits(bs)); }

int bs_setbit(bitstream_t *bs, int bit) {
  uint8_t res = 0;
  if (bs->m_posBase >= bs->m_size) {
    fprintf(stderr, "BitstreamRW: not enough data");
    assert(0);
  }

  res = bs->m_ptr[bs->m_posBase];
  if (bit) {
    res = res | (1 << bs->m_posInBase);
  } else {
    res = res & ~(1 << bs->m_posInBase);
  }
  bs->m_ptr[bs->m_posBase] = res;
  bs->m_posInBase--;

  if (bs->m_posInBase < 0)  // _CHAR_BIT == 8
  {
    bs->m_posInBase = _CHAR_BIT - 1;
    bs->m_posBase++;
  }

  return res;
}

void bs_setbits(bitstream_t *bs, uint32_t num, int nbits) {
  int i, bit;
  assert(nbits <= 32);

  for (i = 0; i < nbits; i++) {
    bit = (num >> (nbits - i - 1)) & 0x01;
    bs_setbit(bs, bit);
  }
}

uint32_t bs_setpadbits(bitstream_t *bs) {
  uint8_t res = 0;
  uint8_t maskv[] = {0x01, 0x03, 0x07, 0x0F, 0x1F, 0x3F, 0x7F, 0xFF};

  if (bs->m_posInBase < _CHAR_BIT - 1) {
    res = bs->m_ptr[bs->m_posBase] & ~maskv[bs->m_posInBase];
    bs->m_ptr[bs->m_posBase] = res;
    res = bs->m_posInBase + 1;

    bs->m_posInBase = _CHAR_BIT - 1;
    bs->m_posBase++;
  }

  return (res);
}

uint32_t get_uint32be(void *buf, int offset) {
  uint32_t x;
  uint8_t *v;
  uint8_t *be = (uint8_t *)buf;

  v = (uint8_t *)&x;
  v[0] = be[offset + 3];
  v[1] = be[offset + 2];
  v[2] = be[offset + 1];
  v[3] = be[offset + 0];

  return (x);
}

int32_t get_int32be(void *buf, int offset) {
  int32_t x;
  uint8_t *v;
  uint8_t *be = (uint8_t *)buf;

  v = (uint8_t *)&x;
  v[0] = be[offset + 3];
  v[1] = be[offset + 2];
  v[2] = be[offset + 1];
  v[3] = be[offset + 0];

  return (x);
}

uint16_t get_uint16be(void *buf, int offset) {
  uint16_t x;
  uint8_t *v;
  uint8_t *be = (uint8_t *)buf;

  v = (uint8_t *)&x;
  v[0] = be[offset + 1];
  v[1] = be[offset + 0];

  return (x);
}

int16_t get_int16be(void *buf, int offset) {
  int16_t x;
  uint8_t *v;
  uint8_t *be = (uint8_t *)buf;

  v = (uint8_t *)&x;
  v[0] = be[offset + 1];
  v[1] = be[offset + 0];

  return (x);
}

uint8_t get_uint8(void *buf, int offset) {
  uint8_t x;
  uint8_t *v;
  uint8_t *bx = (uint8_t *)buf;

  v = (uint8_t *)&x;
  v[0] = bx[offset];

  return (x);
}

int8_t get_int8(void *buf, int offset) {
  int8_t x;
  int8_t *v;
  int8_t *bx = (int8_t *)buf;

  v = (int8_t *)&x;
  v[0] = bx[offset];

  return (x);
}

void set_int32be(int32_t i32, void *buf, int offset) {
  uint8_t *v = ((uint8_t *)buf) + offset;

  v[0] = (i32 >> 24) & 0xff;
  v[1] = (i32 >> 16) & 0xff;
  v[2] = (i32 >> 8) & 0xff;
  v[3] = i32 & 0xff;
}

void set_uint32be(uint32_t u32, void *buf, int offset) {
  uint8_t *v = ((uint8_t *)buf) + offset;

  v[0] = (u32 >> 24) & 0xff;
  v[1] = (u32 >> 16) & 0xff;
  v[2] = (u32 >> 8) & 0xff;
  v[3] = u32 & 0xff;
}

void set_int16be(int16_t i16, void *buf, int offset) {
  uint8_t *v = ((uint8_t *)buf) + offset;

  v[0] = (i16 >> 8) & 0xff;
  v[1] = i16 & 0xff;
}

void set_uint16be(uint16_t u16, void *buf, int offset) {
  uint8_t *v = ((uint8_t *)buf) + offset;

  v[0] = (u16 >> 8) & 0xff;
  v[1] = u16 & 0xff;
}

void set_int8(int8_t i8, void *buf, int offset) {
  int8_t *v = ((int8_t *)buf) + offset;
  v[0] = i8;
}

void set_uint8(uint8_t u8, void *buf, int offset) {
  uint8_t *v = ((uint8_t *)buf) + offset;

  v[0] = u8;
}
