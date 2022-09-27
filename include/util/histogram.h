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

#ifndef HISTOGRAM_H
#define HISTOGRAM_H

typedef struct {
  int32_t   bin_low;
  int32_t   bin_high;
  int32_t   bin_step;
  size_t    num_bins;
  bool      track_overflow;
  uint32_t  bins[];  // Population counts
} Histogram;

#ifdef __cplusplus
extern "C" {
#endif

Histogram *histogram_init(size_t num_bins, int32_t bin_low, int32_t bin_high, bool track_overflow);
static inline void histogram_free(Histogram *hist) {
  free(hist);
}
void histogram_reset(Histogram *hist);
void histogram_set_bounds(Histogram *hist, int32_t bin_low, int32_t bin_high);
void histogram_add_sample(Histogram *hist, int32_t sample);
void histogram_plot(Histogram *hist, unsigned int max_bar_len);

#ifdef __cplusplus
}
#endif

#endif
