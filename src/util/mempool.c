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

#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <ctype.h>

#include "locking.h"
#include "list_ops.h"

#include "range_strings.h"
#include "intmath.h"
#include "mempool.h"

#define MP_FLAG_STATIC   0x01
#define MP_FLAG_DISABLED 0x02

// Round up object size to match alignment of a type
#define ROUND_UP_ALIGN(n, alignment)  (((uintptr_t)(n) + (alignment)-1) & ~((uintptr_t)(alignment) - 1))
#define ALIGN_PTR(p, alignment) (void *)ROUND_UP_ALIGN((p), (alignment))

#define SENTINEL_VALUE(chunk) ((uintptr_t)(chunk)->next ^ 0xa5a5a5a5)


// Newlib doesn't support printf() %zu specifier
#ifdef linux  // x64
#  define PRIuz "zu"
#  define PRIXz "zX"
#else // Newlib on ARM
// arm-none-eabi has size_t as "unsigned int" but uint32_t is "long unsigned int". Crazy
#  define PRIuz "u"
#  define PRIXz "X"
#endif


typedef struct mpPoolChunk_s {
  struct mpPoolChunk_s *next;
#ifdef USE_MP_POINTER_CHECK
  uintptr_t sentinel;
#endif
} mpPoolChunk;



// Check if arg is a power of 2
static inline bool is_power_of_2(size_t n) {
  return (n & (n - 1)) == 0;
}



// ******************** Internal link operations ********************

// Get next pool
static inline mpPool *mp__next(mpPool *pool) {
  return pool->next;
}

// Get previous pool
static inline mpPool *mp__prev(mpPoolSet *pool_set, mpPool *pool) {
  return LL_NODE(ll_slist_find_prev(pool_set->pools, pool), mpPool, next);
}

// Link a new pool to its predecessor
static inline void mp__link(mpPool *prev, mpPool *pool) {
  ll_slist_add_after(&prev, pool);
}

// Link a new pool to the list head
static inline void mp__link_head(mpPoolSet *pool_set, mpPool *pool) {
  ll_slist_push(&pool_set->pools, pool);
}


// Unlink any pool
static inline void mp__unlink(mpPoolSet *pool_set, mpPool *pool) {
  ll_slist_remove(&pool_set->pools, pool);
}

// Unlink a pool from its predecessor
static inline void mp__unlink_from_prev(mpPool *prev, mpPool *pool) {
  ll_slist_remove_after(prev, pool);
}



static mpPoolSet *s_sys_pool_set = NULL;

// ******************** Resource management ********************

/*
Initialize a pool set
Args:
  pool_set : Set object to initialize
Returns:
  Initialized pool set with no internal pools
*/
void mp_init_pool_set(mpPoolSet *pool_set) {
  if(!pool_set) return;
  pool_set->pools = NULL;

  if(!s_sys_pool_set)
    s_sys_pool_set = pool_set;

#ifdef USE_MP_COLLECT_STATS
//  pool_set->hist = histogram_init(20, 0, 50, /* track_overflow */ true);
#endif
}


mpPoolSet *mp_sys_pools(void) {
  return s_sys_pool_set;
}



#ifdef USE_MP_COLLECT_STATS
void mp_add_histogram(mpPoolSet *pool_set, Histogram *hist) {
  pool_set->hist = hist;
}
#endif


/*
Release a pool from memory
Args:
  pool_set       : Set containing the pool
  pool           : Pool to release
  release_in_use : When true, pools with allocated elements will also be released
Returns:
  true if the pool was released
*/
bool mp_release_pool(mpPoolSet *pool_set, mpPool *pool, bool release_in_use) {
  bool release = release_in_use || !mp_pool_in_use(pool);

  if(release) {
    mp__unlink(pool_set, pool);

    if(!(pool->flags & MP_FLAG_STATIC)) // Deallocate dynamic pools
      free(pool);
  }

  return release;
}


