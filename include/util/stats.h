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

#ifndef STATS_H
#define STATS_H

typedef long        SampleDatum;
typedef long long   SampleDatumProduct;

typedef struct {
  size_t count;
  unsigned fp_exp;
  SampleDatum mean;
  SampleDatum m2;
} OnlineStats;


#ifdef __cplusplus
extern "C" {
#endif

//unsigned long isqrt_fixed(unsigned long value, unsigned fp_exp);

void stats_init(OnlineStats *os, unsigned fp_exp);

static inline SampleDatum stats_fp_scale(OnlineStats *os) {
  return 1ul << os->fp_exp;
}

void stats_add_fixed_sample(OnlineStats *os, SampleDatum sample);
static inline void stats_add_sample(OnlineStats *os, SampleDatum sample) {
  stats_add_fixed_sample(os, sample << os->fp_exp);
}

static inline SampleDatum stats_mean(OnlineStats *os) {
  return os->mean;
}
SampleDatum stats_variance(OnlineStats *os);
SampleDatum stats_sample_variance(OnlineStats *os);
SampleDatum stats_std_dev(OnlineStats *os);

#ifdef __cplusplus
}
#endif

#endif // STATS_H
