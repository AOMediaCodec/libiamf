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
 * @file iamf_synchronizer.c
 * @brief IAMF audio synchronizer implementation.
 * @version 2.0.0
 * @date Created 12/11/2025
 **/

#include "iamf_synchronizer.h"

#include <stdlib.h>
#include <string.h>

#include "clog.h"
#include "cqueue.h"
#include "iamf_private_definitions.h"

#undef def_log_tag
#define def_log_tag "IAMF_SYNC"

#define def_syr_str "IAMF Synchronizer"

typedef struct AudioBlocksCache {
  uint32_t id;
  queue_t* audio_blocks;
  uint32_t delay;
  uint32_t skip;
} audio_blocks_cache_t;

/**
 * @brief IAMF audio synchronizer structure
 * Used to synchronize audio data across multiple presentation elements
 */
typedef struct IamfAudioSynchronizer {
  vector_t* audio_blocks_caches;  // vector<audio_blocks_cache_t>
} iamf_synchronizer_t;

static void audio_blocks_cache_delete(audio_blocks_cache_t* cache) {
  if (!cache) return;
  if (cache->audio_blocks)
    queue_free(cache->audio_blocks,
               (func_value_wrap_ptr_free_t)iamf_audio_block_delete);
  def_free(cache);
}

/**
 * @brief Create a new audio blocks cache
 * This function allocates memory for a new audio blocks cache and initializes
 * it
 *
 * @param id ID of the audio cache
 * @return Pointer to the newly created cache, NULL on failure
 */
static audio_blocks_cache_t* audio_blocks_cache_new(uint32_t id) {
  // Allocate memory for new cache
  audio_blocks_cache_t* new_cache = def_mallocz(audio_blocks_cache_t, 1);
  if (!new_cache) {
    def_err_msg_enomem("audio blocks cache", def_syr_str);
    return NULL;
  }

  // Initialize the cache
  new_cache->id = id;
  new_cache->delay = 0;  // Default delay

  // Create queue for audio blocks
  new_cache->audio_blocks = queue_new();
  if (!new_cache->audio_blocks) {
    def_err_msg_enomem("audio blocks queue", def_syr_str);
    def_free(new_cache);
    return NULL;
  }

  return new_cache;
}

static int audio_block_cache_required_data(audio_blocks_cache_t* cache,
                                           iamf_audio_block_t* block) {
  uint32_t offset = 0;
  uint32_t needed_samples = 0;
  int ret = IAMF_OK;

  if (!cache || !block) return IAMF_ERR_BAD_ARG;
  if (queue_is_empty(cache->audio_blocks)) return IAMF_ERR_INVALID_STATE;

  needed_samples = iamf_audio_block_available_samples(block);

  debug("required samples %d for block ID %u", needed_samples, block->id);

  if (block->skip) offset += block->skip;
  if (block->second_skip) offset += block->second_skip;

  do {
    iamf_audio_block_t* cblock = def_value_wrap_type_ptr(
        iamf_audio_block_t, queue_at(cache->audio_blocks, 0));
    uint32_t _offset;

    if (!cblock) {
      warning("Failed to get audio block from cache for ID %u", cache->id);
      ret = IAMF_ERR_INTERNAL;
    }

    _offset = cache->skip + cblock->skip + cblock->second_skip;

    if (cache->skip + needed_samples <
        iamf_audio_block_available_samples(cblock)) {
      iamf_audio_block_partial_copy_data(block, offset, cblock, _offset,
                                         needed_samples);
      cache->skip += needed_samples;
      offset += needed_samples;
      needed_samples = 0;
    } else {
      value_wrap_t v;
      uint32_t available_samples =
          iamf_audio_block_available_samples(cblock) - cache->skip;
      available_samples = def_min(available_samples, needed_samples);
      iamf_audio_block_partial_copy_data(block, offset, cblock, _offset,
                                         available_samples);
      needed_samples -= available_samples;
      cache->skip = 0;
      queue_pop(cache->audio_blocks, &v);
      iamf_audio_block_delete(cblock);
      offset += available_samples;
    }

  } while (!queue_is_empty(cache->audio_blocks) && needed_samples > 0);

  if (ret == IAMF_OK && !needed_samples) {
    if (block->skip) cache->delay += block->skip;
    if (block->second_skip) cache->delay += block->second_skip;
  }

  debug("cache info: ID %u, delay %u, skip %u, blocks %d", cache->id,
        cache->delay, cache->skip, queue_length(cache->audio_blocks));

  return ret;
}