/*
Release all pools in a pool set
Args:
  pool_set       : Set object to release
  release_in_use : When true, pools with allocated elements will also be released
Returns:
  true if all pools were released
*/
bool mp_release_pool_set(mpPoolSet *pool_set, bool release_in_use) {
  mpPool *prev, *cur, *next;
  bool release;
  bool release_all = true;

  if(!pool_set) return false;

  prev = NULL;
  cur = pool_set->pools;

  ENTER_CRITICAL();
    while(cur) {
      release = release_in_use || !mp_pool_in_use(cur);

      if(release) {
        next = mp__next(cur);

        // Unlink this pool
        if(prev) {
          mp__unlink_from_prev(prev, cur);
        } else { // Head of list
          mp__unlink(pool_set, cur);
        }

        if(!(cur->flags & MP_FLAG_STATIC)) // Deallocate dynamic pools
          free(cur);

        cur = next;
      } else { // Keeping this pool
        prev = cur;
        cur = mp__next(cur);
        release_all = false;
      }
    };
  EXIT_CRITICAL();
#ifdef USE_MP_COLLECT_STATS
  if(pool_set->hist) {
    free(pool_set->hist);
    pool_set->hist = NULL;
  }
#endif

  return release_all;
}


/*
Initialize a memory pool
Args:
  pool         : Pool object to initialize
  elements     : Number of elements in the pool
  element_size : Size of each element
  alignment    : Alignment in bytes for each element
Returns:
  Pool with initialized structure
*/
static inline void mp__init_pool(mpPool *pool, size_t elements, size_t element_size, size_t alignment) {
  memset(pool, 0, sizeof(*pool));
  pool->next = NULL;
  pool->pool_begin = ALIGN_PTR(pool->elements, alignment);
  pool->pool_end = (void *)((uintptr_t)pool->pool_begin + elements*element_size);
  pool->element_size = element_size;
#ifdef USE_MP_COLLECT_STATS
  pool->free_elems = elements;
  pool->min_free_elems = elements;
  stats_init(&pool->req_size, 8);
#endif
  pool->flags = 0;

  // Populate free list
  mpPoolChunk *cur = (mpPoolChunk *)pool->pool_begin;
  while(1) {
    cur->next = (mpPoolChunk *)((uint8_t *)cur + element_size);
#ifdef USE_MP_POINTER_CHECK
    cur->sentinel = SENTINEL_VALUE(cur);
#endif
    if((void *)cur->next >= pool->pool_end) { // Overran the pool
      cur->next = NULL;
#ifdef USE_MP_POINTER_CHECK
      cur->sentinel = SENTINEL_VALUE(cur);
#endif
      break;
    }
    cur = cur->next;
  }
  pool->free_list = (mpPoolChunk *)pool->pool_begin;
}


/*
Create a dynamically allocated memory pool

The ``element_size`` argument should be large enough to hold at least two pointers
on the target architecture when ``USE_MP_POINTER_CHECK`` is enabled or one pointer
if it's disabled.

Args:
  elements     : Number of elements in the pool
  element_size : Size of each element (Must be large enough to contain an mpPoolChunk object)
  alignment    : Alignment in bytes for each element (Must be a power of 2)
Returns:
  A newly allocated pool or NULL on failure
*/
mpPool *mp_create_pool(size_t elements, size_t element_size, size_t alignment) {
  assert(is_power_of_2(alignment) && alignment >= 1);
  assert(element_size >= sizeof(mpPoolChunk));

  // Adjust element size to be a multiple of alignment
  element_size = ROUND_UP_ALIGN(element_size, alignment);

  mpPool *pool = (mpPool *)malloc(sizeof(mpPool) + alignment-1 + elements*element_size);
  if(!pool) return NULL;
  mp__init_pool(pool, elements, element_size, alignment);
  return pool;
}


