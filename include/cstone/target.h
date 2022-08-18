#ifndef TARGET_H
#define TARGET_H

// FIXME: Convert to X-macro
typedef enum {
  RESET_UNKNOWN = 0,
  RESET_WATCHDOG,
  RESET_SLEEP,
  RESET_PIN,
  RESET_SOFTWARE,
  RESET_POWERON,
  RESET_BROWNOUT
} ResetSource;


#ifdef __cplusplus
extern "C" {
#endif

void software_reset(void);

ResetSource get_reset_source(void);
void report_reset_source(void);

#ifdef __cplusplus
}
#endif

#endif // TARGET_H
