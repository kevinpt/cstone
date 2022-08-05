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

#ifndef MINMAX_H
#define MINMAX_H

#include <stdint.h>

// ******************** min() ********************

static inline unsigned char min_uc(unsigned char a, unsigned char b) {
  return a <= b ? a : b;
}

static inline signed char min_sc(signed char a, signed char b) {
  return a <= b ? a : b;
}

static inline char min_c(char a, char b) {
  return a <= b ? a : b;
}


static inline unsigned short min_us(unsigned short a, unsigned short b) {
  return a <= b ? a : b;
}

static inline short min_ss(short a, short b) {
  return a <= b ? a : b;
}

static inline unsigned min_ui(unsigned a, unsigned b) {
  return a <= b ? a : b;
}

static inline int min_si(int a, int b) {
  return a <= b ? a : b;
}

static inline unsigned long min_ul(unsigned long a, unsigned long b) {
  return a <= b ? a : b;
}

static inline long min_sl(long a, long b) {
  return a <= b ? a : b;
}


static inline uint8_t min_u8(uint8_t a, uint8_t b) {
  return a <= b ? a : b;
}

static inline int8_t min_s8(int8_t a, int8_t b) {
  return a <= b ? a : b;
}


static inline uint16_t min_u16(uint16_t a, uint16_t b) {
  return a <= b ? a : b;
}

static inline int16_t min_s16(int16_t a, int16_t b) {
  return a <= b ? a : b;
}

static inline uint32_t min_u32(uint32_t a, uint32_t b) {
  return a <= b ? a : b;
}

static inline int32_t min_s32(int32_t a, int32_t b) {
  return a <= b ? a : b;
}

static inline uint64_t min_u64(uint64_t a, uint64_t b) {
  return a <= b ? a : b;
}

static inline int64_t min_s64(int64_t a, int64_t b) {
  return a <= b ? a : b;
}




// ******************** max() ********************

static inline unsigned char max_uc(unsigned char a, unsigned char b) {
  return a >= b ? a : b;
}

static inline signed char max_sc(signed char a, signed char b) {
  return a >= b ? a : b;
}

static inline char max_c(char a, char b) {
  return a >= b ? a : b;
}


static inline unsigned short max_us(unsigned short a, unsigned short b) {
  return a >= b ? a : b;
}

static inline short max_ss(short a, short b) {
  return a >= b ? a : b;
}

static inline unsigned max_ui(unsigned a, unsigned b) {
  return a >= b ? a : b;
}

static inline int max_si(int a, int b) {
  return a >= b ? a : b;
}

static inline unsigned long max_ul(unsigned long a, unsigned long b) {
  return a >= b ? a : b;
}

static inline long max_sl(long a, long b) {
  return a >= b ? a : b;
}


static inline uint8_t max_u8(uint8_t a, uint8_t b) {
  return a >= b ? a : b;
}

static inline int8_t max_s8(int8_t a, int8_t b) {
  return a >= b ? a : b;
}


static inline uint16_t max_u16(uint16_t a, uint16_t b) {
  return a >= b ? a : b;
}

static inline int16_t max_s16(int16_t a, int16_t b) {
  return a >= b ? a : b;
}

static inline uint32_t max_u32(uint32_t a, uint32_t b) {
  return a >= b ? a : b;
}

static inline int32_t max_s32(int32_t a, int32_t b) {
  return a >= b ? a : b;
}

static inline uint64_t max_u64(uint64_t a, uint64_t b) {
  return a >= b ? a : b;
}

static inline int64_t max_s64(int64_t a, int64_t b) {
  return a >= b ? a : b;
}



#if __STDC_VERSION__ >= 201112L

#  define min(a, b)  _Generic((a), \
      unsigned char  : min_uc,  \
      signed char    : min_sc,  \
      char      : min_c,   \
      unsigned short  : min_us,  \
      short     : min_ss,  \
      unsigned  : min_ui,  \
      int       : min_si,  \
      unsigned long  : min_ul,  \
      long      : min_sl   \
    )((a), (b))

#  define max(a, b)  _Generic((a), \
      unsigned char  : max_uc,  \
      signed char    : max_sc,  \
      char      : max_c,   \
      unsigned short  : max_us,  \
      short     : max_ss,  \
      unsigned  : max_ui,  \
      int       : max_si,  \
      unsigned long  : max_ul,  \
      long      : max_sl   \
    )((a), (b))

#endif


#endif // MINMAX_H