/**
 * @brief Create a new audio synchronizer
 * This function allocates memory for a new synchronizer and initializes it
 *
 * @return Pointer to the newly created synchronizer, NULL on failure
 */
iamf_synchronizer_t* iamf_synchronizer_create(void) {
  iamf_synchronizer_t* synchronizer = def_mallocz(iamf_synchronizer_t, 1);
  if (synchronizer) {
    synchronizer->audio_blocks_caches = vector_new();
    if (!synchronizer->audio_blocks_caches) {
      def_err_msg_enomem("audio block caches", def_syr_str);
      iamf_synchronizer_destroy(synchronizer);
      return 0;
    }
  }

  return synchronizer;
}

/**
 * @brief Destroy an audio synchronizer
 * This function frees all resources associated with the synchronizer
 *
 * @param synchronizer Pointer to the synchronizer to destroy
 */
void iamf_synchronizer_destroy(iamf_synchronizer_t* synchronizer) {
  if (!synchronizer) return;
  if (synchronizer->audio_blocks_caches)
    vector_free(synchronizer->audio_blocks_caches,
                (func_value_wrap_ptr_free_t)audio_blocks_cache_delete);
  def_free(synchronizer);
}

/**
 * @brief Add audio cache to the synchronizer
 * This function adds a new audio cache to the synchronizer for processing
 *
 * @param synchronizer Pointer to the synchronizer
 * @param id ID of the audio cache to add
 * @return 0 on success, error code on failure
 */
int iamf_synchronizer_add_audio_cache(iamf_synchronizer_t* synchronizer,
                                      uint32_t id) {
  // Check for valid synchronizer
  if (!synchronizer) {
    error("Invalid synchronizer pointer");
    return IAMF_ERR_BAD_ARG;
  }

  // Check if a cache with the same ID already exists
  int cache_count = vector_size(synchronizer->audio_blocks_caches);
  for (int i = 0; i < cache_count; ++i) {
    value_wrap_t* val = vector_at(synchronizer->audio_blocks_caches, i);
    if (val && val->ptr) {
      audio_blocks_cache_t* cache = (audio_blocks_cache_t*)val->ptr;
      if (cache->id == id) {
        debug("Audio cache with ID %u already exists", id);
        return IAMF_OK;  // Cache already exists, nothing to do
      }
    }
  }

  // Create new cache using the helper function
  audio_blocks_cache_t* new_cache = audio_blocks_cache_new(id);
  if (!new_cache) {
    error("Failed to create audio cache with ID %u", id);
    return IAMF_ERR_ALLOC_FAIL;
  }

  // Add cache to vector
  value_wrap_t cache_wrap = def_value_wrap_instance_ptr(new_cache);
  if (vector_push(synchronizer->audio_blocks_caches, cache_wrap) < 0) {
    error("Failed to add audio cache to vector");
    audio_blocks_cache_delete(new_cache);
    return IAMF_ERR_INTERNAL;
  }

  debug("Added audio cache with ID %u", id);
  return IAMF_OK;
}

// Helper function to find a cache by ID
static audio_blocks_cache_t* find_cache_by_id(iamf_synchronizer_t* synchronizer,
                                              uint32_t id) {
  if (!synchronizer) return NULL;

  int cache_count = vector_size(synchronizer->audio_blocks_caches);
  for (int i = 0; i < cache_count; ++i) {
    value_wrap_t* val = vector_at(synchronizer->audio_blocks_caches, i);
    if (val && val->ptr) {
      audio_blocks_cache_t* cache = (audio_blocks_cache_t*)val->ptr;
      if (cache->id == id) {
        return cache;
      }
    }
  }
  return NULL;
}

