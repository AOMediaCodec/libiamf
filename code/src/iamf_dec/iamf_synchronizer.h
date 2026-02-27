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
 * @file iamf_synchronizer.h
 * @brief IAMF audio synchronizer APIs.
 * @version 2.0.0
 * @date Created 12/11/2025
 **/

#ifndef __IAMF_SYNCHRONIZER_H__
#define __IAMF_SYNCHRONIZER_H__

#include <stdint.h>

#include "cqueue.h"
#include "cvector.h"
#include "iamf_audio_block.h"

typedef struct IamfAudioSynchronizer iamf_synchronizer_t;

/**
 * @brief Create a new audio synchronizer
 * This function allocates memory for a new synchronizer and initializes it
 *
 * @return Pointer to the newly created synchronizer, NULL on failure
 */
iamf_synchronizer_t* iamf_synchronizer_create(void);

/**
 * @brief Destroy an audio synchronizer
 * This function frees all resources associated with the synchronizer
 *
 * @param synchronizer Pointer to the synchronizer to destroy
 */
void iamf_synchronizer_destroy(iamf_synchronizer_t* synchronizer);

/**
 * @brief Add audio cache to the synchronizer
 * This function adds a new audio cache to the synchronizer for processing
 *
 * @param synchronizer Pointer to the synchronizer
 * @param id ID of the audio cache to add
 * @return 0 on success, error code on failure
 */
int iamf_synchronizer_add_audio_cache(iamf_synchronizer_t* synchronizer,
                                      uint32_t id);

/**
 * @brief Synchronize audio blocks
 * This function synchronizes audio blocks across multiple presentation elements
 *
 * @param synchronizer Pointer to the synchronizer
 * @param audio_blocks Array of audio blocks to synchronize
 * @param count Number of audio blocks in the array
 * @return IAMF_OK on success, error code on failure
 */
int iamf_synchronizer_sync_audio_blocks(iamf_synchronizer_t* synchronizer,
                                        iamf_audio_block_t* audio_blocks[],
                                        int count);
#endif  // __IAMF_SYNCHRONIZER_H__