/*
Create a statically allocated memory pool

The static buffer should be allocated as follows:

.. code-block:: c

  alignas(mpPool)
  static uint8_t pool_buf[NUM_ELEMENTS * ELEMENT_SIZE + MP_STATIC_PADDING(alignment)];

Note that the pool will contain fewer elements than specified if ``element_size``
is not a multiple of ``alignment``. When this is the case ``element_size`` is
rounded up to the nearest multiple so that each element in the ``buf`` array has
the correct alignment.

If the ``buf`` argument is not aligned to match that of mpPool then it will be
realigned with additional loss of storage space.

The ``element_size`` argument should be large enough to hold at least two pointers
on the target architecture when ``USE_MP_POINTER_CHECK`` is enabled or one pointer
if it's disabled.

Args:
  buf          : Static buffer to contain the pool
  buf_len      : Size of buf
  element_size : Size of each element (Must be large enough to contain an mpPoolChunk object)
  alignment    : Alignment in bytes for each element (Must be a power of 2)
Returns:
  An initialized pool contained within the buffer or NULL on failure
*/
mpPool *mp_create_static_pool(uint8_t *buf, size_t buf_len, size_t element_size, size_t alignment) {
  assert(is_power_of_2(alignment) && alignment >= 1);
  assert(element_size >= sizeof(mpPoolChunk));

  if(!buf) return NULL;

  // Align the buffer to the required alignment of mpPool
  uint8_t *buf_aligned = ALIGN_PTR(buf, _Alignof(mpPool));
  // Shrink buf_len to compensate for space lost due to alignment
  buf_len -= buf_aligned - buf;
  buf = buf_aligned;

  // Adjust element size to be a multiple of alignment
  element_size = ROUND_UP_ALIGN(element_size, alignment);

  // Determine number of elements in buf
  // We reserve space at the beginning for an mpPool struct along with padding to
  // achieve the requested element alignment.
  size_t elements = (buf_len - (sizeof(mpPool) + alignment - 1)) / element_size;

  mpPool *pool = (mpPool *)buf;
  mp__init_pool(pool, elements, element_size, alignment);
  pool->flags |= MP_FLAG_STATIC;
  return pool;
}


/*
Add a pool to a pool set
Args:
  pool_set : Set to contain the pool
  new_pool : Pool to add into the set
Returns:
  pool_set with new_pool added to its pool list
*/
void mp_add_pool(mpPoolSet *pool_set, mpPool *new_pool) {
  mpPool * cur;
  mpPool *prev = NULL;

  if(!pool_set || !new_pool) return;

#ifdef USE_MP_COLLECT_STATS
  if(pool_set->hist && new_pool->element_size > (size_t)pool_set->hist->bin_high) {
    histogram_set_bounds(pool_set->hist, 0, new_pool->element_size);
  }
#endif


  ENTER_CRITICAL();
    if(!pool_set->pools) { // Empty pool set
      pool_set->pools = new_pool;

    } else {
      // Pool list must be kept sorted in order from smallest to largest element size
      for(cur = pool_set->pools; cur; cur = mp__next(cur)) {
        if(cur->element_size > new_pool->element_size) { // Insert new pool before cur
          if(prev) {
            mp__link(prev, new_pool);
          } else { // Start of list
            mp__link_head(pool_set, new_pool);
          }
          EXIT_CRITICAL();
          return;
        }
        prev = cur;
      }

      // Reached end without inserting pool
      mp__link(prev, new_pool);
    }
  EXIT_CRITICAL();
}


// ******************** Object allocation ********************

// Allocate an element from a pool
static inline mpPoolChunk *mp__take_pool_element(mpPool *pool, size_t *alloc_size) {
  mpPoolChunk *elem = pool->free_list;
  pool->free_list = elem->next;
#ifdef USE_MP_COLLECT_STATS
  pool->free_elems--;
  if(pool->free_elems < pool->min_free_elems)
    pool->min_free_elems = pool->free_elems;
#endif
  if(alloc_size)
    *alloc_size = pool->element_size;

  return elem;
}


