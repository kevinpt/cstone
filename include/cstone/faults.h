#ifndef FAULTS_H
#define FAULTS_H


typedef enum {
  FAULT_HARD,
  FAULT_BUS,
  FAULT_MEM,
  FAULT_USAGE
} FaultOrigin;

typedef struct {
  uint32_t R0;  // Top of fault stack
  uint32_t R1;
  uint32_t R2;
  uint32_t R3;
  uint32_t R12;
  uint32_t LR;
  uint32_t PC;
  uint32_t PSR;
} CMExceptionFrame;

typedef struct __attribute__(( packed )) {
  FaultOrigin origin;
  uint32_t HFSR;  // Hard fault status
  uint32_t CFSR;  // UsageFault, BusFault, and MemManage
  uint32_t DFSR;  // Debug fault
  uint32_t BFAR;  // Bus fault address
  uint32_t MMFAR;  // MemManage fault address
  CMExceptionFrame frame;
  uint16_t crc;
} SysFaultRecord;


// FIXME: Move to separate file
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


extern SysFaultRecord g_fault_record;
extern SysFaultRecord g_prev_fault_record;


#define IS_DEBUGGER_ATTACHED() (CoreDebug->DHCSR & CoreDebug_DHCSR_C_DEBUGEN_Msk)


#ifdef __cplusplus
extern "C" {
#endif

void fault_record_init(SysFaultRecord *fault_record, FaultOrigin origin, CMExceptionFrame *frame);
bool fault_record_is_valid(SysFaultRecord *fault_record);

void trigger_fault_div0(void);
int trigger_fault_illegal_instruction(void);
int trigger_fault_bad_addr(void);
int trigger_fault_stack(void);

bool report_faults(SysFaultRecord *fault_record, bool verbose);

void dump_register(const RegLayout * const layout, uint32_t value, uint8_t left_pad, bool show_bitmap);

#ifdef __cplusplus
}
#endif

#endif // FAULTS_H
