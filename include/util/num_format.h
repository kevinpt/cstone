#ifndef NUM_FORMAT_H
#define NUM_FORMAT_H

// Formatting options for to_si_value():
#define SIF_POW2            0x01  // Scale values by 1024 rather than 1000
#define SIF_SIMPLIFY        0x02  // Remove fraction from larger values
#define SIF_ROUND_TO_CEIL   0x04  // Rounding mode for SIF_SIMPLIFY
#define SIF_TIGHT_UNITS     0x08  // No space between value and prefix
#define SIF_NO_ALIGN_UNITS  0x10  // Skip extra space when there is no prefix
#define SIF_GREEK_MICRO     0x20  // Use UTF-8 µ for micro- prefix
#define SIF_UPPER_CASE_K    0x40  // Use Upper case for kilo- prefix

#ifdef __cplusplus
extern "C" {
#endif

char *to_si_value(long value, int value_exp, char *buf, size_t buf_size, short frac_places,
                  unsigned short options);

#ifdef __cplusplus
}
#endif

#endif // NUM_FORMAT_H