/*
Retrieve an element from a pool
Args:
  pool_set   : Set to allocate from
  size       : Size of the desired element
  alloc_size : Size of the allocated element. Will be at least size or larger
Returns:
  An allocated element or NULL on failure, alloc_size contains the size of the element
*/
void *mp_alloc(mpPoolSet *pool_set, size_t size, size_t *alloc_size) {
  mpPoolChunk *alloc = NULL;
  mpPool *cur;

#ifdef USE_MP_COLLECT_STATS
  histogram_add_sample(pool_set->hist, (int32_t)size);
#endif

  if(alloc_size)
    *alloc_size = 0;

  if(!pool_set) return NULL;

  ENTER_CRITICAL();
    // Search for non-empty pool with elements >= size
    for(cur = pool_set->pools; cur; cur = mp__next(cur)) {
      if(cur->free_list && cur->element_size >= size && !(cur->flags & MP_FLAG_DISABLED)) {
        alloc = mp__take_pool_element(cur, alloc_size);
        break;
      }
    }
  EXIT_CRITICAL();

#ifdef USE_MP_COLLECT_STATS
  if(alloc)
    stats_add_sample(&cur->req_size, size);
#endif

  return (void *)alloc;
}

/*
Retrieve an element from a pool with specified alignment
Args:
  pool_set   : Set to allocate from
  size       : Size of the desired element
  alloc_size : Size of the allocated element. Will be at least size or larger
  alignment  : Required alignment of the allocated element
Returns:
  An allocated element or NULL on failure, alloc_size contains the size of the element
*/
void *mp_alloc_aligned(mpPoolSet *pool_set, size_t size, size_t *alloc_size, size_t alignment) {
  mpPoolChunk *alloc = NULL;
  mpPool *cur;

#ifdef USE_MP_COLLECT_STATS
  histogram_add_sample(pool_set->hist, (int32_t)size);
#endif

  if(alloc_size)
    *alloc_size = 0;

  if(!pool_set) return NULL;

  ENTER_CRITICAL();
    // Search for non-empty pool with elements >= size
    for(cur = pool_set->pools; cur; cur = mp__next(cur)) {
      if(cur->free_list && cur->element_size >= size && !(cur->flags & MP_FLAG_DISABLED)) {
        // Check alignment of this pool
        mpPoolChunk *aligned = (mpPoolChunk *)ALIGN_PTR(cur->free_list, alignment);
        if(cur->free_list == aligned) {
          alloc = mp__take_pool_element(cur, alloc_size);
          break;
        }
      }
    }
  EXIT_CRITICAL();

#ifdef USE_MP_COLLECT_STATS
  if(alloc)
    stats_add_sample(&cur->req_size, size);
#endif

  return (void *)alloc;
}


/*
Retrieve an element from a pool with largest available size
Args:
  pool_set   : Set to allocate from
  size       : Size of the desired element
  alloc_size : Size of the allocated element. Can be smaller than size
Returns:
  An allocated element or NULL on failure, alloc_size contains the size of the element
*/
void *mp_alloc_best_effort(mpPoolSet *pool_set, size_t size, size_t *alloc_size) {
  mpPoolChunk *alloc = NULL;
  mpPool *cur, *alloc_pool = NULL;

#ifdef USE_MP_COLLECT_STATS
  histogram_add_sample(pool_set->hist, (int32_t)size);
#endif

  if(alloc_size)
    *alloc_size = 0;

  //printf("mp_alloc_best_effort(%lu)\n", size);
  if(!pool_set) return NULL;

  ENTER_CRITICAL();
    // Search for non-empty pool with elements >= size
    for(cur = pool_set->pools; cur; cur = mp__next(cur)) {
      if(cur->free_list && !(cur->flags & MP_FLAG_DISABLED)) {
        alloc_pool = cur; // Keep track of the most recent pool with available elements
        if(cur->element_size >= size) // Found ideal pool
          break;
      }
    }

    if(alloc_pool)
      alloc = mp__take_pool_element(alloc_pool, alloc_size);

  EXIT_CRITICAL();

#ifdef USE_MP_COLLECT_STATS
  if(alloc_pool)
    stats_add_sample(&alloc_pool->req_size, alloc_pool->element_size);
#endif

  return (void *)alloc;
}


