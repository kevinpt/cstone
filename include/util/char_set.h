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

/*
------------------------------------------------------------------------------
char_set

This provides facilities for managing character sets. It has a fast access bitmap
CharSet suitable for 8-bit characters only and routines for managing arrays of
character ranges CharRange that can be used with Unicode code points when
USE_UNICODE is defined.
------------------------------------------------------------------------------
*/

#ifndef CHAR_SET_H
#define CHAR_SET_H

#ifdef USE_UNICODE
typedef uint32_t Codepoint;
#define CPOINT_MAX  UINT32_MAX
#else
typedef unsigned char Codepoint;
#define CPOINT_MAX  UCHAR_MAX
#endif


// Inclusive range of characters
typedef struct {
  Codepoint low;
  Codepoint high;
} CharRange;

// Range of a single char
#define ONE_CHAR(c)  {(c), (c)}
#define END_RANGE    {'\0', '\0'}


// Bit map of chars in a set
typedef struct {
  uint32_t char_blocks[8]; // Set of 256 bits for each char
} CharSet;

// CharSet capped at 256 8-bit chars (ASCII, Latin-1, etc.)
#define CSET_MAX  255




#ifdef __cplusplus
extern "C" {
#endif

// ******************** Sequences ********************

size_t char_seq_init(char *seq, CharSet *cset);


// ******************** Character sets (8-bit only, no Unicode) ********************

/*
Initialize an empty character set

Args:
  cset:     Set to initialize
*/
static inline void cset_init(CharSet *cset) {
  memset(cset, 0, sizeof(*cset));
}

/*
Count members in a character set

Args:
  cset:     Set to count from

Returns:
  Number of characters in the set
*/
static inline size_t cset_count(CharSet *cset) {
  return char_seq_init(NULL, cset);
}

bool cset_init_from_crange(CharSet *cset, CharRange ranges[]);
void cset_init_from_seq(CharSet *cset, unsigned char *seq);

/*
Add a single character to a set

Args:
  cset: Character set to add to
  ch:   Character to add
*/
static inline void cset_add_char(CharSet *cset, unsigned char ch) {
  cset->char_blocks[ch / 32] |= (1ul << (ch % 32));
}

/*
Remove a single character from a set

Args:
  cset: Character set to delete from
  ch:   Character to delete
*/
static inline void cset_del_char(CharSet *cset, unsigned char ch) {
  cset->char_blocks[ch / 32] &= ~(1ul << (ch % 32));
}

/*
Test if a character is in a set

Args:
  cset: Character set to check
  ch:   Character to test for

Returns:
  true if character is in set
*/
static inline bool cset_has_char(CharSet *cset, unsigned char ch) {
  return cset->char_blocks[ch / 32] & (1ul << (ch % 32));
}

bool cset_is_subset(CharSet *cset, CharSet *elements);
void cset_merge(CharSet *cset, CharSet *elements);
void cset_remove(CharSet *cset, CharSet *elements);


// ******************** Character ranges ********************

void crange_init_from_cset(CharRange **ranges, CharSet *cset);
size_t crange_count(CharRange ranges[]);
bool crange_has_char(CharRange ranges[], size_t ranges_len, Codepoint ch);
void crange_sort(CharRange ranges[], size_t ranges_len);
size_t crange_condense(CharRange ranges[], size_t ranges_len);
CharRange *crange_merge(CharRange *rng1, CharRange *rng2);
void crange_dump(CharRange *ranges);

#ifdef __cplusplus
}
#endif

#endif // CHAR_SET_H

