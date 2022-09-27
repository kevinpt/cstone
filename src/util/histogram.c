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

#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "histogram.h"
#include "intmath.h"
#include "minmax.h"


static bool histogram_init_from_buf(Histogram *hist, size_t hist_size, size_t num_bins,
                          int32_t bin_low, int32_t bin_high, bool track_overflow) {

  if(!hist)
    return false;

  // Calculate step size without overflow bin
  int32_t bin_step = (bin_high - bin_low) / num_bins;
  if(bin_low + (bin_step * (int32_t)num_bins) < bin_high) // Round up
    bin_step++;

  // Verify hist is large enough
  size_t require_size = sizeof(Histogram) + num_bins*sizeof(uint32_t);
  if(track_overflow) {
    require_size += sizeof(uint32_t); // Add extra bin
    num_bins++;
  }

  if(hist_size < require_size)
    return false;

  memset(hist, 0, hist_size);

  hist->bin_low  = bin_low;
  hist->bin_high = bin_high;
  hist->bin_step = bin_step;
  hist->num_bins = num_bins;
  hist->track_overflow = track_overflow;

  return true;
}

/*
Prepare a histogram structure

Args:
  num_bins:       Total bins
  bin_low:        Value for smallest bin
  bin_high:       Value for largest bin
  track_overflow: Add extra bin for out of range values

Returns:
  A new histogram object on success
*/
Histogram *histogram_init(size_t num_bins, int32_t bin_low, int32_t bin_high, bool track_overflow) {
  size_t hist_size = sizeof(Histogram) + num_bins*sizeof(uint32_t);
  if(track_overflow)
    hist_size += sizeof(uint32_t); // Add extra bin

  Histogram *hist = malloc(hist_size);
  histogram_init_from_buf(hist, hist_size, num_bins, bin_low, bin_high, track_overflow);

  return hist;
}


/*
Reset collected bin counts in a histogram

Args:
  hist: Histogram to reset

*/
void histogram_reset(Histogram *hist) {
  memset(&hist->bins, 0, hist->num_bins * sizeof(hist->bins[0]));
}


/*
Change the binning parameters

Args:
  hist:     Histogram to modify
  bin_low:  Value for smallest bin
  bin_high: Value for largest bin
*/
void histogram_set_bounds(Histogram *hist, int32_t bin_low, int32_t bin_high) {
  size_t num_bins = hist->num_bins;
  if(hist->track_overflow)
    num_bins--;

  int32_t bin_step = (bin_high - bin_low) / num_bins;
  if(bin_low + (bin_step * (int32_t)num_bins) < bin_high)
    bin_step++;

  hist->bin_low  = bin_low;
  hist->bin_high = bin_high;
  hist->bin_step = bin_step;
}


/*
Add a new data sample to a histogram

Args:
  hist:   Histogram to work on
  sample: New data to be binned
*/
void histogram_add_sample(Histogram *hist, int32_t sample) {
  size_t bin_ix;

  // Bounds check
  if(sample >= hist->bin_low)
    bin_ix = (sample - hist->bin_low) / hist->bin_step;
  else
    bin_ix = SIZE_MAX;

  if(bin_ix >= hist->num_bins)
    bin_ix = SIZE_MAX;

  if(hist->track_overflow && bin_ix == SIZE_MAX)
    bin_ix = hist->num_bins-1; // Put into overflow bin

  if(bin_ix != SIZE_MAX)
    hist->bins[bin_ix]++;
}



/*
Plot a bar chart of histogram bins

Args:
  hist:         Histogram to work on
  max_bar_len:  Maximum number of chars for longest bar
*/
void histogram_plot(Histogram *hist, unsigned int max_bar_len) {
  // Get max population
  uint32_t max_pop = 0;
  for(size_t bin_ix = 0; bin_ix < hist->num_bins; bin_ix++) {
    max_pop = max(max_pop, hist->bins[bin_ix]);
  }

  // Get widest bin label
  int label_low_len = base10_digits(iabs(hist->bin_low));
  if(hist->bin_low < 0)
    label_low_len++; // Add char for sign

  int label_high_len = base10_digits(iabs(hist->bin_high));
  if(hist->bin_high < 0)
    label_high_len++; // Add char for sign

  int label_len = max(label_low_len, label_high_len);

  // Get widest pop count
  int pop_len = base10_digits(max_pop);


  // Plot histogram
  int32_t bin_label = hist->bin_low;

  for(size_t bin_ix = 0; bin_ix < hist->num_bins; bin_ix++) {

    if(hist->track_overflow && bin_ix == hist->num_bins-1)  // Overflow bar
      printf(u8"  %*s : %*" PRIu32 u8" \u2502", label_len, "OV", pop_len, hist->bins[bin_ix]);
    else // Normal bar
      printf(u8"  %*" PRId32 " : %*" PRIu32 u8" \u2502", label_len, bin_label, pop_len, hist->bins[bin_ix]);

#define FULL_BLOCK      u8"\u2588"
#define THREEQTR_BLOCK  u8"\u258A"
#define HALF_BLOCK      u8"\u258C"
#define QUARTER_BLOCK   u8"\u258E"
    // Scale up 4x so we can draw fractional bar ends with Unicode
    unsigned long bar_chars_4x = hist->bins[bin_ix] * max_bar_len * 4 / max_pop;
    unsigned long full_bar_chars = bar_chars_4x / 4;
    unsigned fraction = bar_chars_4x - full_bar_chars*4;
    for(unsigned long j = 0; j < full_bar_chars; j++) {
      fputs(FULL_BLOCK, stdout);
    }
    switch(fraction) {
    case 1: puts(QUARTER_BLOCK); break;
    case 2: puts(HALF_BLOCK); break;
    case 3: puts(THREEQTR_BLOCK); break;
    default: puts(""); break;
    }

    bin_label += hist->bin_step;
  }

}