typedef atomic_uint mpRefCount;

/*
Retrieve a reference counted element from a pool
Args:
  pool_set   : Set to allocate from
  size       : Size of the desired element
  alloc_size : Size of the allocated element. Will be at least size or larger
Returns:
  An allocated element or NULL on failure, alloc_size contains the size of the element.
  The returned pointer has a ref count of 1. You can increment the count with
  :c:func:`mp_inc_ref` and decrement with :c:func:`mp_free`.
*/
void *mp_alloc_with_ref(mpPoolSet *pool_set, size_t size, size_t *alloc_size) {
  mpPoolChunk *alloc = NULL;
  mpPool *cur;

  size += sizeof(mpRefCount);

#ifdef USE_MP_COLLECT_STATS
  histogram_add_sample(pool_set->hist, (int32_t)size);
#endif

  if(alloc_size)
    *alloc_size = 0;
  size_t real_alloc_size;

  if(!pool_set) return NULL;

  ENTER_CRITICAL();
    // Search for non-empty pool with elements >= size
    for(cur = pool_set->pools; cur; cur = mp__next(cur)) {
      if(cur->free_list && cur->element_size >= size && !(cur->flags & MP_FLAG_DISABLED)) {
        alloc = mp__take_pool_element(cur, &real_alloc_size);
        break;
      }
    }
  EXIT_CRITICAL();

  if(alloc) {
    if(alloc_size)
      *alloc_size = real_alloc_size - sizeof(mpRefCount);
    atomic_init((mpRefCount *)alloc, 1);        // Initial count
    alloc += sizeof(mpRefCount);  // Return pointer following the reference count

#ifdef USE_MP_COLLECT_STATS
    stats_add_sample(&cur->req_size, size);
#endif
  }

  return (void *)alloc;
}


static inline bool mp__is_ref_counted(mpPool *pool, void *element) {
  // Ref counted elements have a pointer offset by sizeof(mpRefCount)
  return (uintptr_t)(element - pool->pool_begin) % pool->element_size == sizeof(mpRefCount);
}


// Find the pool an element belongs to
static inline mpPool *mp__find_pool(mpPoolSet *pool_set, void *element) {
  for(mpPool *cur = pool_set->pools; cur; cur = mp__next(cur)) {
    if(element >= cur->pool_begin && element < cur->pool_end) {
      return cur;
    }
  }
  return NULL;
}


/*
Check if a pool element is reference counted
Args:
  pool_set : Set element is from
  element  : Element to check
Returns:
  true if element has a reference count
*/
bool mp_is_ref_counted(mpPoolSet *pool_set, void *element) {
  if(!pool_set || !element) return false;

  mpPool *pool = mp__find_pool(pool_set, element);
  if(!pool)
    return false;

  return mp__is_ref_counted(pool, element);
}


/*
Resize an existing element from a pool
Args:
  pool_set: Set to allocate from
  element:  Existing element to resize
  size:     New size of the element

Returns:
  An allocated element or NULL on failure. If size is 0 the element is feeed.
  If the element is already large enough it is returned unchanged. Otherwise
  a new element is allocated with a copy of original data.
*/
void *mp_realloc(mpPoolSet *pool_set, void *element, size_t size) {
  if(!pool_set || !element) return 0;

  if(size == 0) {
    mp_free(pool_set, element);
    return NULL;
  }

  // Find the pool for this element
  mpPool *pool = mp__find_pool(pool_set, element);
  if(!pool)
    return NULL;

  if(mp__is_ref_counted(pool, element))
    size += sizeof(mpRefCount);

  if(pool->element_size >= size)  // Already big enough
    return element;

  size_t alloc_size;
  void *new_element = mp_alloc(pool_set, size, &alloc_size);
  if(!new_element)
    return NULL;

  memcpy(new_element, element, pool->element_size);
  mp_free(pool_set, element);

  return new_element;
}



