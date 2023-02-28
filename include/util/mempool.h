/* SPDX-License-Identifier: MIT
Copyright 2021 Kevin Thibedeau
(kevin 'period' thibedeau 'at' gmail 'punto' com)

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice (including the next
paragraph) shall be included in all copies or substantial portions of the
Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.
*/

#ifndef MEMPOOL_H
#define MEMPOOL_H

#include "util/locking.h"

// Configration options
#define USE_MP_COLLECT_STATS
#define USE_MP_POINTER_CHECK

#ifdef USE_MP_COLLECT_STATS
#  include "util/stats.h"
#  include "util/histogram.h"
#endif


typedef struct mpPoolChunk_s mpPoolChunk;

// Pool of allocatable chunk elements
typedef struct mpPool {
  struct mpPool *next;
  void        *pool_begin;
  void        *pool_end;
  mpPoolChunk *free_list;
  size_t      element_size;
#ifdef USE_MP_COLLECT_STATS
  size_t      free_elems;
  size_t      min_free_elems;
  OnlineStats req_size;
#endif
  uint8_t     flags;
  uint8_t     elements[];
} mpPool;

// Container of pool list
typedef struct {
  mpPool *pools;
#ifdef USE_MP_COLLECT_STATS
  Histogram   *hist;
#endif

#if defined USE_PTHREAD_LOCK
  pthread_mutex_t lock;
#elif defined USE_ATOMIC_SPINLOCK
  atomic_flag lock;
#endif
} mpPoolSet;


// Extra padding added to static buffers to guarantee a desired number of
// elements in the pool.
//   EXAMPLE:  static uint8_t pool_buf[1024*20 + MP_STATIC_PADDING(alignof(uintptr_t)];
// This guarantees 20 1KB blocks after init with mp_create_static_pool().
#define MP_STATIC_PADDING(align)  (sizeof(mpPool) + (align) - 1)

#ifdef __cplusplus
extern "C" {
#endif

// ******************** Resource management ********************
void mp_init_pool_set(mpPoolSet *pool_set);
mpPoolSet *mp_sys_pools(void);
#ifdef USE_MP_COLLECT_STATS
void mp_add_histogram(mpPoolSet *pool_set, Histogram *hist);
#endif
bool mp_release_pool(mpPoolSet *pool_set, mpPool *pool, bool release_in_use);
bool mp_release_pool_set(mpPoolSet *pool_set, bool release_in_use);

mpPool *mp_create_pool(size_t elements, size_t element_size, size_t alignment);
mpPool *mp_create_static_pool(uint8_t *buf, size_t buf_len, size_t element_size, size_t alignment);
void mp_add_pool(mpPoolSet *pool_set, mpPool *pool);

// ******************** Object allocation ********************
void *mp_alloc(mpPoolSet *pool_set, size_t size, size_t *alloc_size);
void *mp_alloc_aligned(mpPoolSet *pool_set, size_t size, size_t *alloc_size, size_t alignment);
void *mp_alloc_best_effort(mpPoolSet *pool_set, size_t size, size_t *alloc_size);
void *mp_realloc(mpPoolSet *pool_set, void *element, size_t size);

void *mp_alloc_with_ref(mpPoolSet *pool_set, size_t size, size_t *alloc_size);
void mp_inc_ref(void *element);
uint32_t mp_ref_count(void *element);
bool mp_is_ref_counted(mpPoolSet *pool_set, void *element);

bool mp_free(mpPoolSet *pool_set, void *element);
bool mp_free_secure(mpPoolSet *pool_set, void *element);

bool mp_from_pool(mpPoolSet *pool_set, void *element);
size_t mp_get_size(mpPoolSet *pool_set, void *element);

// ******************** Utility ********************
void mp_pool_enable(mpPool *pool, bool enable);
size_t mp_total_elements(mpPool *pool);
size_t mp_total_free_elements(mpPool *pool);
bool mp_pool_in_use(mpPool *pool);
void mp_summary(mpPoolSet *pool_set);
void mp_plot_stats(mpPoolSet *pool_set);

#ifdef __cplusplus
}
#endif

#endif // MEMPOOL_H
