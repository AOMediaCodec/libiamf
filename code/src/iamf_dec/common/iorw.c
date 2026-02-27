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
 * @file iorw.c
 * @brief iorw APIs.
 * @version 2.0.0
 * @date Created 19/02/2025
 **/

#include "iorw.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void ioc_init(io_context_t* c, uint8_t* buffer, uint32_t size) {
  c->buffer = buffer;
  c->size = size;
  c->idx = 0;
}

uint32_t ioc_tell(io_context_t* ctx) { return ctx->idx; }
uint32_t ioc_remain(io_context_t* ctx) { return ctx->size - ctx->idx; }

static inline int ioc_precheck(io_context_t* ctx, uint32_t n) {
  return ctx->idx + n <= ctx->size;
}

uint32_t iow_8(io_context_t* ctx, uint32_t value) {
  if (!ioc_precheck(ctx, 1)) return 0;
  ctx->buffer[ctx->idx++] = (uint8_t)(value & 0xff);
  return 1;
}

uint32_t ior_read(io_context_t* ctx, uint8_t* data, uint32_t n) {
  if (!ioc_precheck(ctx, n)) n = ctx->size - ctx->idx;
  memcpy(data, &ctx->buffer[ctx->idx], n);
  ctx->idx += n;
  return n;
}

uint32_t ior_8(io_context_t* ctx) {
  return ioc_precheck(ctx, 1) ? ctx->buffer[ctx->idx++] : 0;
}

uint32_t ior_b16(io_context_t* ctx) {
  uint32_t val = ior_8(ctx) << 8;
  val |= ior_8(ctx);
  return val;
}

uint32_t ior_b24(io_context_t* ctx) {
  uint32_t val = ior_8(ctx) << 16;
  val |= ior_b16(ctx);
  return val;
}

uint32_t ior_b32(io_context_t* ctx) {
  uint32_t val = ior_b16(ctx) << 16;
  val |= ior_b16(ctx);
  return val;
}

uint64_t ior_leb128(io_context_t* ctx) {
  uint64_t val = 0;
  uint8_t byte;
  for (int n = 0; n < 8; n++) {
    byte = ior_8(ctx);
    val |= (((uint64_t)byte & 0x7f) << (n * 7));
    if (!(byte & 0x80)) break;
  }
  return val;
}

uint32_t ior_leb128_u32(io_context_t* ctx) {
  uint64_t val = ior_leb128(ctx);
  if (val > UINT32_MAX) val = UINT32_MAX;
  return val;
}

uint32_t ior_expandable(io_context_t* ctx) {
  uint32_t val = 0;
  uint8_t byte;
  for (uint32_t i = 0; i < 4; i++) {
    byte = ior_8(ctx);
    val <<= 7;
    val |= (byte & 0x7f);
    if (!(byte & 0x80)) break;
  }
  return val;
}

uint32_t ior_skip(io_context_t* ctx, uint32_t n) {
  if (!ioc_precheck(ctx, n)) n = ctx->size - ctx->idx;
  ctx->idx += n;
  return n;
}

uint32_t ior_string(io_context_t* ctx, char* str, uint32_t n) {
  size_t slen = strlen((char*)&ctx->buffer[ctx->idx]);
  uint32_t len = n;
  if (!ioc_precheck(ctx, len)) len = ctx->size - ctx->idx - 1;
  if (slen < len) len = slen;
  if (len == n) --len;
  memcpy(str, &ctx->buffer[ctx->idx], len);
  str[len] = '\0';
  if (!ioc_precheck(ctx, slen + 1))
    ctx->idx = ctx->size;
  else
    ctx->idx += (slen + 1);
  return len + 1;
}

void bits_ioc_init(bits_io_context_t* bits_ctx, io_context_t* ioc) {
  memset(bits_ctx, 0, sizeof(bits_io_context_t));
  bits_ctx->ioc = ioc;
}

#define def_byte_bits 8

uint32_t bits_ior_le32(bits_io_context_t* ctx, uint32_t n) {
  io_context_t* r = ctx->ioc;
  uint32_t bit_remain;
  uint32_t value = 0;

  while (n) {
    bit_remain = def_byte_bits - ctx->bit_idx;
    if (!ctx->bit_idx) ctx->byte = ior_8(r);

    value <<= n < bit_remain ? n : bit_remain;
    if (n < bit_remain) {
      value |= (ctx->byte >> (bit_remain - n)) & ((1 << n) - 1);
      ctx->bit_idx += n;
      break;
    }
    value |= ctx->byte & ((1 << bit_remain) - 1);
    ctx->bit_idx += bit_remain;
    ctx->bit_idx %= def_byte_bits;
    n -= bit_remain;
  }

  return value;
}