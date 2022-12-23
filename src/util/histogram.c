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

#include "util/histogram.h"
#include "util/intmath.h"
#include "util/minmax.h"
#include "util/term_color.h"


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
Get the largest bin count for the current data set

Args:
  hist:   Histogram to work on

Returns:
  Largest bin count
*/

uint32_t histogram_max_bin(Histogram *hist) {
  uint32_t max_pop = 0;
  for(size_t bin_ix = 0; bin_ix < hist->num_bins; bin_ix++) {
    max_pop = max(max_pop, hist->bins[bin_ix]);
  }
  return max_pop;
}



/*
Plot a bar chart of histogram bins with a vertical axis

Args:
  hist:         Histogram to work on
  max_bar_len:  Maximum number of chars for longest bar
*/
uint32_t histogram_plot(Histogram *hist, unsigned max_bar_len) {
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

#define TICK_VMAJ_S  u8"\u2511"
#define TICK_VMAJ_M  u8"\u2525"
#define TICK_VMAJ_E  u8"\u2519"

  static const char *vticks[] = {TICK_VMAJ_S, TICK_VMAJ_M, TICK_VMAJ_E, " "};

  int32_t bin_label = hist->bin_low;

  for(size_t bin_ix = 0; bin_ix < hist->num_bins; bin_ix++) {

    bool overflow = hist->track_overflow && bin_ix == hist->num_bins-1;

    const char *tick;
    if(bin_ix == 0)
      tick = vticks[0]; // Start
    else if(bin_ix == hist->num_bins - (hist->track_overflow ? 2 : 1))
      tick = vticks[2]; // End
    else if(overflow)
      tick = vticks[3];
    else
      tick = vticks[1]; // Middle


    // Ticks
    if(overflow)  // Red overflow bar
      printf(A_YLW u8"  %*s %s" A_NONE, label_len, "OV", tick);
    else // Normal bar
      printf(A_YLW u8"  %*" PRId32 " %s" A_NONE, label_len, bin_label, tick);

    // Pop count for bin
    if(hist->bins[bin_ix] > 0)
      printf(" %*" PRIu32 " ", pop_len, hist->bins[bin_ix]);

    if(overflow)
      fputs(A_BRED, stdout);


    // Bar
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
    case 1: fputs(QUARTER_BLOCK, stdout); break;
    case 2: fputs(HALF_BLOCK, stdout); break;
    case 3: fputs(THREEQTR_BLOCK, stdout); break;
    default: break;
    }

    // End bar
    puts(overflow ? A_NONE : "");

    bin_label += hist->bin_step;
  }

  return max_pop;
}


