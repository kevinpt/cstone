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

#include <stdint.h>

#include "intmath.h"


// __builtin_clz() added in GCC 3.4 and Clang 5
#if (defined __GNUC__ && __GNUC__ >= 4) || (defined __clang__ && __clang_major__ >= 5)
#  define HAVE_BUILTIN_CLZ
#endif

#ifdef HAVE_BUILTIN_CLZ
#  define clz(x)  __builtin_clz(x)
#else
// Count leading zeros
// From Hacker's Delight 2nd ed. Fig 5-12. Modified to support 16-bit ints.
static int clz(unsigned x) {
  static_assert(sizeof(x) <= 4, "clz() only supports a 32-bit or 16-bit argument");
  unsigned y;
  int n = sizeof(x) * 8;

  if(sizeof(x) > 2) { // 32-bit x
    y = x >> 16; if(y) {n -= 16; x = y;}
  }
  y = x >> 8;  if(y) {n -= 8; x = y;}
  y = x >> 4;  if(y) {n -= 4; x = y;}
  y = x >> 2;  if(y) {n -= 2; x = y;}
  y = x >> 1;  if(y) return n - 2;

  return n - x;
}
#endif

uint32_t ceil_pow2(uint32_t x) {
  return 1ul << (32 - clz(x - 1));
}

uint32_t floor_pow2(uint32_t x) {
  return 1ul << (31 - clz(x));
}


/*
Integer base-10 logarithm

From Hacker's Delight 2nd ed. Fig 11-10

Args:
  n: Input to logarithm

Returns:
  floor(log10(n))
*/
unsigned ilog10(uint32_t n) {
  unsigned log;

  // Estimated base-10 log. Produces wrong result for some ranges of values.
  static const uint8_t log10_est[] = {
              // clz()    base-10 range
    9,9,9,    // 0 - 2    (2**32-1 - 2**29)
    8,8,8,    // 3 - 5    (2**29-1 - 2**26)
    7,7,7,    // 6 - 8    (2**26-1 - 2**23)
    6,6,6,6,  // 9 - 12   (2**23-1 - 2**19)
    5,5,5,    // 13 - 15  (2**19-1 - 65536)
    4,4,4,    // 16 - 18  (65535 - 8192)
    3,3,3,3,  // 19 - 22  (8191 - 512)
    2,2,2,    // 23 - 25  (511 - 64)
    1,1,1,    // 26 - 28  (63 - 8)
    0,0,0,0   // 29 - 32  (7 - 0)
  };

  // Thresholds for correcting initial estimate
  static const uint32_t pow10[] = {
    1, 10, 100, 1000, 10000, 100000,
    1000000, 10000000, 100000000, 1000000000
  };

  log = log10_est[clz(n)];
  if(n < pow10[log]) // Apply correction
    log--;

  return log;
}


/*
Integer logarithm with arbitrary base

Args:
  n:    Input to logarithm
  base: Base for the logarithm

Returns:
  floor(log_base_(n))
*/
unsigned ilog_b(unsigned n, unsigned base) {
  unsigned log, residual;

  residual = n;
  log = 0;

  while(residual > base-1) {
    residual = residual / base;
    log++;
  }

  return log;
}


/*
Convert fixed point value to integer

Args:
  fp_value: Value in fixed point representation
  scale:    Scale factor for fp_value

Returns:
  Integral part of fp_value with rounded up fraction removed
*/
unsigned ufixed_to_uint(unsigned fp_value, unsigned scale) {
  fp_value += scale/2;  // Round up
  return fp_value / scale;
}


/*
Convert signed fixed point value to integer

Args:
  fp_value: Value in fixed point representation
  scale:    Scale factor for fp_value

Returns:
  Integral part of fp_value with rounded up fraction removed
*/
int fixed_to_int(int fp_value, unsigned scale) {
  fp_value += scale/2;  // Round up
  return fp_value / scale;
}


