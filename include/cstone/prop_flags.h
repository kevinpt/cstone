#ifndef PROP_FLAGS_H
#define PROP_FLAGS_H

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
Manage Boolean property flags in a common set. This is a more efficient
alternative to storing flags in a prop_db hash table.

------------------------------------------------------------------------------
*/

typedef struct {
  uint32_t  prefix;    // Prop ID for flag set
  const char **index_names;
  size_t    index_names_len;
  int       max_name_len;
  uint8_t   flags[32];  // 256 bits indexed by P3 array
} PropFlags;



#ifdef __cplusplus
extern "C" {
#endif

bool prop_flags_init(PropFlags *pf, uint32_t prefix, const char **index_names,
                      size_t index_names_len, const uint8_t flag_values[32]);
void prop_flags_get_all(PropFlags *pf, uint8_t flag_values[32]);

bool prop_flags_set(PropFlags *pf, uint32_t prop, bool value);
bool prop_flags_set_by_name(PropFlags *pf, const char *flag_name, bool value);
bool prop_flags_get(PropFlags *pf, uint32_t prop);

int prop_flags_lookup_name(PropFlags *pf, const char *flag_name);
const char *prop_flags_lookup_index(PropFlags *pf, uint8_t index);

void prop_flags_dump(PropFlags *pf, bool set_flags_only, uint8_t max_flag);


#ifdef __cplusplus
}
#endif


#endif // PROP_FLAGS_H
