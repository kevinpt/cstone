#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <inttypes.h>
#include <string.h>

#include "cstone/blocking_io.h"
#include "cstone/dump_reg.h"

#include "util/term_color.h"


static void print_binary(uint32_t value, uint8_t bits) {
  uint32_t bit_mask = 1ul << (bits-1);
  while(bit_mask) {
    fputc((value & bit_mask) ? '1' : '0', stdout);
    bit_mask >>= 1;
  }
}


void dump_register(const RegLayout * const layout, uint32_t value, uint8_t left_pad, bool show_bitmap) {
/*
  HFSR = 0xFFAA3210
    31      23      15      7       
    11111111101010100011001000010000
    ││              │      │      ╰─── VECTTBL  = 0
    ││              ╰──────┴────────── RESERVED = 00110010
    │╰──────────────────────────────── FORCED   = 1
    ╰───────────────────────────────── DEBUGEVT = 1
*/
  int nibbles = (layout->reg_bits+3) / 4;

  // Name
  bprintf("%*s" A_BMAG "%s" A_NONE " = 0x%0*" PRIX32 "\n", left_pad, "", layout->name,
          nibbles, value & ((1ul << layout->reg_bits)-1));

  left_pad += 2;

  if(show_bitmap) {
    // Numeric bit positions
    printf("%*s" A_YLW, left_pad, "");
    int bit_num = ((layout->reg_bits / 8) * 8) - 1;
    if(bit_num < layout->reg_bits-1)  // Pad to 8-bit boundary
      printf("%*s", layout->reg_bits-1 - bit_num, "");

    while(bit_num > 0) {
      printf("%-8d", bit_num);
      bit_num -= 8;
    }
    puts(A_NONE);

    // Binary value
    printf("%*s", left_pad, "");
#if 0
    print_binary(value, layout->reg_bits);
#else
    // Print binary nibbles in alternating color
    for(int n = nibbles; n > 0; n--) {
      fputs((n & 0x01) ? A_NONE : A_CYN, stdout);
      uint8_t nibble_bits = 4;
      if(n*4 > layout->reg_bits)  // Adjust if not a multiple of 4
        nibble_bits = 4 - (n*4 - layout->reg_bits);

      print_binary(value >> ((n-1)*4), nibble_bits);
    }
#endif
    putnl();
  }

#define HORZ_LINE         u8"\u2500"
#define VERT_LINE         u8"\u2502"
#define TEE_UP            u8"\u2534"
#define ARC_BOTTOM_LEFT   u8"\u2570"

  // Scan through field definitions
  uint32_t field_mask = 0;
  int field_count = 0;
  int max_name_len = 0;
  for(int i = 0; layout->fields[i].high_bit >= 0; i++) {
    field_mask |= 1ul << layout->fields[i].high_bit;
    field_mask |= 1ul << layout->fields[i].low_bit;
    int name_len = strlen(layout->fields[i].name);
    if(name_len > max_name_len)
      max_name_len = name_len;
    field_count++;
  }
  field_mask &= (1ul << layout->reg_bits) - 1;  // Remove out of range fields

  // Fields are ordered from high to low in array so we work backwards
  for(int i = field_count-1; i >= 0; i--) {
    // Clear this field bit from mask
    field_mask &= ~(1ul << layout->fields[i].high_bit);
    field_mask &= ~(1ul << layout->fields[i].low_bit);

    bprintf("%*s", left_pad, "");

    uint8_t field_size = layout->fields[i].high_bit - layout->fields[i].low_bit + 1;

    if(show_bitmap) {
      // Print leading vertical lines
      if(field_mask) {
        for(int b = layout->reg_bits-1; b > layout->fields[i].high_bit; b--) {
          fputs((field_mask & (1ul << b)) ? VERT_LINE : " ", stdout);
        }
      } else { // Last line
        // Pad to place corner of field in highest bit position
        printf("%*s", layout->reg_bits-1 - layout->fields[i].high_bit, "");
      }

      fputs(ARC_BOTTOM_LEFT, stdout); // Turn corner
      if(field_size > 1) {  // Print span to last bit in field
        for(int b = field_size-2; b > 0; b--) {
          fputs(HORZ_LINE, stdout);
        }
        fputs(TEE_UP, stdout);
      }

      // Print horizontal line
      for(int b = layout->fields[i].low_bit-1+2; b >= 0; b--) {
        fputs(HORZ_LINE, stdout);
      }
      fputc(' ', stdout);
    }

    uint32_t value_mask = (1ul << field_size) - 1;
    uint32_t field_value = (value >> layout->fields[i].low_bit) & value_mask;
    if(field_value) // Non-zero fields in color
      fputs(A_YLW, stdout);
    printf("%-*s = ", max_name_len, layout->fields[i].name);
    print_binary(field_value, field_size);
    puts(A_NONE);
  }

}

