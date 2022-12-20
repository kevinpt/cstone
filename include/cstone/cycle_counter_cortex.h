#ifndef CYCLE_COUNTER_CORTEX_H
#define CYCLE_COUNTER_CORTEX_H

#ifdef __cplusplus
extern "C" {
#endif

void cycle_counter_init(void);
uint32_t cycle_count(void);

#ifdef __cplusplus
}
#endif

#endif // CYCLE_COUNTER_CORTEX_H
