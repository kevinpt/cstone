#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "cstone/prop_id.h"
#include "cstone/debug.h"

#include "bsd/string.h"
#include "util/list_ops.h"
#include "util/minmax.h"
#include "util/stats.h"
#include "util/mempool.h"
#include "util/num_format.h"
#include "util/getopt_r.h"

#include "cstone/profile.h"

#ifndef COUNT_OF
#  define COUNT_OF(a) (sizeof(a) / sizeof(*(a)))
#endif

// FIXME: Move to cstone common header
// Newlib doesn't support printf() %zu specifier so we detect the target platform
#if defined __linux__ || defined __APPLE__ || defined _WIN32  // System libc for full OS env.
#  define PRIuz "zu"
#  define PRIXz "zX"
// Baremetal:
#elif defined __arm__ // Newlib on ARM32
// arm-none-eabi has size_t as "unsigned int" but uint32_t is "long unsigned int". Crazy
#  define PRIuz "u"
#  define PRIXz "X"
#elif defined __aarch64__ // Newlib on ARM64
#  define PRIuz "lu"
#  define PRIXz "lX"
#else
#  error "Unknown target platform"
#endif


typedef struct ProfileItem {
  struct ProfileItem *next;
  uint32_t id;
  uint32_t start_time;

  OnlineStats stats;
  uint32_t min_elapsed;
  uint32_t max_elapsed;

  char name[16];
  bool active;

} ProfileItem;


struct ProfileState {
  ProfileTimerCount get_timer_count;
  uint32_t timer_clock_hz;
  uint32_t fixed_overhead;

  ProfileItem *profile_list;
  int num_profiles;
  int max_profiles;
};


static struct ProfileState s_prof_state = {0};




void profile_init(ProfileTimerCount get_timer_count, uint32_t timer_clock_hz, int max_profiles) {
  s_prof_state.get_timer_count = get_timer_count;
  s_prof_state.timer_clock_hz = timer_clock_hz;
  s_prof_state.fixed_overhead = 0;

  if(s_prof_state.profile_list)
    profile_delete_all();

  s_prof_state.profile_list = NULL;
  s_prof_state.num_profiles = 0;
  s_prof_state.max_profiles = max_profiles;

  profile_calibrate();
}


static ProfileItem *profile__find(uint32_t id) {
  ProfileItem *cur = s_prof_state.profile_list;
  while(cur) {
    if(cur->id == id)
      return cur;
    cur = cur->next;
  }

  return NULL;
}


static ProfileItem *profile__find_by_name(const char *name) {
  ProfileItem *cur = s_prof_state.profile_list;
  while(cur) {
    if(!strcmp(cur->name, name))
      return cur;
    cur = cur->next;
  }

  return NULL;
}


void profile_calibrate(void) {
  // Get overhead for profiling calls
  uint32_t id = profile_add(0, NULL);

  if(id == 0) return;
  s_prof_state.fixed_overhead = 0;

  for(int i = 0; i < 20; i++) {
    profile_start(id);
    profile_stop(id);
  }

  ProfileItem *p = profile__find(id);
  if(!p) return;

  s_prof_state.fixed_overhead = p->min_elapsed;

  DPRINT("Fixed overhead: %"PRIu32" cycles,  %"PRIu32" ns", s_prof_state.fixed_overhead, 
          (uint32_t)(s_prof_state.fixed_overhead * 1000000ul / (s_prof_state.timer_clock_hz/1000)));

  profile_delete(id);
}


uint32_t profile_add(uint32_t id, const char *name) {
  // Check for existing name or id
  if(name) {
    ProfileItem *p = profile__find_by_name(name);
    if(p) return p->id;
  } else if(id != 0 && profile__find(id)) {
    return id;
  }

  if(s_prof_state.max_profiles > 0 && s_prof_state.num_profiles >= s_prof_state.max_profiles)
    return 0;

  ProfileItem *new_item = mp_alloc(mp_sys_pools(), sizeof(ProfileItem), NULL);
  if(!new_item)
    return 0;


  if(id == 0)
    id = prop_new_global_id();

  memset(new_item, 0, sizeof *new_item);
  new_item->id = id;
  stats_init(&new_item->stats, 0);
  new_item->min_elapsed = UINT32_MAX;
  new_item->max_elapsed = 0;

  if(name)
    strlcpy(new_item->name, name, sizeof new_item->name);
  else
    snprintf(new_item->name, sizeof new_item->name, PROP_ID, id);

  ll_slist_push(&s_prof_state.profile_list, new_item);
  s_prof_state.num_profiles++;

  return id;
}


void profile_delete(uint32_t id) {
  ProfileItem *p = profile__find(id);
  if(!p) return;

  ll_slist_remove(&s_prof_state.profile_list, p);
  mp_free(mp_sys_pools(), p);
  s_prof_state.num_profiles--;
}


