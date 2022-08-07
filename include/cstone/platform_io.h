#ifndef PLATFORM_IO_H
#define PLATFORM_IO_H

#include "cstone/platform.h"

#ifdef PLATFORM_LINUX
#  include <unistd.h> // For usleep()
#endif

// ******************** Basic print wrappers ********************

#ifdef PLATFORM_EMBEDDED
// Use newlib iprintf() for reduced scope without floating point support
#  define printf      iprintf
#  define fprintf     fiprintf
#  define sprintf     siprintf
#  define snprintf    sniprintf
#  define vprintf     viprintf
#  define vfprintf    vfiprintf
#  define vsprintf    vsiprintf
#  define vsnprintf   vsniprintf
#  define vasprintf   vasiprintf
#  define vasnprintf  vasniprintf
#endif

#if 0
// Alternate printf() implementation
#  include "printf.h"
#  define fputs(s, fh)  printf("%s", s)
#  define puts(s)       printf("%s\n", s)
#  define putc(c, fh)   printf("%c", c)
#endif



// ******************** Blocking stdout control ********************

// Place these macros around printing statements that you need to
// block the task/process when the output buffer is full.
#if defined PLATFORM_EMBEDDED
#  define BLOCK_IO_START()  \
    bool old__block_mode = false; \
    do { \
      Console *bio__con = active_console(); \
      if(bio__con) \
        old__block_mode = console_blocking_stdout(bio__con, true); \
    } while(0)

#  define BLOCK_IO_END()  \
    do { \
      Console *bio__con = active_console(); \
      if(bio__con) \
        console_blocking_stdout(bio__con, old__block_mode); \
    } while(0)


#elif defined PLATFORM_LINUX
#  define BLOCK_IO_START()
// Linux console drops characters when buffering disabled by setvbuf().
// Looks like a bad interaction with pthreads but cause isn't clear.
#  define BLOCK_IO_END()    usleep(1000) // Minimizes but doesn't fully resolve the issue

#else
#  define BLOCK_IO_START()
#  define BLOCK_IO_END()
#endif

#endif // PLATFORM_IO_H
