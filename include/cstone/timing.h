#ifndef TIMING_H
#define TIMING_H

#ifdef __cplusplus
extern "C" {
#endif

unsigned long millis(void);
unsigned long micros(void);

void delay_millis(uint32_t msec);


#ifdef __cplusplus
}
#endif

#endif // TIMING_H
