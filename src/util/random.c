/* SPDX-License-Identifier: MIT
Copyright 2022 Kevin Thibedeau
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
Basic pseudorandom number generator

This is not for applications that require cryptographic security.
------------------------------------------------------------------------------
*/

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <time.h>

#include "random.h"

// https://www.cs.yale.edu/homes/aspnes/pinewiki/C(2f)Randomization.html
// https://en.wikipedia.org/wiki/Xorshift
static inline uint64_t xorshift64(RandomState *state) {
  uint64_t x = state->s;
  x ^= x << 13;
  x ^= x >> 7;
  x ^= x << 17;
  return state->s = x;
}


static inline uint64_t splitmix64(RandomState *state) {
  uint64_t x = (state->s += 0x9E3779B97f4A7C15ul);
  x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ul;
  x = (x ^ (x >> 27)) * 0x94D049BB133111EBul;
  return x ^ (x >> 31);
}



/*
Initialize state for a PRNG

Args:
  state:  PRNG state to initialize
  seed:   Value to initialize state
*/
void random_init(RandomState *state, uint64_t seed) {
  if(seed == 0) {
    seed = 0x123456789ABCDEFul ^ state->s;
    if(seed == 0)
      seed = 0x123456789ABCDEFul;
  }

  state->s = seed;
}


__attribute__((weak))
uint32_t random_from_system(void) {
  return 0;
}


/*
Generate a random seed value from an initial timestamp

This uses an alternate PRNG to prepare a seed value from the provided system time
that can be passed to :c:func:`random_init`.

Args:
  timestamp:  Optional timestamp. Calls time() when 0

Returns:
  New random seed
*/
uint64_t random_seed_from_time(time_t timestamp) {
  static RandomState seed_state = {.s = 0};
  if(seed_state.s == 0) {
    if(timestamp == 0)
      timestamp = time(NULL);
    seed_state.s = timestamp;
  }

  return splitmix64(&seed_state);
}


/*
Hash a string into an integer key
*/
static uint32_t hash__string(const char *str) {
  // djb2 xor hash
  uint32_t h = 5381;

  while(*str != '\0') {
    h = (h + (h << 5)) ^ *str++;
  }

  return h;
}


/*
Generate a random seed value from an string

This uses an alternate PRNG to prepare a seed value from the provided string
that can be passed to :c:func:`random_init`.

Args:
  seed: Source string hashed to establish seed

Returns:
  New random seed
*/
uint64_t random_seed_from_str(const char *seed) {
  uint32_t hash = hash__string(seed);
  return random_seed_from_time((time_t)hash);
}


/*
Generate a random 64-bit integer

Args:
  state:  PRNG state

Returns:
  Random value
*/
uint64_t random_next64(RandomState *state) {
  return xorshift64(state);
}


/*
Generate a random 32-bit integer

Args:
  state:  PRNG state

Returns:
  Random value
*/
uint32_t random_next32(RandomState *state) {
  return (uint32_t)(xorshift64(state) >> 32);
}


/*
Select a random 64-bit integer from an inclusive range

Args:
  state:  PRNG state
  min:    Start of range
  max:    End of range (inclusive)

Returns:
  Random value within range
*/
int64_t random_range64(RandomState *state, int64_t min, int64_t max) {
  if(max < min)
    return min;

  uint64_t span = (max - min) + 1;
  // We will reject any number that isn't within the largest multiple of span
  uint64_t limit = UINT64_MAX - (UINT64_MAX % span);
  uint64_t r;
  do { // Rejection sampling for uniform distribution
    r = xorshift64(state);
  } while(r >= limit);

  return (int64_t)(r % span) + min;
}


/*
Select a random 32-bit integer from an inclusive range

Args:
  state:  PRNG state
  min:    Start of range
  max:    End of range (inclusive)

Returns:
  Random value within range
*/
int32_t random_range32(RandomState *state, int32_t min, int32_t max) {
  if(max < min)
    return min;

  uint32_t span = (max - min) + 1;
  // We will reject any number that isn't within the largest multiple of span
  uint32_t limit = UINT32_MAX - (UINT32_MAX % span);
  uint32_t r;
  do { // Rejection sampling for uniform distribution
    r = xorshift64(state) >> 32;
  } while(r >= limit);

  return (int32_t)(r % span) + min;
}