void profile_delete_all(void) {
  ProfileItem *p;
  while((p = (ProfileItem *)ll_slist_pop(&s_prof_state.profile_list))) {
    mp_free(mp_sys_pools(), p);
  }

  s_prof_state.num_profiles = 0;
}


void profile_start(uint32_t id) {
  ProfileItem *p = profile__find(id);
  if(!p) return;

  if(!p->active) {
    p->start_time = s_prof_state.get_timer_count();
    p->active = true;
  }
}


void profile_stop(uint32_t id) {
  uint32_t now = s_prof_state.get_timer_count();

  ProfileItem *p = profile__find(id);
  if(!p) return;

  if(p->active) {
    uint32_t elapsed = (now - p->start_time);
    if(elapsed > s_prof_state.fixed_overhead)
      elapsed -= s_prof_state.fixed_overhead;

    stats_add_sample(&p->stats, elapsed);
    p->min_elapsed = min(p->min_elapsed, elapsed);
    p->max_elapsed = max(p->max_elapsed, elapsed);

    p->active = false;  
  }
}



static void profile__reset(ProfileItem *p) {
  p->active = false;
  p->start_time = 0;
  stats_init(&p->stats, 0);
  p->min_elapsed = UINT32_MAX;
  p->max_elapsed = 0;
}


void profile_reset(uint32_t id) {
  ProfileItem *p = profile__find(id);
  if(!p) return;
  profile__reset(p);
}


void profile_reset_all(void) {
  for(ProfileItem *cur = s_prof_state.profile_list; cur; cur = cur->next) {
    profile__reset(cur);
  }
}


#define US_SCALE  1000000ul
#define NS_SCALE  1000000000ul

static uint32_t select_fixed_scaling(uint32_t max_val, int *fp_exp ) {
  uint32_t fp_scale;

  if((max_val * NS_SCALE) / NS_SCALE == max_val) { // Scale to nsecs
    *fp_exp = -9;
    fp_scale = NS_SCALE;
  } else { // Scale to usecs
    *fp_exp = -6;
    fp_scale = US_SCALE;
  }

  return fp_scale;
}


static void profile__report(ProfileItem *p, bool heading) {
  if(heading) {
    puts(A_YLW "    Name        Count    Avg        Min        Max");
    puts(    u8"  ───────────────────────────────────────────────────" A_NONE);
  }

  printf("  %-12s %5"PRIuz, p->name, p->stats.count);

  // Select scale factor for fixed-point conversion
  uint32_t fp_scale;
  int fp_exp;
  fp_scale = select_fixed_scaling(p->max_elapsed > 0 ? p->max_elapsed : p->min_elapsed, &fp_exp);

  uint32_t tvals[3] = {stats_mean(&p->stats), p->min_elapsed, p->max_elapsed};

  for(unsigned i = 0; i < COUNT_OF(tvals); i++) {
    char si_buf[10]; // 3 + "." + 2 + " X" = 8
    // Convert cycles to scaled time
    uint32_t fp_time = (uint64_t)tvals[i] * fp_scale / s_prof_state.timer_clock_hz;
    to_si_value(fp_time, fp_exp, si_buf, sizeof si_buf, /*frac_places*/2, SIF_GREEK_MICRO);
    printf(" %9ss", si_buf);
  }
  puts("");
}


void profile_report(uint32_t id) {
  ProfileItem *p = profile__find(id);
  if(!p) return;

  profile__report(p, /*heading*/true);
}


void profile_report_all(void) {
  ProfileItem *cur = s_prof_state.profile_list;
  bool first_profile = true;

  while(cur) {
    profile__report(cur, first_profile);
    first_profile = false;
    cur = cur->next;
  }
}





int32_t cmd_profile(uint8_t argc, char *argv[], void *eval_ctx) {
  GetoptState state = {.report_errors = true};
  int c;

  bool reset = false;

  while((c = getopt_r(argv, "rh", &state)) != -1) {
    switch(c) {
    case 'r': reset = true; break;

    case 'h':
      puts("PROFile [-r] [-h]");
      return 0;
      break;

    default:
    case ':':
    case '?':
      return -3;
      break;
    }
  }

  if(reset) {
    puts("Reset profile stats");
    profile_reset_all();

  } else {
    char si_buf[10];
    to_si_value(s_prof_state.timer_clock_hz, 0, si_buf, sizeof si_buf, /*frac_places*/1, SIF_SIMPLIFY);
    printf("Timer freq: %sHz\n", si_buf);

    //uint32_t fp_scale;
    int fp_exp;
    select_fixed_scaling(UINT32_MAX, &fp_exp);
    //printf("## fp_scale: %"PRIu32"  fp_exp: %d\n", fp_scale, fp_exp);

    uint32_t fp_time = UINT32_MAX / s_prof_state.timer_clock_hz;
    to_si_value(fp_time, fp_exp, si_buf, sizeof si_buf, /*frac_places*/2, SIF_GREEK_MICRO);
    printf("Max profile interval: %ss\n\n", si_buf);

    profile_report_all();
  }

  return 0;
}

