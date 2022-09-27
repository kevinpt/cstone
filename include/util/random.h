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

#ifndef RANDOM_H
#define RANDOM_H

typedef struct {
  uint64_t s;
} RandomState;


#ifdef __cplusplus
extern "C" {
#endif

void random_init(RandomState *state, uint64_t seed);
uint64_t random_seed_from_time(time_t timestamp);
uint64_t random_seed_from_str(const char *seed);
uint32_t random_from_system(void);

uint64_t random_next64(RandomState *state);
uint32_t random_next32(RandomState *state);

int64_t random_range64(RandomState *state, int64_t min, int64_t max);
int32_t random_range32(RandomState *state, int32_t min, int32_t max);

void random_bytes(RandomState *state, uint8_t *dest, size_t dest_size);

void random_weights_init(const uint32_t *weights, size_t weights_len, uint32_t *cum_weights);
size_t random_weighted_choice(RandomState *state, const uint32_t *cum_weights, size_t cum_weights_len);
bool random_bool(RandomState *state, unsigned chance, unsigned out_of);

#ifdef __cplusplus
}
#endif

#endif // RANDOM_H
