#ifndef PLATFORM_H
#define PLATFORM_H

// This defines a variety of PLATFORM_* macros so that target dependent code
// has a consistent prefix that can be grepped for auditing.


// Determine target runtime platform to handle inconsistencies between C libraries
#if defined __linux__
#  define PLATFORM_LINUX 1

#elif defined newlib
// NOTE: "newlib" macro isn't provided by the newlib library. You must set
// it in your build script for targets linking with newlib.
#  define PLATFORM_NEWLIB 1

#else
#  define PLATFORM_UNKNOWN 1
# error "Unknown platform"
#endif

// Detect target architecture
#if defined __arm__
#  define PLATFORM_ARCH_ARM 1
#endif

#if defined __x86_64__ || defined __i386__
#  define PLATFORM_ARCH_INTEL 1
#endif


// Catchall for determining if we are on an embedded target and can expect access to
// typical microcontroller peripherals.
#if defined PLATFORM_LINUX
#  if !defined PLATFORM_HOSTED
#    define PLATFORM_HOSTED 1
#  endif
#else
#  define PLATFORM_EMBEDDED 1
#endif



// ******************** Platform dependent features ********************

// Newlib doesn't support printf() %zu specifier
#if defined PLATFORM_NEWLIB // Newlib on ARM32
// arm-none-eabi has size_t as "unsigned int" but uint32_t is "long unsigned int". Crazy.
// To format uint32_t portably you will have to use PRIu32 since %u won't always work.

// Formats for size_t:
#  define PRIuz "u"
#  define PRIXz "X"

#else // Use standard size_t format specifiers
#  define PRIuz "zu"
#  define PRIXz "zX"
#endif




#if (!defined PLATFORM_HAS_ATOMICS && defined __STDC_VERSION__ && __STDC_VERSION__ >= 201112L \
    && !defined __STDC_NO_ATOMICS__)
#  define PLATFORM_HAS_ATOMICS
#endif


// Heap allocation wrappers for cstone library
#define cs_malloc(s)      malloc(s)
#define cs_calloc(n, s)   calloc((n), (s))
#define cs_realloc(p, s)  realloc((p), (s))
#define cs_free(p)        free(p)


#endif // PLATFORM_H
