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
 * @file iamf_header.c
 * @brief IAMF information.
 * @version 0.1
 * @date Created 03/03/2023
 **/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>

#include "atom.h"
#include "dmemory.h"
#include "iamf_header.h"

/* Header contents:
  - "OpusHead" (64 bits)
  - version number (8 bits)
  - Channels C (8 bits)
  - Pre-skip (16 bits)
  - Sampling rate (32 bits)
  - Gain in dB (16 bits, S7.8)
  - Mapping (8 bits, 0=single stream (mono/stereo) 1=Vorbis mapping,
             2..254: reserved, 255: multistream with no mapping)

  - if (mapping != 0)
     - N = total number of streams (8 bits)
     - M = number of paired streams (8 bits)
     - C times channel origin
          - if (C<2*M)
             - stream = byte/2
             - if (byte&0x1 == 0)
                 - left
               else
                 - right
          - else
             - stream = byte-M
*/

typedef struct {
  const unsigned char *data;
  int maxlen;
  int pos;
} ROPacket;

static int read_uint32(ROPacket *p, uint32_t *val) {
  if (p->pos > p->maxlen - 4) {
    return 0;
  }
  *val = (uint32_t)p->data[p->pos];
  *val = *val << 8 | (uint32_t)p->data[p->pos + 1];
  *val = *val << 8 | (uint32_t)p->data[p->pos + 2];
  *val = *val << 8 | (uint32_t)p->data[p->pos + 3];
  p->pos += 4;
  return 1;
}

static int read_uint16(ROPacket *p, uint16_t *val) {
  if (p->pos > p->maxlen - 2) {
    return 0;
  }
  *val = (uint16_t)p->data[p->pos];
  *val = *val << 8 | (uint16_t)p->data[p->pos + 1];
  p->pos += 2;
  return 1;
}

static int read_chars(ROPacket *p, unsigned char *str, int nb_chars) {
  int i;
  if (p->pos > p->maxlen - nb_chars) {
    return 0;
  }
  if (!str)
    p->pos += nb_chars;
  else {
    for (i = 0; i < nb_chars; i++) {
      str[i] = p->data[p->pos++];
    }
  }
  return 1;
}

static uint32_t read_tag_size(ROPacket *p) {
  uint8_t ch = 0;
  uint32_t size = 0;

  for (int cnt = 0; cnt < 4; cnt++) {
    read_chars(p, &ch, 1);

    size <<= 7;
    size |= (ch & 0x7f);
    if (!(ch & 0x80)) {
      break;
    }
  }
  return size;
}

int iamf_header_read_description_OBUs(IAMFHeader *h, unsigned char *dst,
                                      int size) {
  if (size) {
    if (h->description) {
      if (h->description_length <= size)
        memcpy(dst, h->description, h->description_length);
      else
        memcpy(dst, h->description, size);
      return h->description_length;
    }
  }
  return 0;
}

#define FREE(x)                    \
  if (x) {                         \
    _dfree(x, __FILE__, __LINE__); \
    x = 0;                         \
  }
void iamf_header_free(IAMFHeader *h, int n) {
  if (h) {
    for (int i = 0; i < n; ++i) {
      FREE(h[i].description);
    }
  }
  FREE(h);
}
