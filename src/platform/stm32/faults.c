#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "stm32f4xx.h"

#include "cstone/faults.h"
#include "util/crc16.h"
#include "cstone/term_color.h"
#include "cstone/blocking_io.h"
#include "lib_cfg/build_config.h"

#ifndef COUNT_OF
#  define COUNT_OF(a) (sizeof(a) / sizeof(*(a)))
#endif

// ******************** Fault log ********************

// https://blog.feabhas.com/2018/09/updated-developing-a-generic-hard-fault-handler-for-arm-cortex-m3-cortex-m4-using-gcc/
// https://community.silabs.com/s/article/how-to-read-the-link-register-lr-for-an-arm-cortex-m-series-device?language=en_US
// https://interrupt.memfault.com/blog/cortex-m-fault-debug

__attribute__(( section(".noinit") ))
SysFaultRecord g_fault_record;

SysFaultRecord g_prev_fault_record = {0};


void fault_record_init(SysFaultRecord *fault_record, FaultOrigin origin, CMExceptionFrame *frame) {
  fault_record->origin = origin;
  fault_record->HFSR = SCB->HFSR;
  fault_record->CFSR = SCB->CFSR;  // UsageFault, BusFault, and MemManage
  fault_record->DFSR = SCB->DFSR;
  fault_record->BFAR = SCB->BFAR;
  fault_record->MMFAR = SCB->MMFAR;

  fault_record->frame = *frame;

  uint16_t crc = crc16_init();
  crc = crc16_update_block(crc, (uint8_t *)fault_record, offsetof(SysFaultRecord, crc));
  fault_record->crc = crc16_finish(crc);
}


bool fault_record_is_valid(SysFaultRecord *fault_record) {
  uint16_t crc = crc16_init();
  crc = crc16_update_block(crc, (uint8_t *)fault_record, offsetof(SysFaultRecord, crc));
  return fault_record->crc == crc16_finish(crc);
}




// ******************** Fault generation ********************

// https://interrupt.memfault.com/blog/cortex-m-fault-debug#halting--determining-core-register-state

void trigger_fault_div0(void) {
  // Result: Usage fault with div-by-zero flagged (Depends on enabling SCB_CCR_DIV_0_TRP)
  volatile uint32_t q = 1;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdiv-by-zero"
  q = q / 0;  // Trigger fault
#pragma GCC diagnostic pop
  printf("Div0: %lu\n", q); // Should never reach this
}

int trigger_fault_illegal_instruction(void) {
  // Result: Memory fault from invalid instruction access
  int (*bad_entry)(void) = (int(*)(void))0xE0000000; // System space address marked as XN
  return bad_entry();
}

int trigger_fault_bad_addr(void) {
  // Result: Bus fault from invalid address
  return *(volatile uint32_t *)0xBADADD1; // Unaligned read
}


__attribute__(( noinline ))
static int corrupt_stack(uint8_t *buf) {
  memset(buf, 0xFF, 64);
  return 0;
}

int trigger_fault_stack(void) {
  // Result: Memory fault from corruption of this stack frame by callee
  uint8_t stack_var;
  return corrupt_stack(&stack_var);
}



// ******************** Fault reporting ********************

static RegField s_reg_hfsr_fields[] = {
  REG_BIT("DEBUGEVT", 31),
  REG_BIT("FORCED", 30),  // Lower priority exception forced to hard fault
  REG_BIT("VECTTBL", 1),
  REG_END
};

static RegLayout s_reg_hfsr = {
  .name     = "HFSR",
  .fields   = s_reg_hfsr_fields,
  .reg_bits = 32
};

static RegField s_reg_cfsr_fields[] = {
  // UsageFault
  REG_BIT("DIVBYZERO", 25),
  REG_BIT("UNALIGNED", 24),
  REG_BIT("NOCP", 19),
  REG_BIT("INVPC", 18),
  REG_BIT("INVSTATE", 17),
  REG_BIT("UNDEFINSTR", 16),

  // BusFault
  REG_BIT("BFARVALID", 15),
  REG_BIT("LSPERR", 13),
  REG_BIT("STKERR", 12),
  REG_BIT("UNSTKERR", 11),
  REG_BIT("IMPRECISERR", 10),
  REG_BIT("PRECISERR", 9),
  REG_BIT("IBUSERR", 8),

  // MemManage
  REG_BIT("MMARVALID", 7),
  REG_BIT("MLSPERR", 5),
  REG_BIT("MSTKERR", 4),
  REG_BIT("MUNSTKERR", 3),
  REG_BIT("DACCVIOL", 1),
  REG_BIT("IACCVIOL", 0),
  REG_END
};