/*
Increment the reference count on an allocated pool object
Args:
  element:  Object to increment ref count on
*/
void mp_inc_ref(void *element) {
  mpRefCount *ref_count = (mpRefCount *)(element - sizeof(mpRefCount));
  atomic_fetch_add(ref_count, 1);
}


/*
Get the current reference count from a pool object
Args:
  element: Element to retrieve ref count from
Returns:
  Current ref count of element
*/
uint32_t mp_ref_count(void *element) {
  mpRefCount *ref_count = (mpRefCount *)(element - sizeof(mpRefCount));
  return atomic_load(ref_count);
}



/*
Deallocate an element taken from a pool set
Args:
  pool_set : Set to return element to
  element  : Element to free
Returns:
  true on success.
*/
bool mp_free(mpPoolSet *pool_set, void *element) {
  if(!pool_set || !element) return false;

  // Find the pool for this element
  mpPool *pool = mp__find_pool(pool_set, element);
  if(pool) {

    // Check if reference counted
    if(mp__is_ref_counted(pool, element)) {
      mpRefCount *ref_count = (mpRefCount *)(element - sizeof(mpRefCount));
      if(atomic_fetch_sub(ref_count, 1) > 1)
        return true;

      // Ref count is zero so free the element
      element = element - sizeof(mpRefCount);
    }

    // Return element to free list
    mpPoolChunk *chunk = (mpPoolChunk *)element;

    ENTER_CRITICAL();
      chunk->next = pool->free_list;
      pool->free_list = chunk;
#ifdef USE_MP_COLLECT_STATS
      pool->free_elems++;
#endif

#ifdef USE_MP_POINTER_CHECK
      chunk->sentinel = SENTINEL_VALUE(chunk);
#endif
    EXIT_CRITICAL();

    return true;
  }

  //printf("## ERROR: No pool\n");
  return false;
}


/*
Deallocate an element taken from a pool set. Element content is zeroed
Args:
  pool_set : Set to return element to
  element  : Element to free
Returns:
  true on success.
*/
bool mp_free_secure(mpPoolSet *pool_set, void *element) {
  if(!pool_set || !element) return false;

  // Find the pool for this element
  mpPool *pool = mp__find_pool(pool_set, element);
  if(pool) {

    // Check if reference counted
    if(mp__is_ref_counted(pool, element)) {
      mpRefCount *ref_count = (mpRefCount *)(element - sizeof(mpRefCount));
      if(atomic_fetch_sub(ref_count, 1) > 1)
        return true;

      // Ref count is zero so free the element
      element = element - sizeof(mpRefCount);
    }

    memset(element, 0, pool->element_size); // Clear data

    // Return element to free list
    mpPoolChunk *chunk = (mpPoolChunk *)element;

    ENTER_CRITICAL();
      chunk->next = pool->free_list;
      pool->free_list = chunk;
#ifdef USE_MP_COLLECT_STATS
      pool->free_elems++;
#endif

#ifdef USE_MP_POINTER_CHECK
      chunk->sentinel = SENTINEL_VALUE(chunk);
#endif
    EXIT_CRITICAL();

    return true;
  }

  return false;
}



/*
Check if a pointer is from a pool set
Args:
  pool_set : Set to check membership
  element  : Element to check
Returns:
  true if element is in the set
*/
bool mp_from_pool(mpPoolSet *pool_set, void *element) {
  if(!pool_set || !element) return false;

  // Find the pool for this element
  mpPool *pool = mp__find_pool(pool_set, element);
  return pool != NULL;
}


/*
Get the size of a pool element
Args:
  pool_set : Set element is from
  element  : Element to check
Returns:
  size if element is in the set or 0
*/
size_t mp_get_size(mpPoolSet *pool_set, void *element) {
  if(!pool_set || !element) return 0;

  // Find the pool for this element
  mpPool *pool = mp__find_pool(pool_set, element);
  if(!pool)
    return 0;

  return pool->element_size;
}



