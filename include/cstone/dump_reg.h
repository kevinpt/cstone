#ifndef DUMP_REG_H
#define DUMP_REG_H

typedef struct {
  const char * const name;     // Name of this field
  short       high_bit; // MSB bit position
  short       low_bit;  // LSB bit position
} RegField;

#define REG_BIT(name, bit)  {(name), (bit), (bit)}
#define REG_SPAN(name, high, low) {(name), (high), (low)}
#define REG_END  {"", -1, -1}

typedef struct {
  const char * const name;     // Name of the register
  const RegField * const fields;   // Field array. Must be ordered from high bit down to 0.
  short       reg_bits; // Number of bits in the register
} RegLayout;


#ifdef __cplusplus
extern "C" {
#endif

void dump_register(const RegLayout * const layout, uint32_t value, uint8_t left_pad, bool show_bitmap);

#ifdef __cplusplus
}
#endif

#endif // DUMP_REG_H
