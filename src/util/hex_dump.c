/* SPDX-License-Identifier: MIT
Copyright 2020 Kevin Thibedeau
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
Routines for dumping the contents of a data buffer.

------------------------------------------------------------------------------
*/

#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>

#include "util/term_color.h"
#include "util/hex_dump.h"


// Newlib doesn't support printf() %zu specifier
#ifdef linux  // x64
#  define PRIuz "zu"
#  define PRIXz "zX"
#else // Newlib on ARM
// arm-none-eabi has size_t as "unsigned int" but uint32_t is "long unsigned int". Crazy
#  define PRIuz "u"
#  define PRIXz "X"
#endif


// Dump a line of hex data
static void hex_dump(size_t line_addr, size_t buf_addr, uint8_t *buf, size_t buf_len,
                     int indent, int addr_size, bool show_ascii, bool ansi_color) {
  size_t offset = buf_addr - line_addr;
  unsigned int line_bytes = 16;

  // Indent line and show address
  if(ansi_color)
    fputs(A_BLU, stdout);

  printf("%*s%0*" PRIXz "  ", indent, " ", addr_size, line_addr);

  if(ansi_color)
    fputs(A_NONE, stdout);


  // Print leading gap
  for(size_t i = 0; i < offset; i++) {
    fputs("   ", stdout);
  }
  // Print data
  bool color_on = false;
  for(size_t i = 0; i < buf_len; i++) {
    if(isgraph(buf[i])) {
      if(ansi_color && !color_on)
        fputs(A_YLW, stdout);
      color_on = true;
      printf("%02X ", buf[i]);
    } else {
      if(ansi_color && color_on)
        fputs(A_NONE, stdout);
      color_on = false;
      printf("%02X ", buf[i]);
    }
  }
  if(ansi_color && color_on)
    fputs(A_NONE, stdout);


  // Print trailing gap
  if(buf_len + offset < line_bytes) {
    offset = line_bytes - (buf_len + offset);

    for(size_t i = 0; i < offset; i++) {
      fputs("   ", stdout);
    }
  }

  if(show_ascii) {
    if(ansi_color)
      fputs(A_GRN " |" A_NONE, stdout);
    else
      fputs(" |", stdout);

    offset = buf_addr - line_addr;
    // Print leading gap
    for(size_t i = 0; i < offset; i++) {
      fputs(" ", stdout);
    }
    // Print data
    color_on = false;
    for(size_t i = 0; i < buf_len; i++) {
      if(isgraph(buf[i])) {
        if(ansi_color && !color_on)
          fputs(A_YLW, stdout);
        color_on = true;

        printf("%c", buf[i]);
      } else {
        if(ansi_color && color_on)
          fputs(A_NONE, stdout);
        color_on = false;

        printf(".");
      }
    }
    if(ansi_color && color_on)
      fputs(A_NONE, stdout);

    // Print trailing gap
    if(buf_len + offset < line_bytes) {
      offset = line_bytes - (buf_len + offset);

      for(size_t i = 0; i < offset; i++) {
        fputs(" ", stdout);
      }
    }

    if(ansi_color)
      fputs(A_GRN "|" A_NONE, stdout);
    else
      fputs("|", stdout);

  }
  
  printf("\n");
}


#define DEFAULT_INDENT  4
#define ADDR_LEN        4

/*
Dump the contents of a buffer to stdout in hex format

This dumps lines of data with the offset, hex values, and printable ASCII

Args:
  buf        : Buffer to dump
  buf_len    : Length of buf data
  show_ascii : Show table of printable ASCII on right side
  ansi_color : Print dump with color output
*/
void dump_array_ex(uint8_t *buf, size_t buf_len, bool show_ascii, bool ansi_color) {
  size_t buf_pos, buf_count;
  size_t line_addr, line_offset;
  unsigned line_bytes = 16;

  line_addr = 0;
  line_offset = 0;

  buf_pos = 0;

  if(buf_len > 0) { // Chunk with data
    while(buf_pos < buf_len) {
      buf_count = (buf_len - buf_pos) < line_bytes ? buf_len - buf_pos : line_bytes;
      buf_count -= line_offset;
      hex_dump(line_addr, line_addr + line_offset, &buf[buf_pos], buf_count,
              DEFAULT_INDENT, ADDR_LEN, show_ascii, ansi_color);

      buf_pos += buf_count;
      if(buf_count + line_offset == line_bytes) // Increment address after full line
        line_addr += line_bytes;
      line_offset = 0;
    }

    line_offset = buf_count; // Next chunk may need to offset first line
  }

}


/*
Dump the contents of a buffer to stdout in hex format

This dumps lines of data with the offset, hex values, and printable ASCII

Args:
  buf     : Buffer to dump
  buf_len : Length of buf data
*/
void dump_array(uint8_t *buf, size_t buf_len) {
  dump_array_ex(buf, buf_len, /*show_ascii*/ true, /*ansi_color*/ true);
}


/*
Prepare for line-by-line dump

Args:
  state:      State to track dump progress
  buf:        Buffer to dump
  buf_len:    Length of buf data
  show_ascii: Show table of printable ASCII on right side
  ansi_color: Print dump with color output
*/
void dump_array_init(DumpArrayState *state, uint8_t *buf, size_t buf_len,
                      bool show_ascii, bool ansi_color) {

  memset(state, 0, sizeof(*state));

  state->buf = buf;
  state->buf_len = buf_len;
  state->show_ascii = show_ascii;
  state->ansi_color = ansi_color;
}


/*
Dump next line of hex data

Args:
  state:      State to track dump progress

Returns:
  true when dump is still active
*/
bool dump_array_line(DumpArrayState *state) {
  unsigned line_bytes = 16;
  size_t remaining = state->buf_len - state->buf_pos;
  size_t buf_count = remaining < line_bytes ? remaining : line_bytes; // Bytes for this line

  if(state->line_offset > buf_count)
    state->line_offset = 0; // Cancel any offset

  buf_count -= state->line_offset;

  hex_dump(state->line_addr, state->line_addr + state->line_offset,
          &state->buf[state->buf_pos], buf_count,
          DEFAULT_INDENT, ADDR_LEN, state->show_ascii, state->ansi_color);

  state->buf_pos += buf_count;
  if(buf_count + state->line_offset == line_bytes) // Increment address after full line
    state->line_addr += line_bytes;
  state->line_offset = 0;

  return state->buf_pos < state->buf_len;
}


/*
Dump entire contents of buffer from state

Args:
  state:      State to track dump progress
*/
void dump_array_state(DumpArrayState *state) {
  if(state->buf_pos < state->buf_len) {
    while(dump_array_line(state));
  }
}

