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
 * @file cbuffer.h
 * @brief Circular buffer APIs.
 * @version 0.1
 * @date Created 12/11/2025
 **/

#ifndef __CBUFFER_H__
#define __CBUFFER_H__

#include <stdint.h>

#include "iorw.h"

typedef struct CBuffer buffer_t;

typedef struct CBufferWrap {
  buffer_t* buf;

  uint8_t* data;
  uint32_t size;
} buffer_wrap_t;

typedef struct CBufferIOWrap {
  buffer_t* buf;
  io_context_t ioctx;
} buffer_io_wrap_t;

buffer_wrap_t* buffer_wrap_default_new();
buffer_wrap_t* buffer_wrap_new(uint32_t size);
int buffer_wrap_init_span(buffer_wrap_t* buf_wrap, uint8_t* data,
                          uint32_t size);
void buffer_wrap_clear(buffer_wrap_t* buf_wrap);
void buffer_wrap_free(buffer_wrap_t* buf_wrap);

int buffer_io_wrap_init_default(buffer_io_wrap_t* buf_io_wrap);
int buffer_io_wrap_init_span(buffer_io_wrap_t* buf_io_wrap, uint8_t* data,
                             uint32_t size);
int buffer_io_wrap_clone(buffer_io_wrap_t* dst, buffer_io_wrap_t* src);
#endif  // __CBUFFER_H__