/*
Populate a buffer with random bytes of data

Args:
  state:      PRNG state
  dest:       Buffer to fill
  dest_size:  Size of buffer
*/
void random_bytes(RandomState *state, uint8_t *dest, size_t dest_size) {
  // Align to 64-bit boundary
  const uintptr_t mask = ~(8ul - 1);

  if(((uintptr_t)dest & mask) != (uintptr_t)dest) { // Not aligned
    uint64_t r = xorshift64(state);
    while(((uintptr_t)dest & mask) != (uintptr_t)dest && dest_size > 0) {
      *dest++ = r & 0xFF;
      r >>= 8;
      dest_size--;
    }
  }

  // Generate 64-bit words
  uint64_t *dest64 = (uint64_t *)dest;
  while(dest_size >= 8) {
    *dest64++ = xorshift64(state);
    dest_size -= 8;
  }

  // Finish remaining unaligned bytes
  if(dest_size > 0) {
    dest = (uint8_t *)dest64;
    uint64_t r = xorshift64(state);
    while(dest_size-- > 0) {
      *dest++ = r & 0xFF;
      r >>= 8;
    }
  }
}


/*
Generate an array of cumulative weights

This converts an array or independent weights into a cumulative distribution for
use by :c:func:`random_weighted_choice`.

Args:
  state:        PRNG state
  weights:      Array of independent weights
  weights_len:  Length of array
  cum_weights:  Cumulative weight array

Returns:
  Cumulative weight array in cum_weights
*/
void random_weights_init(const uint32_t *weights, size_t weights_len, uint32_t *cum_weights) {
  uint32_t accum = 0;
  for(size_t i = 0; i < weights_len; i++) {
    accum += weights[i];
    cum_weights[i] = accum;
  }
}


static inline size_t search_weights(uint32_t key, const uint32_t *cum_weights, size_t cum_weights_len) {
  ptrdiff_t low = 0;
  ptrdiff_t high = cum_weights_len-1;

  while(low < high) {
    ptrdiff_t mid = low + (high-low)/2;
    if(cum_weights[mid] <= key)
      low = mid+1;
    else
      high = mid;
  }

  return low;
}


/*
Select a random index from a weighted array of choices

The probability of each choice is controlled by the weights. The absolute values of the
weights do not matter, only their relative proportions.

  Weights     Cumulative  Result
  {1,1}       {1,2}       50% chance for 0 and 1
  {1,2}       {1,3}       33% for 0, 66% for 1
  {10,20}     {10,30}     Same
  {10,40,50}  {10,50,100} 10% for 0, 40% for 1, 50% for 2

https://eli.thegreenplace.net/2010/01/22/weighted-random-generation-in-python

Args:
  state:            PRNG state
  cum_weights:      Cumulative weight array (from :c:func:`random_weights_init`)
  cum_weights_len:  Length of array

Returns:
  0-based index of the next choice
*/
size_t random_weighted_choice(RandomState *state, const uint32_t *cum_weights, size_t cum_weights_len) {
  uint32_t r = random_range32(state, 0, cum_weights[cum_weights_len-1]-1);
  // Get index for weight 
  return search_weights(r, cum_weights, cum_weights_len);
}



bool random_bool(RandomState *state, unsigned chance, unsigned out_of) {
  if(out_of < 1)  return false;
  if(chance > out_of) chance = out_of;

  uint32_t cum_weights[2] = {out_of - chance, out_of };
  return (bool)random_weighted_choice(state, cum_weights, 2);
}



#ifdef TEST_RANDOM

#ifndef COUNT_OF
#  define COUNT_OF(a) (sizeof(a) / sizeof(*(a)))
#endif

#include <stdio.h>

int main(int argc, char *argv[]) {
  uint32_t w[] = {1,2,1,1,5};
  uint32_t w2[10];

  random_weights_init(w, COUNT_OF(w), w2);

  RandomState s;
//  random_init(&s, 0);
  random_init(&s, random_seed());


  for(uint32_t i = 0; i < w2[COUNT_OF(w)-1]; i++) {
    size_t ix = search_weights(i, w2, COUNT_OF(w));

    printf("%d: %lu (%u, %u)\n", i, ix, w[ix], w2[ix]);
  }

  size_t bins[5] = {0};
  for(int i = 0; i < 1000000; i++) {
    size_t ix = random_weighted_choice(&s, w2, COUNT_OF(w));
    //printf("%d\n", ix);
    bins[ix]++;
  }

  printf("\nHistogram:\n");
  for(uint32_t i = 0; i < COUNT_OF(bins); i++) {
    printf("[%d]: %lu\n", i, bins[i]);
  }


  uint8_t buf[29] = {0};

  //random_init(&s, time(NULL));
  //random_init(&s, random_next64(&s));
  random_init(&s, random_seed());

  puts("Random buf:");
  random_bytes(&s, buf, COUNT_OF(buf));
  for(uint32_t i = 0; i < COUNT_OF(buf); i++) {
    printf("%02X ", buf[i]);
  }
  puts("");

  unsigned true_count = 0;
  unsigned draws = 10000;
  for(unsigned i = 0; i < draws; i++) {
    if(random_bool(&s, 25, 100))
      true_count++;
  }
  printf("Bool:  draws=%u  count=%u (%f)\n", draws, true_count, (double)true_count / draws);

  return 0;
}


#endif
