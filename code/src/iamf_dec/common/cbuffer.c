/*
 * Copyright (c) 2025, Alliance for Open Media. All rights reserved
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
 * @file cbuffer.c
 * @brief Circular buffer implementation.
 * @version 0.1
 * @date Created 12/11/2025
 **/

#include "cbuffer.h"

// #include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct CBuffer {
  uint8_t* data;
  uint32_t size;
};

static void _buffer_free(buffer_t* buf) {
  if (!buf) return;

  // fprintf(stderr, "freeing buffer %p.\n", buf);

  if (buf->data) free(buf->data);
  free(buf);
}

static buffer_t* _buffer_new(uint32_t size) {
  buffer_t* buf = (buffer_t*)calloc(1, sizeof(buffer_t));
  if (buf) {
    // fprintf(stderr, "malloc buffer %p.\n", buf);
    buf->data = (uint8_t*)malloc(size);
    if (!buf->data) {
      _buffer_free(buf);
      buf = 0;
    } else {
      buf->size = size;
    }
  }
  return buf;
}

static int _buffer_wrap_init_buffer(buffer_wrap_t* buf_wrap, buffer_t* buf) {
  if (!buf_wrap || !buf) return -22;  // -EINVAL

  buf_wrap->buf = buf;
  buf_wrap->data = buf->data;
  buf_wrap->size = buf->size;
  return 0;
}

buffer_wrap_t* buffer_wrap_default_new() {
  buffer_wrap_t* buf_wrap = (buffer_wrap_t*)calloc(1, sizeof(buffer_wrap_t));
  return buf_wrap;
}

buffer_wrap_t* buffer_wrap_new(uint32_t size) {
  buffer_wrap_t* buf_wrap = buffer_wrap_default_new();
  if (buf_wrap) {
    buffer_t* buf = _buffer_new(size);
    if (!buf) {
      buffer_wrap_free(buf_wrap);
      buf_wrap = 0;
    } else {
      _buffer_wrap_init_buffer(buf_wrap, buf);
    }
  }

  return buf_wrap;
}

int buffer_wrap_init_span(buffer_wrap_t* buf_wrap, uint8_t* data,
                          uint32_t size) {
  if (!buf_wrap) return -22;  // -EINVAL

  buf_wrap->data = data;
  buf_wrap->size = size;
  return 0;
}

void buffer_wrap_clear(buffer_wrap_t* buf_wrap) {
  if (!buf_wrap) return;
  if (buf_wrap->buf) _buffer_free(buf_wrap->buf);
  memset(buf_wrap, 0, sizeof(buffer_wrap_t));
}

void buffer_wrap_free(buffer_wrap_t* buf_wrap) {
  if (!buf_wrap) return;
  buffer_wrap_clear(buf_wrap);
  free(buf_wrap);
}

int buffer_io_wrap_init_default(buffer_io_wrap_t* buffer_io) {
  if (!buffer_io) return -22;  // -EINVAL
  memset(buffer_io, 0, sizeof(buffer_io_wrap_t));
  return 0;
}

int buffer_io_wrap_init_span(buffer_io_wrap_t* buffer_io, uint8_t* data,
                             uint32_t size) {
  buffer_io_wrap_init_default(buffer_io);
  ioc_init(&buffer_io->ioctx, data, size);
  return 0;
}

int buffer_io_wrap_clone(buffer_io_wrap_t* dst, buffer_io_wrap_t* src) {
  if (!dst || !src) return -22;  // -EINVAL
  if (dst->buf) _buffer_free(dst->buf);
  dst->buf = 0;
  dst->ioctx = src->ioctx;
  return 0;
}
