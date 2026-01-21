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
 * @file iorw.h
 * @brief iorw APIs.
 * @version 0.1
 * @date Created 19/02/2025
 **/

#ifndef __IO_RW_H__
#define __IO_RW_H__

#include <stdint.h>
#include <stdio.h>

typedef struct IOContext {
  uint8_t* buffer;
  uint32_t size;
  uint32_t idx;
} io_context_t;

typedef struct BitsIOContext {
  io_context_t* ioc;
  uint32_t bit_idx;
  uint32_t byte;
} bits_io_context_t;

void ioc_init(io_context_t* ctx, uint8_t* buffer, uint32_t size);
uint32_t ioc_tell(io_context_t* ctx);
uint32_t ioc_remain(io_context_t* ctx);

uint32_t iow_write(io_context_t* ctx, const uint8_t* data, uint32_t n);
uint32_t iow_8(io_context_t* ctx, uint32_t value);
uint32_t iow_b16(io_context_t* ctx, uint32_t value);
uint32_t iow_b32(io_context_t* ctx, uint32_t value);
uint32_t iow_b64(io_context_t* ctx, uint64_t value);
uint32_t iow_leb128(io_context_t* ctx, uint64_t value);

uint32_t ior_read(io_context_t* ctx, uint8_t* data, uint32_t n);
uint32_t ior_8(io_context_t* ctx);
uint32_t ior_b16(io_context_t* ctx);
uint32_t ior_b24(io_context_t* ctx);
uint32_t ior_b32(io_context_t* ctx);
uint64_t ior_b64(io_context_t* ctx);
uint64_t ior_leb128(io_context_t* ctx);
uint32_t ior_leb128_u32(io_context_t* ctx);
uint32_t ior_expandable(io_context_t* ctx);
uint32_t ior_skip(io_context_t* ctx, uint32_t n);
uint32_t ior_string(io_context_t* ctx, char* str, uint32_t n);

void bits_ioc_init(bits_io_context_t* bits_ctx, io_context_t* ctx);
uint32_t bits_ior_le32(bits_io_context_t* ctx, uint32_t n);

#endif  // __IO_RW_H__