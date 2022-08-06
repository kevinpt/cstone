#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

#include "cstone/platform.h"
#include "cstone/prop_id.h"
#include "cstone/prop_flags.h"
#include "cstone/debug.h"

#ifndef COUNT_OF
#  define COUNT_OF(a) (sizeof(a) / sizeof(*(a)))
#endif

// Debugging is configured with the P_DEBUG_SYS_LOCAL_VALUE prop.
// This global is used to mirror that setting so we can avoid a hash table
// lookup in performance critical code. event_monitor_task() performs the update.
unsigned int g_debug_level = 0;


#define DEBUG_FLAG_NAME_ITEM(name, ix)  #name,
static const char *s_dbg_flag_names[] = {
  DEBUG_FLAGS(DEBUG_FLAG_NAME_ITEM)
};

static PropFlags s_debug_flags;




void debug_init(void) {
  // Configure system debug flags
  prop_flags_init(&s_debug_flags, P_DEBUG_SYS_LOCAL,
                  s_dbg_flag_names, COUNT_OF(s_dbg_flag_names), NULL);
}

bool debug_flag_set(uint32_t prop, bool value) {
  return prop_flags_set(&s_debug_flags, prop, value);
}

bool debug_flag_get(uint32_t prop) {
  return prop_flags_get(&s_debug_flags, prop);
}

bool debug_flag_set_by_name(const char *flag_name, bool value) {
  return prop_flags_set_by_name(&s_debug_flags, flag_name, value);
}

void debug_flag_dump(void) {
  prop_flags_dump(&s_debug_flags, /*set_flags_only*/ false, COUNT_OF(s_dbg_flag_names)-1);
}


void debug__printf(const char *msg, ...) {
  va_list args;

  va_start(args, msg);
#ifdef PLATFORM_EMBEDDED
#  define DEBUG_STREAM  stdout
#else
#  define DEBUG_STREAM  stderr
#endif

  vfprintf(DEBUG_STREAM, msg, args);
  va_end(args);
}
