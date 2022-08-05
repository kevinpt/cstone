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

/* Hex data dump library */

#ifndef HEX_DUMP_H
#define HEX_DUMP_H


typedef struct {
  uint8_t *buf;
  size_t buf_len;
  bool show_ascii;
  bool ansi_color;

  size_t buf_pos;
  size_t line_addr;   // 
  size_t line_offset; // Bytes to skip at start of line
} DumpArrayState;


#ifdef __cplusplus
extern "C" {
#endif

void dump_array(uint8_t *buf, size_t buf_len);
void dump_array_ex(uint8_t *buf, size_t buf_len, bool show_ascii, bool ansi_color);


void dump_array_init(DumpArrayState *state, uint8_t *buf, size_t buf_len,
                      bool show_ascii, bool ansi_color);
bool dump_array_line(DumpArrayState *state);
void dump_array_state(DumpArrayState *state);

#ifdef __cplusplus
}
#endif

#endif // HEX_DUMP_H
