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

#ifdef __cplusplus
}
#endif

#endif // FAULTS_H