static RegLayout s_reg_cfsr = {
  .name     = "CFSR",
  .fields   = s_reg_cfsr_fields,
  .reg_bits = 32
};


static RegField s_reg_psr_fields[] = {
  REG_BIT("N", 31),
  REG_BIT("Z", 30), // Zero flag
  REG_BIT("C", 29), // Carry flag
  REG_BIT("V", 28), // Overflow flag
  REG_BIT("Q", 27), // Saturation flag
  REG_SPAN("ICI/IT", 26, 25),
  REG_BIT("T", 24), // Thumb mode
  REG_SPAN("GE", 19, 16),
  REG_SPAN("ICI/IT", 15, 10),
  REG_SPAN("Exception", 8, 0),
  REG_END
};

static RegLayout s_reg_psr = {
  .name     = "PSR",
  .fields   = s_reg_psr_fields,
  .reg_bits = 32
};



bool report_faults(SysFaultRecord *fault_record, bool verbose) {
  if(!fault_record_is_valid(fault_record))
    return false;

  puts(A_BRED "Fault event:" A_NONE);

  // Dump captured registers
  struct RegItem {
    const char *name;
    size_t reg_offset;
  };

  static const struct RegItem reg_items[] = {
    {"HFSR",  offsetof(SysFaultRecord, HFSR)},
    {"CFSR",  offsetof(SysFaultRecord, CFSR)},
    {"R0",    offsetof(SysFaultRecord, frame) + offsetof(CMExceptionFrame, R0)},
    {"R1",    offsetof(SysFaultRecord, frame) + offsetof(CMExceptionFrame, R1)},
    {"R2",    offsetof(SysFaultRecord, frame) + offsetof(CMExceptionFrame, R2)},
    {"R3",    offsetof(SysFaultRecord, frame) + offsetof(CMExceptionFrame, R3)},
    {"R12",   offsetof(SysFaultRecord, frame) + offsetof(CMExceptionFrame, R12)},
    {"PC",    offsetof(SysFaultRecord, frame) + offsetof(CMExceptionFrame, PC)},
    {"LR",    offsetof(SysFaultRecord, frame) + offsetof(CMExceptionFrame, LR)},
    {"PSR",   offsetof(SysFaultRecord, frame) + offsetof(CMExceptionFrame, PSR)},
  };

  for(size_t i = 0; i < COUNT_OF(reg_items); i++) {
    uint32_t *reg = (uint32_t *)((char *)fault_record + reg_items[i].reg_offset);
    printf("  %-5s: %08" PRIX32 "\n", reg_items[i].name, *reg);
  }


  if(verbose) {
#ifndef NDEBUG
    // Get location of exception
    printf("\naddr2line -e " APP_BUILD_IMAGE " -f -C -a %08" PRIx32 "\n", fault_record->frame.PC);
#endif

    // Summary of fault origin
    switch(fault_record->origin) {
    case FAULT_HARD:  puts("\nHard fault"); break;
    case FAULT_BUS:   puts("\nBus fault"); break;
    case FAULT_MEM:   puts("\nMemory fault"); break;
    case FAULT_USAGE: puts("\nUsage fault"); break;
    }

    if(fault_record->CFSR & SCB_CFSR_IMPRECISERR_Msk)
      puts("  Imprecise error. Diagnose by enabling ACTLR:DISDEFWBUF");

    if(fault_record->CFSR & SCB_CFSR_MMARVALID_Msk)
      printf("  Memory fault @ 0x%08" PRIx32 "\n", fault_record->MMFAR);
    else if(fault_record->CFSR & SCB_CFSR_BFARVALID_Msk) {
      printf("  Bus fault @ 0x%08" PRIx32 "\n", fault_record->BFAR);
    }

    // Detailed register dumps
    putnl();
    dump_register(&s_reg_hfsr, fault_record->HFSR, 2, /*show_bitmap*/true);
    dump_register(&s_reg_cfsr, fault_record->CFSR, 2, /*show_bitmap*/true);
    dump_register(&s_reg_psr, fault_record->frame.PSR, 2, /*show_bitmap*/true);
  }

  return true;
}






// FIXME: Relocate this code
static void print_binary(uint32_t value, uint8_t bits) {
  uint32_t bit_mask = 1ul << (bits-1);
  while(bit_mask) {
    fputc((value & bit_mask) ? '1' : '0', stdout);
    bit_mask >>= 1;
  }
}


void dump_register(RegLayout *layout, uint32_t value, uint8_t left_pad, bool show_bitmap) {
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