// ******************** Utility ********************

/*
Disable or enable a pool for allocation
Args:
  pool : pool to configure
  enable : Pool is available when true
Returns:
  None
*/
void mp_pool_enable(mpPool *pool, bool enable) {
  if(enable)
    pool->flags &= ~MP_FLAG_DISABLED;
  else // disable
    pool->flags |= MP_FLAG_DISABLED;
}


/*
Get total elements in a pool
Args:
  pool : Pool to count elements
Returns:
  Number of elements in the pool
*/
size_t mp_total_elements(mpPool *pool) {
  return ((uintptr_t)pool->pool_end - (uintptr_t)pool->pool_begin) / pool->element_size;
}


/*
Get total free elements in a pool
Args:
  pool : Pool to count free elements
Returns:
  Number of free elements in the pool
*/
size_t mp_total_free_elements(mpPool *pool) {
#ifdef USE_MP_COLLECT_STATS
  return pool->free_elems;
#else
  size_t free_count = 0;
  mpPoolChunk *cur = pool->free_list;

  while(cur) {
    free_count++;
    cur = cur->next;
  }

  return free_count;
#endif
}


/*
Check if a pool has allocated elements in use
Args:
  pool : Pool to check
Returns:
  true if any elements from the pool are allocated
*/
bool mp_pool_in_use(mpPool *pool) {
  return mp_total_free_elements(pool) != mp_total_elements(pool);
}

// FIXME: Relocate to new library
// Formatting options for to_si_value():
#define SIF_POW2            0x01  // Scale values by 1024 rather than 1000
#define SIF_SIMPLIFY        0x02  // Remove fraction from larger values
#define SIF_ROUND_TO_CEIL   0x04  // Rounding mode for SIF_SIMPLIFY
#define SIF_TIGHT_UNITS     0x08  // No space between value and prefix
#define SIF_NO_ALIGN_UNITS  0x10  // Skip extra space when there is no prefix
#define SIF_GREEK_MICRO     0x20  // Use UTF-8 µ for micro- prefix
#define SIF_UPPER_CASE_K    0x40  // Use Upper case for kilo- prefix

/*
Format a number into a string with SI prefix for exponent

Args:
  value:        Integer value to format
  value_exp:    Base-10 exponent for value
  buf:          Destination buffer
  buf_size:     Size of buf
  frac_places:  Number of fractional decimal places in output, -1 for max precision
  options:      Formatting option flags

Returns:
  buf pointer
*/
static char *to_si_value(long value, int value_exp, char *buf, size_t buf_size, short frac_places,
                        unsigned short options) {
  char si_prefix;

  // Convert exponent to scaling
  unsigned fp_scale = 1;
  for(int i = 0; i < frac_places; i++) {
    fp_scale *= 10;
  }

  long scaled_v   = to_fixed_si(value, value_exp, fp_scale, &si_prefix, options & SIF_POW2);
  bool negative   = scaled_v < 0;
  unsigned long scaled_v_abs = negative ? -scaled_v : scaled_v;

  // Remove fraction if integer portion >= 10 when there is a prefix
  if((options & SIF_SIMPLIFY) && scaled_v_abs >= 10 * fp_scale && si_prefix != '\0') {
    scaled_v_abs += (options & SIF_ROUND_TO_CEIL) ? fp_scale : fp_scale/2;  // Round up
    scaled_v_abs = (scaled_v_abs / fp_scale) * fp_scale;
  }
  scaled_v = negative ? -scaled_v_abs : scaled_v_abs;

  AppendRange rng;
  range_init(&rng, buf, buf_size);

  if((scaled_v_abs / fp_scale) * fp_scale == scaled_v_abs)  // No fractional part
    frac_places = 0;

  // Format fixed point into string
  range_cat_fixed(&rng, scaled_v, fp_scale, frac_places);
  if(!(options & SIF_TIGHT_UNITS))
    range_cat_char(&rng, ' ');

  if(si_prefix != '\0') {
    if(si_prefix == 'u' && (options & SIF_GREEK_MICRO))
      range_cat_str(&rng, u8"\u00b5"); // µ
    else {
      if(si_prefix == 'k' && (options & SIF_UPPER_CASE_K))
        si_prefix = 'K';
      range_cat_char(&rng, si_prefix);
    }
  } else if(!(options & SIF_NO_ALIGN_UNITS)) {
    range_cat_char(&rng, ' ');  // Add space in place of prefix so unit symbols stay aligned
  }

  return buf;
}


