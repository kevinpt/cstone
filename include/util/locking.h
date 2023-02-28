#ifndef LOCKING_H
#define LOCKING_H

/* SPDX-License-Identifier: MIT
Copyright 2023 Kevin Thibedeau
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

/*
------------------------------------------------------------------------------
This implements simple coarse-grained locks meant to be used briefly in critical
sections. Attempting to hold a lock for extended time frames will likely result in
bad system behavior.

There are currently four possible lock implementations. They are listed in order of
preference:

  1) Disable interrupts (FreeRTOS only)
  2) Single-threaded with no locking (requires USE_SINGLE_THREAD)
  3) Thread mutex (platforms with pthreads)
  4) Spinlock (platforms with C11 stdatomic support)


The mutex and spinlock implementations require a lock object to be instantiated. For
pthreads this will be a `pthread_mutex_t`. For the spinlock it will be `atomic_flag`.
Use the LOCK_INIT(lock) macro to initialize it. For the other implementations you can
pass in a dummy value for the lock object if necessary. You can test for the selected
implementations with lock objects by checking if USE_PTHREAD_LOCK or USE_ATOMIC_SPINLOCK
are defined by this header. Do not define these yourself.

There are configuration macros you can use to influence the selection of the lock
implementation:

  * USE_FREERTOS      - Define this to lock by disabling interrupts using the FreeRTOS API
  * USE_SINGLE_THREAD - Permit lock free implementation on single-threaded systems

------------------------------------------------------------------------------
*/


#if (!defined PLATFORM_HAS_ATOMICS && defined __STDC_VERSION__ && __STDC_VERSION__ >= 201112L \
    && !defined __STDC_NO_ATOMICS__)
#  define PLATFORM_HAS_ATOMICS
#endif

#if defined USE_FREERTOS
#  include "FreeRTOS.h"
#  include "task.h"

#else
#  if defined __unix__ || defined __MACH__
#    include <pthread.h>
#    define USE_PTHREAD_LOCK
#  elif defined PLATFORM_HAS_ATOMICS
#    include <stdatomic.h>
#    define USE_ATOMIC_SPINLOCK
#  endif
#endif


// Select lock implementation

#if defined USE_FREERTOS
// Critical sections (disable interrupts)
#  include "FreeRTOS.h"
#  include "task.h"
#  define LOCK_INIT(lock)
#  define LOCK_TAKE(lock)     taskENTER_CRITICAL()
#  define LOCK_RELEASE(lock)  taskEXIT_CRITICAL()

#elif defined USE_SINGLE_THREAD
// Locking not needed
#  define LOCK_INIT(lock)
#  define LOCK_TAKE(lock)
#  define LOCK_RELEASE(lock)

#elif defined USE_PTHREAD_LOCK
// Pthreads
#  define LOCK_INIT(lock)     pthread_mutex_init((lock), NULL)
#  define LOCK_TAKE(lock)     pthread_mutex_lock(lock)
#  define LOCK_RELEASE(lock)  pthread_mutex_unlock(lock)

#elif defined PLATFORM_HAS_ATOMICS
// Portable C11 spinlock
#  define LOCK_INIT(lock)     spinlock_init(lock)
#  define LOCK_TAKE(lock)     spinlock_lock(lock)
#  define LOCK_RELEASE(lock)  spinlock_unlock(lock)

#else
#  error "No available lock implementation"
#endif




#ifdef __cplusplus
extern "C" {
#endif
// Spinlock as a portable, OS agnostic way to guarantee mutual exclusion.
// This should be a last resort when other less portable options are not
// available.
#ifdef USE_ATOMIC_SPINLOCK
static inline void spinlock_init(atomic_flag *spin_lock) {
  atomic_flag_clear(spin_lock);
}


static inline void spinlock_lock(atomic_flag *spin_lock) {
  while(atomic_flag_test_and_set_explicit(spin_lock, memory_order_acquire)) {}
}

static inline void spinlock_unlock(atomic_flag *spin_lock) {
  atomic_flag_clear_explicit(spin_lock, memory_order_release);
}
#endif

#ifdef __cplusplus
}
#endif


#endif // LOCKING_H
