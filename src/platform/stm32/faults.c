#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "build_config.h" // Get build-specific platform settings
#ifdef PLATFORM_STM32F1
#  include "stm32f1xx.h"
#else
#  include "stm32f4xx.h"
#endif

#include "cstone/dump_reg.h"
#include "cstone/faults.h"
#include "cstone/blocking_io.h"

#include "util/crc16.h"
#include "util/term_color.h"


#ifndef COUNT_OF
#  define COUNT_OF(a) (sizeof(a) / sizeof(*(a)))
#endif

// Use small table CRC implementation for reduced memory consumption
#define crc16_update_block(crc, data, len)  crc16_update_small_block((crc), (data), (len))


// ******************** Fault log ********************

// https://blog.feabhas.com/2018/09/updated-developing-a-generic-hard-fault-handler-for-arm-cortex-m3-cortex-m4-using-gcc/
// https://community.silabs.com/s/article/how-to-read-the-link-register-lr-for-an-arm-cortex-m-series-device?language=en_US
// https://interrupt.memfault.com/blog/cortex-m-fault-debug

// Storing fault record in .noinit so it will be preserved across resets
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

static const RegField s_reg_hfsr_fields[] = {
  REG_BIT("DEBUGEVT", 31),
  REG_BIT("FORCED", 30),  // Lower priority exception forced to hard fault
  REG_BIT("VECTTBL", 1),
  REG_END
};

static const RegLayout s_reg_hfsr = {
  .name     = "HFSR",
  .fields   = s_reg_hfsr_fields,
  .reg_bits = 32
};

static const RegField s_reg_cfsr_fields[] = {
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

static const RegLayout s_reg_cfsr = {
  .name     = "CFSR",
  .fields   = s_reg_cfsr_fields,
  .reg_bits = 32
};


static const RegField s_reg_psr_fields[] = {
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

static const RegLayout s_reg_psr = {
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

