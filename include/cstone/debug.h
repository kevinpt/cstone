#ifndef DEBUG_H
#define DEBUG_H

#include "cstone/term_color.h"


// Runtime debug levels
#define DEBUG_LEVEL_NONE    0
#define DEBUG_LEVEL_BASIC   1
#define DEBUG_LEVEL_VERBOSE 2

#define DEBUG_IS_ON         (g_debug_level > DEBUG_LEVEL_NONE)
#define DEBUG_IS_VERBOSE    (g_debug_level >= DEBUG_LEVEL_VERBOSE)

#define DEBUG_FEATURE(flag) (DEBUG_IS_ON && debug_flag_get(flag))


// Debugging is configured with the P_DEBUG_SYS_LOCAL_VALUE prop.
// This global is used to mirror that setting so we can avoid a hash table
// lookup in performance critical code.
extern unsigned int g_debug_level;


// FIXME: Move flags to an app specific header
#ifdef PROP_ID_H
// Prefix for system debug flags
#  define P_DEBUG_SYS_LOCAL  (P1_DEBUG | P2_SYS | P3_LOCAL | P3_ARR(0))

// FIXME: Replace these placeholders
#  define DEBUG_FLAGS(M) \
  M(FEATURE0, 0) \
  M(F1,    1) \
  M(FEATURE2, 2) \

#  define DEBUG_FLAG_ENUM_ITEM(name, ix)  PF_DEBUG_SYS_LOCAL_##name = P_DEBUG_SYS_LOCAL | P3_ARR(ix),
enum DebugFlags {
  DEBUG_FLAGS(DEBUG_FLAG_ENUM_ITEM)
};
#endif


// ******************** Debug printing ********************

// Formatting helpers
#define DEBUG_PREFIX(c)   u8"\u2770" #c u8"\u2771"  // ❰c❱
#define ERROR_PREFIX      A_BRED DEBUG_PREFIX(E)
#define WARN_PREFIX       A_BYLW DEBUG_PREFIX(W)
#define SUCCESS_PREFIX    A_BGRN u8"\u2714" // ✔
#define FAIL_PREFIX       A_BRED u8"\u2718" // ✘


#define EMOJI_BUG u8"\U0001F41E"  // 🐞

#ifdef NDEBUG
#  define DPUTS(msg)
#  define DPRINT(...)
#else

#  define DPUTS(msg)  fputs(EMOJI_BUG A_GRN " ", stdout), fputs(__func__, stdout), \
                      puts(": " A_BGRN msg A_NONE)

#  define va_debug_print(fmt, ...)  debug__printf(EMOJI_BUG A_GRN " %s: " A_BGRN fmt A_NONE "%c", \
                                                  __func__, __VA_ARGS__)

// NOTE: Including '\n' as a default argument is a hack to make __VA_ARGS__ work with an empty
// argument list
#  define DPRINT(...) va_debug_print(__VA_ARGS__, '\n')
#endif



#ifdef __cplusplus
extern "C" {
#endif

void debug_init(void);
static inline void debug_set_level(unsigned debug_level) {
  g_debug_level = debug_level;
}

bool debug_flag_set(uint32_t prop, bool value);
bool debug_flag_set_by_name(const char *flag_name, bool value);
bool debug_flag_get(uint32_t prop);
void debug_flag_dump(void);

void debug__printf(const char *msg, ...);

#ifdef __cplusplus
}
#endif

#endif // DEBUG_H
