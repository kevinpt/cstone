#ifndef PROFILE_H
#define PROFILE_H

#include "util/stats.h"

typedef uint32_t (*ProfileTimerCount)(void);


#ifdef __cplusplus
extern "C" {
#endif

void profile_init(ProfileTimerCount get_timer_count, uint32_t timer_clock_hz, int max_profiles);
void profile_calibrate(void);

uint32_t profile_add(uint32_t id, const char *name);
void profile_delete(uint32_t id);
void profile_delete_all(void);

void profile_start(uint32_t id);
void profile_stop(uint32_t id);
void profile_reset(uint32_t id);

void profile_report(uint32_t id);
void profile_report_all(void);

#ifdef __cplusplus
}
#endif

#endif // PROFILE_H
