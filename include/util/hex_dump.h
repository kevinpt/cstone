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
Hex data dump library

This prints array data as a hex dump with optional ANSI coloration.
------------------------------------------------------------------------------
*/

#ifndef HEX_DUMP_H
#define HEX_DUMP_H

typedef struct {
  bool show_ascii;    // Show printable ASCII dump
  bool ansi_color;    // Colorize output
  const char *prefix; // Optional prefix at the start of each line
  int indent;         // Indentation level for dumped lines
  int addr_size;      // Characters for printing address field
} DumpArrayCfg;


typedef struct {
  uint8_t *buf;
  size_t buf_len;
  size_t buf_addr;
  DumpArrayCfg cfg;

  size_t buf_pos;
  size_t line_addr;
  size_t line_offset; // Bytes to skip at start of line
} DumpArrayState;


#ifdef __cplusplus
extern "C" {
#endif

// ******************** Bulk dump operations ********************
void dump_array(const uint8_t *buf, size_t buf_len);
void dump_array_ex(const uint8_t *buf, size_t buf_len, size_t buf_addr, const DumpArrayCfg *cfg);


// ******************** Line by line dump operations ********************
// Use these for systems with limited memory for stdout buffer
void dump_array_init(DumpArrayState *state, uint8_t *buf, size_t buf_len, size_t buf_addr,
                     DumpArrayCfg *cfg);
bool dump_array_line(DumpArrayState *state);
void dump_array_state(DumpArrayState *state);

#ifdef __cplusplus
}
#endif

#endif // HEX_DUMP_H