/*
Report on the state of a pool set
Args:
  pool_set : Set to report
Returns:
  Nothing
*/
void mp_summary(mpPoolSet *pool_set) {
  int i = 1;
  for(mpPool *cur = pool_set->pools; cur; cur = mp__next(cur)) {
    // Count elements on free list
    size_t flist_count = 0;
    bool good_free_list = true;
    ENTER_CRITICAL();
      for(mpPoolChunk *elem = cur->free_list; elem; elem = elem->next) {
        flist_count++;
#ifdef USE_MP_POINTER_CHECK
        uintptr_t check = SENTINEL_VALUE(elem);
        if(elem->sentinel != check)
          good_free_list = false;
#endif
      }
    EXIT_CRITICAL();

    printf("\nPool %d (%" PRIuz " B):", i++, cur->element_size);
    if(!good_free_list)
      puts(" CORRUPT");
    else
      puts("");

    char buf[8];
#define TO_SI(v)  to_si_value((v), 0, buf, sizeof buf, 1, SIF_SIMPLIFY | SIF_POW2 | SIF_UPPER_CASE_K)

    size_t total_size = mp_total_elements(cur) * cur->element_size;
    printf("\tTotal:  %6sB\n", TO_SI(total_size));

#ifdef USE_MP_COLLECT_STATS
    size_t free_size = cur->free_elems * cur->element_size;

    printf("\tUsed:   %6sB", TO_SI(total_size - free_size));
    printf("\t\t\tObjects: %" PRIuz " / %" PRIuz "\n",
            mp_total_elements(cur) - cur->free_elems, mp_total_elements(cur));
    printf("\tFree:   %6sB", TO_SI(free_size));
    printf(" (Min %sB)\n", TO_SI(cur->min_free_elems * cur->element_size));

    // Format average request size
    long mean = stats_mean(&cur->req_size);
    int b10_exp;
    mean = to_fixed_base10(mean, stats_fp_scale(&cur->req_size), -1, &b10_exp);
    to_si_value(mean, b10_exp, buf, sizeof buf, 1, SIF_SIMPLIFY | SIF_NO_ALIGN_UNITS | SIF_UPPER_CASE_K);

    printf("\tRequests:%3" PRIuz "    (Avg %sB", cur->req_size.count, buf);

    // Format standard deviation
    long sdev = stats_std_dev(&cur->req_size);
    sdev = to_fixed_base10(sdev, stats_fp_scale(&cur->req_size), -1, &b10_exp);
    to_si_value(sdev, b10_exp, buf, sizeof buf, 1, SIF_SIMPLIFY | SIF_NO_ALIGN_UNITS | SIF_UPPER_CASE_K);
    printf(", SDev. %sB)", buf);

#else
    size_t free_size = flist_count * cur->element_size;
    printf("\tFree:   %6sB", TO_SI(free_size));
#endif

    if(cur->flags) {
      fputs("\tFlags:", stdout);
      if(cur->flags & MP_FLAG_STATIC)
        fputs(" Static", stdout);
      if(cur->flags & MP_FLAG_DISABLED)
        fputs(" Disabled", stdout);
    }
    puts("");
  }
}


/*
Plot a histogram of requests to a pool set
Args:
  pool_set : Set to plot from
Returns:
  Nothing
*/
void mp_plot_stats(mpPoolSet *pool_set) {
#ifdef USE_MP_COLLECT_STATS
  if(pool_set->hist)
    histogram_plot(pool_set->hist, 30);
#endif
}

