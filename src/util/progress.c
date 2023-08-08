/* SPDX-License-Identifier: MIT
Copyright 2023 Kevin Thibedeau
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
Progress bar and activity spinner

------------------------------------------------------------------------------
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "util/progress.h"
#include "util/term_color.h"


// ******************** Progress bar ********************

static const ProgressBarStyle s_default_style = {
  .left_cap = A_BWHT u8"⦗" A_NONE,
  .right_cap = A_NONE A_BWHT u8"⦘" A_NONE,
  .start_bar = A_CYN, //A_CLR(FG_BCYN, BG_BLU),
  .bar_ch = NULL, //u8"▰",
  .start_empty = A_BBLK,
  .empty_ch = u8"▱",
  .show_percent = true,
  .skip_refresh = false
};



/*
Print a new progress bar line to console

The scale factor fp_scale can be any arbitrary integer, not just a power of 2. Use 100
for integer percentages between 0.0 and 1.0.

When the syle has skip_refresh == false you can solely call this function in a loop to
redeaw the line for the progress bar. When it is false, you need to manually position
the cursor at the start of the line with :c:func:`home_cursor` and call :c:func:`fflush`
if line buffering is enabled.

When the style is configured without a string for bar_ch, the bar is rendered using
block drawing characters that increase the effective horizontal resolution 4x. This
permits more detailed rendering with short bars.

Args:
  fp_value: Fixed-point value for progress bar position
  fp_scale: Scale factor for fp_value
  width:    Character cells occupied by the bar
  prefix:   Optional string printed before the bar
  style:    Style configuration for rendering the bar
*/
void print_progress_bar_ex(unsigned fp_value, unsigned fp_scale, unsigned width, const char *prefix,
                            const ProgressBarStyle *style) {
#define FULL_BLOCK      u8"\u2588"
#define THREEQTR_BLOCK  u8"\u258A"
#define HALF_BLOCK      u8"\u258C"
#define QUARTER_BLOCK   u8"\u258E"

  unsigned long empty_chars;

  if(!style->skip_refresh)
    home_cursor();

  if(prefix)
    fputs(prefix, stdout);

  // Start bar
   if(style->left_cap)
    fputs(style->left_cap, stdout);


  if(!style->bar_ch) { // Block drawing bar with fractional end
    // Scale up 4x so we can draw fractional bar ends with Unicode
    unsigned long bar_chars_4x = (unsigned long)fp_value * width * 4 / fp_scale;
    unsigned long full_bar_chars = bar_chars_4x / 4;
    unsigned fraction = bar_chars_4x - full_bar_chars*4; // 0.0, 0.25, 0.5, or 0.75

    empty_chars = width - full_bar_chars - 1; // Assume fraction block

    // Print bar
    fputs(style->start_bar, stdout);
    for(unsigned long i = 0; i < full_bar_chars; i++) {
      fputs(FULL_BLOCK, stdout);
    }

    switch(fraction) {
    case 0: empty_chars++; break; // No fraction block
    case 1: fputs(QUARTER_BLOCK, stdout); break;
    case 2: fputs(HALF_BLOCK, stdout); break;
    case 3: fputs(THREEQTR_BLOCK, stdout); break;
    default: break;
    }

  } else { // Caller specified bar chars without fractional end
    unsigned long full_bar_chars = ((unsigned long)fp_value * width + (fp_scale/2)) / fp_scale;
    empty_chars = width - full_bar_chars;

    // Print bar
    fputs(style->start_bar, stdout);
    for(unsigned long i = 0; i < full_bar_chars; i++) {
      fputs(style->bar_ch, stdout);
    }
  }


  // Fill empty space
  fputs(style->start_empty, stdout);

  for(unsigned long i = 0; i < empty_chars; i++) {
    fputs(style->empty_ch, stdout);
  }

  // End bar
  if(style->right_cap)
   fputs(style->right_cap, stdout);

  if(style->show_percent) {
    unsigned long percent = ((unsigned long)fp_value * 100 + (fp_scale/2)) / fp_scale;
    printf(" %3d%%", percent);
  }

  if(!style->skip_refresh)
    fflush(stdout);
}


/*
Print a new progress bar line to console

This is a simplified wrapper for :c:func:`print_progress_bar_ex` using the default style.

Args:
  fp_value: Fixed-point value for progress bar position
  fp_scale: Scale factor for fp_value
  width:    Character cells occupied by the bar
*/
void print_progress_bar(unsigned fp_value, unsigned fp_scale, unsigned width) {
  print_progress_bar_ex(fp_value, fp_scale, width, NULL, &s_default_style);
}



// ******************** Activity spinner ********************

/*
Initialize an activity spinner

Args:
  spin: Spinner state
  glyphs: Array of strings for each animation frame. Must be NULL terminated.
  ansi_fmt: Format string for the glyphs. Use an empty string for no format or when using emoji.
*/
void spinner_init(SpinnerState *spin, const uint8_t *glyphs[], const uint8_t *ansi_fmt) {
  spin->glyphs      = glyphs;
  spin->pos         = 0;
  spin->ansi_fmt    = ansi_fmt;
}



static inline const uint8_t *spinner__next_glyph(SpinnerState *spin) {
  const uint8_t *next = spin->glyphs[spin->pos++];

  if(!spin->glyphs[spin->pos])
    spin->pos = 0;

  return next;
}



/*
Print a frame of a spinner status line

Args:
  spin:     Spinner state
  prefix:   Optional string printed before the bar 
  frame:    Text for the spinner frame to render
  refresh:  When true, redraw console line with home of cursor and fflush
*/

void print_spinner_frame(SpinnerState *spin, const char *prefix, const uint8_t *frame, bool refresh) {
  if(refresh)
    home_cursor();

  if(prefix)
    fputs(prefix, stdout);

  printf("%s%s" A_NONE, spin->ansi_fmt, frame);

  if(refresh)
    fflush(stdout);
}


/*
Print the next frame of a spinner animation sequence

Args:
  spin:     Spinner state
  prefix:   Optional string printed before the bar 
  refresh:  When true, redraw console line with home of cursor and fflush
*/
void print_spinner(SpinnerState *spin, const char *prefix, bool refresh) {
  const uint8_t *glyph = spinner__next_glyph(spin);

  print_spinner_frame(spin, prefix, glyph, refresh);
}




#ifdef TEST_PROGRESS

#include <unistd.h>

int main(int argc, char *argv[]) {


  unsigned width = 10;
  if(argc > 1)
    width = strtoul(argv[1], NULL, 10);
 
  for(unsigned i = 0; i <= 100; i += 2) {
    print_progress_bar_ex(i, 100, width, "Prefix: ", &s_default_style);
    usleep(50 * 1000);
  }
  puts("");


  SpinnerState spin;

#  if 0
  static const uint8_t *arrow_spinner[] = {
    u8"←", u8"↖", u8"↑", u8"↗", u8"→", u8"↘", u8"↓", u8"↙", NULL
  };
  spinner_init(&spin, arrow_spinner, A_CYN);

#  else
  static const uint8_t *moon_spinner[] = {
    u8"🌑", u8"🌒", u8"🌓", u8"🌔", u8"🌕", u8"🌖", u8"🌗", u8"🌘", NULL
  };
  spinner_init(&spin, moon_spinner, "");
#  endif

  for(int i = 0; i < 20; i++) {
    print_spinner(&spin, "Prefix: ", /*refresh*/true);
    fputs(" Message", stdout);
    fflush(stdout);
    usleep(100 * 1000);
  }

  puts("");
  

  return 0;
}

#endif

