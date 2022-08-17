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
Track statistics with online algorithm

This library implements an online mean and variance algorithm using fixed-point
integer values. Fixed-point numbers are scaled by a base-2 exponent established
when intializing an OnlineStats object with :c:func:`stats_init`. This permits
an application to selection the desired fractional precision for a particular
use case.

The mean calculation is numerically stable but as samples accumulate their
ability to shift the mean diminishes. With a sufficiently large count, new
samples have no effect. This starts becoming a problem when the sample count
reaches 2^fp_exp. For long running statistics you should arrange to replace an
OnlineStats object with a fresh copy to avoid the numeric effects of division
with a large sample count.

------------------------------------------------------------------------------
*/

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "intmath.h"
#include "stats.h"


/*
Initialize online statistics state

Args:
  os:     Statistics state
  fp_exp: Fixed-point base-2 exponent of samples. Use 0 for plain integers.
*/
void stats_init(OnlineStats *os, unsigned fp_exp) {
  memset(os, 0, sizeof *os);
  os->fp_exp = fp_exp;
}


/*
Add a new fixed-point sample to the online statistics

This tracks the mean and variance of all samples processed.

Args:
  os:     Statistics state
  sample: New fixed-point sample to add
*/
void stats_add_fixed_sample(OnlineStats *os, SampleDatum sample) {
  // https://en.wikipedia.org/wiki/Algorithms_for_calculating_variance
  os->count++;

  if(os->count == 1) {
    os->mean  = sample;
    os->m2    = 0;
  } else {
    // Welford's online algorithm
    SampleDatum prev_mean = os->mean;
    os->mean  += (sample - os->mean) / (SampleDatum)os->count;
    os->m2    += ((SampleDatumProduct)(sample - prev_mean) * (sample - os->mean)) >> os->fp_exp;
  }
}


/*
Calculate the variance of collected samples

Args:
  os: Statistics state

Returns:
  Variance of all data as base-2 fixed-point
*/
SampleDatum stats_variance(OnlineStats *os) {
  if(os->count < 1)
    return 0;

  return os->m2 / (SampleDatum)os->count;
}


/*
Calculate the sample variance of collected samples

Args:
  os: Statistics state

Returns:
  Sample variance of all data as base-2 fixed-point
*/
SampleDatum stats_sample_variance(OnlineStats *os) {
  if(os->count < 2)
    return 0;

  return os->m2 / (SampleDatum)(os->count - 1);
}


/*
Calculate the standard deviation of collected samples

Args:
  os: Statistics state

Returns:
  Standard deviation of all data as base-2 fixed-point
*/
SampleDatum stats_std_dev(OnlineStats *os) {
  if(os->count < 2)
    return 0;

  SampleDatum svar = os->m2 / (SampleDatum)(os->count - 1);
  return isqrt_fixed(svar, os->fp_exp);
}

