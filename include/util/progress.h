#ifndef PROGRESS_H
#define PROGRESS_H

typedef struct {
  const char *left_cap;     // Optional leading text for left side of bar
  const char *right_cap;    // Optional trailing test on right side of bar
  const char *start_bar;    // ANSI sequence to configure bar format
  const char *bar_ch;       // Optional character for filled cells
  const char *start_empty;  // ANSI sequence to configure empty section
  const char *empty_ch;     // Character for empty cells
  bool  show_percent;       // Show a numeric percentage after the bar
  bool  skip_refresh;       // When true, don't home cursor or call fflush()
} ProgressBarStyle;

/*
                  ⦗▰▰▰▰▰▰▰▰▰▰▰▰▰▰▰▰▰▰▱▱▱▱▱▱▱▱▱▱▱▱▱▱▱▱▱▱▱▱▱▱⦘  46%
                  ↑   ↑                  ↑                 ↑
left_cap  u8"⦗" ──╯   │                  │                 │
bar_ch    u8"▰" ──────╯                  │                 │
empty_ch  u8"▱" ─────────────────────────╯                 │
right_cap u8"⦘" ───────────────────────────────────────────╯

                  ⦗█████████████████████▌▱▱▱▱▱▱▱▱▱▱▱▱▱▱▱▱▱▱⦘  54%
                      ↑
bar_ch    NULL  ──────╯

*/


typedef struct {
  const uint8_t **glyphs;   // NULL terminated array of utf-8 strings
  const uint8_t *ansi_fmt;  // ANSI formatting for the spinner glyphs
  int pos;                  // Iterator index into glyphs array
} SpinnerState;


#ifdef __cplusplus
extern "C" {
#endif

static inline void home_cursor(void) {
#define VT100_LN_CLR        "\33[2K"
#define VT100_LN_HOME       "\33[200D"  // Move left 200 chars to force into home column

  fputs(VT100_LN_CLR VT100_LN_HOME, stdout);
}


void print_progress_bar_ex(unsigned fp_value, unsigned fp_scale, unsigned width, const char *prefix,
                            const ProgressBarStyle *style);

void print_progress_bar(unsigned fp_value, unsigned fp_scale, unsigned width);

void spinner_init(SpinnerState *spin, const uint8_t *glyphs[], const uint8_t *ansi_fmt);
void print_spinner_frame(SpinnerState *spin, const char *prefix, const uint8_t *frame, bool refresh);
void print_spinner(SpinnerState *spin, const char *prefix, bool refresh);

#ifdef __cplusplus
}
#endif

#endif // PROGRESS_H