int iamf_synchronizer_sync_audio_blocks(iamf_synchronizer_t* synchronizer,
                                        iamf_audio_block_t* audio_blocks[],
                                        int count) {
  if (!synchronizer || !audio_blocks || count <= 0) {
    error("Invalid parameters");
    return IAMF_ERR_BAD_ARG;
  }

  iamf_audio_block_t default_block;
  memset(&default_block, 0, sizeof(iamf_audio_block_t));

  for (int i = 0; i < count; ++i) {
    if (audio_blocks[i]) {
      default_block.capacity_per_channel =
          def_max(default_block.capacity_per_channel,
                  audio_blocks[i]->capacity_per_channel);
      default_block.num_samples_per_channel =
          def_max(default_block.num_samples_per_channel,
                  audio_blocks[i]->num_samples_per_channel);
      default_block.skip = def_max(default_block.skip, audio_blocks[i]->skip);
      default_block.padding =
          def_max(default_block.padding, audio_blocks[i]->padding);
      default_block.second_skip =
          def_max(default_block.second_skip, audio_blocks[i]->second_skip);
      default_block.second_padding = def_max(default_block.second_padding,
                                             audio_blocks[i]->second_padding);
    }
  }

  debug(
      "Default block: capacity_per_channel=%u, num_samples_per_channel=%u, "
      "skip=%u, padding=%u, second_skip=%u, second_padding=%u",
      default_block.capacity_per_channel, default_block.num_samples_per_channel,
      default_block.skip, default_block.padding, default_block.second_skip,
      default_block.second_padding);

  for (int i = 0; i < count; ++i) {
    iamf_audio_block_t* block = audio_blocks[i];
    iamf_audio_block_t required_block;
    audio_blocks_cache_t* cache;

    if (!block) continue;

    cache = find_cache_by_id(synchronizer, block->id);
    if (!cache) {
      warn("Cache for ID %u not found, skipping block", block->id);
      continue;
    }

    if (queue_is_empty(cache->audio_blocks) &&
        block->num_samples_per_channel ==
            default_block.num_samples_per_channel &&
        block->second_skip == default_block.second_skip &&
        block->skip == default_block.skip)
      continue;

    debug("cache audio block for ID %u", block->id);

    if (block->data) {
      iamf_audio_block_t* copied_block =
          iamf_audio_block_default_new(block->id);
      if (!copied_block) {
        def_err_msg_enomem("copied audio block", def_syr_str);
        continue;
      }

      *copied_block = *block;
      debug(
          "Copied block: capacity_per_channel=%u, num_samples_per_channel=%u,"
          "skip=%u, padding=%u, second_skip=%u, second_padding=%u",
          copied_block->capacity_per_channel,
          copied_block->num_samples_per_channel, copied_block->skip,
          copied_block->padding, copied_block->second_skip,
          copied_block->second_padding);

      if (queue_push(cache->audio_blocks,
                     def_value_wrap_instance_ptr(copied_block)) < 0) {
        error("Failed to add copied block to cache for ID %u", block->id);
        copied_block->data = 0;
        iamf_audio_block_delete(copied_block);
        continue;
      }
    }

    memset(&required_block, 0, sizeof(iamf_audio_block_t));
    if (iamf_audio_block_resize(&required_block,
                                default_block.capacity_per_channel,
                                block->num_channels) != IAMF_OK) {
      def_err_msg_enomem("required audio block", def_syr_str);
      continue;
    }

    required_block.id = block->id;
    iamf_audio_block_samples_info_copy(&required_block, &default_block);

    if (audio_block_cache_required_data(cache, &required_block) != IAMF_OK) {
      warning("Failed to get required data from cache for ID %u", block->id);
      continue;
    }

    *block = required_block;
  }

  return IAMF_OK;
}