/*
Plot a bar chart of histogram bins with a horizontal axis

Bars longer than :c:data:`bar_threshold` are not considered for vertical scaling.
Over-long bars are capped with a triangle indicating they extend beyond
the chart.

Args:
  hist:           Histogram to work on
  max_bar_len:    Maximum number of char cells for longest bar
  indent:         Number of chars to indent
  min_tick_step:  Minimum chars between ticks. 0 for default.
  bar_threshold:  Limit length of bars longer then threshold. 0 to disable.
*/
uint32_t histogram_plot_horiz(Histogram *hist, unsigned max_bar_len, unsigned indent,
                              unsigned min_tick_step, uint32_t bar_threshold) {
  // Get max population
  uint32_t max_pop = 1;
  for(size_t bin_ix = 0; bin_ix < hist->num_bins; bin_ix++) {
    if(bar_threshold == 0 || hist->bins[bin_ix] <= bar_threshold)
      max_pop = max(max_pop, hist->bins[bin_ix]);
  }

  // Build bitmap of cells containing bars
  // Fractionally covered cells are encoded bitwise
#define bytes_per_bar  max_bar_len
  uint8_t *bmap = calloc(hist->num_bins * bytes_per_bar, 1);
  if(!bmap)
    return 0;

  for(size_t bin_ix = 0; bin_ix < hist->num_bins; bin_ix++) {
    unsigned bar_len;
    bool truncated = bar_threshold > 0 && hist->bins[bin_ix] > max_pop;
      if(!truncated)
        bar_len = hist->bins[bin_ix] * (max_bar_len*8) / max_pop;
      else
        bar_len = max_bar_len*8;

    // Fill full cells 8-bits at once
    int addr = 0;
    for(unsigned y = 0; y < bar_len/8; y++) {
      addr = bin_ix*bytes_per_bar + y;
      bmap[addr] = 0xFF;
    }
    if(truncated)
      bmap[addr] = 0x10;

    // Fill partial cells at top of bar
    unsigned sy = (bar_len/8)*8;
    if(sy < bar_len) {
      addr = bin_ix*bytes_per_bar + sy/8;
      uint8_t partial = (1u << (bar_len - sy))-1;
      bmap[addr] = partial;
    }
  }

  // Block drawing for partial char cells
  static const char *hist_chars[] = {".", u8"\u2581", u8"\u2582", u8"\u2583",
                    u8"\u2584", u8"\u2585", u8"\u2586", u8"\u2587", u8"\u2588",
                    u8"\u25B2"}; // Triangle

  // Plot histogram
  for(int y = max_bar_len-1; y >= 0; y--) {
    bool overflow = false;

    printf("%*s", indent, "");
    for(size_t bin_ix = 0; bin_ix < hist->num_bins; bin_ix++) {
      uint8_t cell = bmap[bin_ix*bytes_per_bar + y];
      if(hist->track_overflow) // Overflow bar is red
        overflow = bin_ix == hist->num_bins-1 && cell != 0;

      if(overflow)
        fputs(A_RED, stdout);

      switch(cell) {
      case 0x00: fputs(hist_chars[0], stdout); break;
      case 0x01: fputs(hist_chars[1], stdout); break;
      case 0x03: fputs(hist_chars[2], stdout); break;
      case 0x07: fputs(hist_chars[3], stdout); break;
      case 0x0F: fputs(hist_chars[4], stdout); break;
      case 0x1F: fputs(hist_chars[5], stdout); break;
      case 0x3F: fputs(hist_chars[6], stdout); break;
      case 0x7F: fputs(hist_chars[7], stdout); break;
      case 0xFF: fputs(hist_chars[8], stdout); break;
      case 0x10: fputs(hist_chars[9], stdout); break; // Truncated bar
      default: fputs("x", stdout); break; // Shouldn't ever happen
      }

    }
    puts(overflow ? A_NONE : "");
  }

  free(bmap);


  // Plot tick marks
  unsigned num_bins = hist->num_bins;
  if(hist->track_overflow)  // No tick under overflow bar
    num_bins--;

  // Ensure ticks are spaced enough to permit label with one space
  unsigned min_step_low = base10_digits(iabs(hist->bin_low)) + 1;
  if(hist->bin_low < 0) min_step_low++;
  unsigned min_step_high = base10_digits(iabs(hist->bin_high)) + 1;
  if(hist->bin_high < 0) min_step_high++;

  min_tick_step = max(min_tick_step, max(min_step_low, min_step_high));


  unsigned tick_step = num_bins;
  // Find integer divisor for bins
  for(unsigned d = 25; d >= min_tick_step; d--) {
    if((num_bins / d) * d == num_bins)
      tick_step = d;
  }

  printf("%*s" A_YLW, indent, "");
  unsigned axis_pos = 0;
  unsigned tick_pos = tick_step;

#define TICK_MAJ_S  u8"\u250E"
#define TICK_MAJ_M  u8"\u2530"
#define TICK_MAJ_E  u8"\u2512"
#define H_LINE      u8"\u2500"

  while(axis_pos < num_bins) {
    if(axis_pos == 0) {
      fputs(TICK_MAJ_S, stdout);
    } else if(axis_pos == num_bins-1) {
      puts(TICK_MAJ_E A_NONE);
    } else if(axis_pos >= tick_pos) {
      fputs(TICK_MAJ_M, stdout);
      tick_pos += tick_step;
    } else {
      fputs(H_LINE, stdout);
    }
    axis_pos++;
  }


  // Plot bin labels
  int32_t bin_label = hist->bin_low;

  bool adj_neg_labels = false;
  if(bin_label < 0 && indent > 0)
    adj_neg_labels = true;

  if(adj_neg_labels) // Shift negative labels left one char to align digit with tick
    indent--;
  printf("%*s" A_YLW, indent, "");

  unsigned last_tick_step = tick_step;
  axis_pos = 0;
  while(axis_pos < num_bins) {
    if(axis_pos + tick_step >= num_bins) { // Last tick may need adjustment
      last_tick_step = tick_step;
      tick_step = num_bins - axis_pos - 1;
      if(tick_step == 0)
        break;
    }

    printf("%-*"PRIi32, tick_step, bin_label);

    int32_t next_label = bin_label + hist->bin_step * tick_step;
    if(adj_neg_labels && bin_label < 0 && next_label >= 0) // Add extra space to undo left adjustment
      fputs(" ", stdout);

    axis_pos += tick_step;
    bin_label = next_label;
  }

  // Last label
  printf("%s%"PRIi32 A_NONE"\n", last_tick_step < min_tick_step ? " " : "",
                            num_bins * hist->bin_step);

  return max_pop;
}




#ifdef TEST_HISTOGRAM
int main(void) {
  Histogram *h = histogram_init(50, 0, 100, /*track_overflow*/true);

  const int counts[150] = {[0]=1, [50]=32, [80]=16+3, [149]=10};
  for(int i = 0; i < 150; i++) {
    for(int s = 0; s < counts[i]; s++)
      histogram_add_sample(h, i);
  }

//  histogram_plot_horiz(h, 4, 2, 0, 0);

  uint32_t max_bin = histogram_max_bin(h);
  histogram_plot_horiz(h, 4, 2, 0, max_bin * 7 / 8);
//  histogram_plot(h, 50);

  free(h);

  return 0;
}
#endif
