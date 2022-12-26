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



// Newlib doesn't support printf() %zu specifier so we detect the target platform
#if defined __linux__ || defined __APPLE__ || defined _WIN32  // System libc for full OS env.
#  define PRIuz "zu"
#  define PRIXz "zX"
// Baremetal:
#elif defined __arm__ // Newlib on ARM32
// arm-none-eabi has size_t as "unsigned int" but uint32_t is "long unsigned int". Crazy
#  define PRIuz "u"
#  define PRIXz "X"
#elif defined __aarch64__ // Newlib on ARM64
#  define PRIuz "lu"
#  define PRIXz "lX"
#else
#  error "Unknown target platform"
#endif


#define ADDR_LEN        4

// Dump a line of hex data
static void hex_dump_line(size_t line_addr, size_t buf_addr, const uint8_t *buf, size_t buf_len,
                     const DumpArrayCfg *cfg) {
  size_t offset = buf_addr - line_addr;
  unsigned int line_bytes = 16;
  int addr_size = (cfg->addr_size == 0) ? ADDR_LEN : cfg->addr_size;

  bool color_on = false;

#define SET_COLOR(c)  if(cfg->ansi_color && !color_on) fputs(c, stdout); color_on = true
#define END_COLOR     if(cfg->ansi_color && color_on) fputs(A_NONE, stdout); color_on = false

  if(cfg->prefix)
    fputs(cfg->prefix, stdout);

  // Indent line and show address
  SET_COLOR(A_BLU);
  printf("%*s%0*" PRIXz "  ", cfg->indent, " ", addr_size, line_addr);
  END_COLOR;

  // Print leading gap
  for(size_t i = 0; i < offset; i++) {
    fputs("   ", stdout);
  }
  // Print data

  for(size_t i = 0; i < buf_len; i++) {
    if(isgraph(buf[i])) {
      SET_COLOR(A_YLW);
      printf("%02X ", buf[i]);
    } else {
      END_COLOR;
      printf("%02X ", buf[i]);
    }
  }
  END_COLOR;


  // Print trailing gap
  if(buf_len + offset < line_bytes) {
    offset = line_bytes - (buf_len + offset);

    for(size_t i = 0; i < offset; i++) {
      fputs("   ", stdout);
    }
  }

  if(cfg->show_ascii) {
    fputs(cfg->ansi_color ? A_GRN " |" A_NONE : " |", stdout);

    offset = buf_addr - line_addr;
    // Print leading gap
    for(size_t i = 0; i < offset; i++) {
      fputs(" ", stdout);
    }
    // Print data
    color_on = false;
    for(size_t i = 0; i < buf_len; i++) {
      if(isgraph(buf[i])) {
        SET_COLOR(A_YLW);
        printf("%c", buf[i]);
      } else {
        END_COLOR;
        printf(".");
      }
    }
    END_COLOR;

    // Print trailing gap
    if(buf_len + offset < line_bytes) {
      offset = line_bytes - (buf_len + offset);

      for(size_t i = 0; i < offset; i++) {
        fputs(" ", stdout);
      }
    }

    fputs(cfg->ansi_color ? A_GRN " |" A_NONE : " |", stdout);
  }
  
  printf("\n");
}


// ******************** Bulk dump operations ********************

/*
Dump the contents of a buffer to stdout in hex format

This dumps lines of data with the offset, hex values, and printable ASCII

Args:
  buf:      Buffer to dump
  buf_len:  Length of buf data
  buf_addr: Address for start of buf
  cfg:      Configuration settings for dump format
*/
void dump_array_ex(const uint8_t *buf, size_t buf_len, size_t buf_addr, const DumpArrayCfg *cfg) {
  size_t buf_pos, buf_count;
  size_t line_addr, line_offset;
  const size_t line_bytes = 16;

  line_addr = buf_addr & ~(line_bytes-1);
  line_offset = buf_addr - line_addr;

  buf_pos = 0;

  if(buf_len > 0) { // Chunk with data
    while(buf_pos < buf_len) {
      buf_count = (buf_len - buf_pos) < line_bytes ? buf_len - buf_pos : line_bytes;
      buf_count -= line_offset;
      hex_dump_line(line_addr, line_addr + line_offset, &buf[buf_pos], buf_count, cfg);

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
void dump_array(const uint8_t *buf, size_t buf_len) {
  DumpArrayCfg cfg = {
    .show_ascii = true,
    .ansi_color = true,
    .indent     = 4
  };

  dump_array_ex(buf, buf_len, 0, &cfg);
}


// ******************** Line by line dump operations ********************
// Use these for systems with limited memory for stdout buffer

/*
Prepare for line-by-line dump

Args:
  state:    State to track dump progress
  buf:      Buffer to dump
  buf_len:  Length of buf data
  buf_addr: Address for start of buf
  cfg:      Configuration settings for dump format
*/
void dump_array_init(DumpArrayState *state, uint8_t *buf, size_t buf_len, size_t buf_addr,
                     DumpArrayCfg *cfg) {

  const size_t line_bytes = 16;

  memset(state, 0, sizeof(*state));

  state->buf      = buf;
  state->buf_len  = buf_len;
  state->buf_addr = buf_addr;
  state->cfg      = *cfg;

  state->line_addr   = buf_addr & ~(line_bytes-1);
  state->line_offset = buf_addr - state->line_addr;
}


/*
Dump next line of hex data

Args:
  state:      State to track dump progress

Returns:
  true when dump is still active
*/
bool dump_array_line(DumpArrayState *state) {
  const size_t line_bytes = 16;
  size_t remaining = state->buf_len - state->buf_pos;
  size_t buf_count = remaining < line_bytes ? remaining : line_bytes; // Bytes for this line

  if(state->line_offset > buf_count)
    state->line_offset = 0; // Cancel any offset

  buf_count -= state->line_offset;

  hex_dump_line(state->line_addr, state->line_addr + state->line_offset,
          &state->buf[state->buf_pos], buf_count, &state->cfg);

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

