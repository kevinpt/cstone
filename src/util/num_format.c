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

#include <stdint.h>
#include <stdbool.h>

#include "range_strings.h"
#include "num_format.h"
#include "intmath.h"


/*
Format a number into a string with SI prefix for exponent

Args:
  value:        Integer value to format
  value_exp:    Base-10 exponent for value
  buf:          Destination buffer
  buf_size:     Size of buf
  frac_places:  Number of fractional decimal places in output, -1 for max precision
  options:      Formatting option flags

Returns:
  buf pointer
*/
char *to_si_value(long value, int value_exp, char *buf, size_t buf_size, short frac_places,
                  unsigned short options) {
  char si_prefix;

  // Convert exponent to scaling
  unsigned fp_scale = 1;
  for(int i = 0; i < frac_places; i++) {
    fp_scale *= 10;
  }

  long scaled_v   = to_fixed_si(value, value_exp, fp_scale, &si_prefix, options & SIF_POW2);
  bool negative   = scaled_v < 0;
  unsigned long scaled_v_abs = negative ? -scaled_v : scaled_v;

  // Remove fraction if integer portion >= 10 when there is a prefix
  if((options & SIF_SIMPLIFY) && scaled_v_abs >= 10 * fp_scale && si_prefix != '\0') {
    scaled_v_abs += (options & SIF_ROUND_TO_CEIL) ? fp_scale : fp_scale/2;  // Round up
    scaled_v_abs = (scaled_v_abs / fp_scale) * fp_scale;
  }
  scaled_v = negative ? -scaled_v_abs : scaled_v_abs;

  AppendRange rng;
  range_init(&rng, buf, buf_size);

  if((scaled_v_abs / fp_scale) * fp_scale == scaled_v_abs)  // No fractional part
    frac_places = 0;

  // Format fixed point into string
  range_cat_fixed(&rng, scaled_v, fp_scale, frac_places);
  if(!(options & SIF_TIGHT_UNITS))
    range_cat_char(&rng, ' ');

  if(si_prefix != '\0') {
    if(si_prefix == 'u' && (options & SIF_GREEK_MICRO))
      range_cat_str(&rng, u8"\u00b5"); // µ
    else {
      if(si_prefix == 'k' && (options & SIF_UPPER_CASE_K))
        si_prefix = 'K';
      range_cat_char(&rng, si_prefix);
    }
  } else if(!(options & SIF_NO_ALIGN_UNITS)) {
    range_cat_char(&rng, ' ');  // Add space in place of prefix so unit symbols stay aligned
  }

  return buf;
}
