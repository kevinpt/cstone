#ifndef LOG_INFO_H
#define LOG_INFO_H

#include "cstone/platform.h"

// FIXME: MOVE elsewhere

// ******************** Configuration ********************
//#define LOG_TO_RAM  // Force settings to RAM for debug

#if defined LOG_TO_RAM || !defined PLATFORM_EMBEDDED // Small in-memory filesystem for testing
#  define LOG_NUM_SECTORS 3
#  define LOG_SECTOR_SIZE 128

#else // Log to flash storage
// STORAGE0 in STM32 sectors 1-3
#  define LOG_NUM_SECTORS 3
#  define LOG_SECTOR_SIZE (16 * 1024)
#endif



#ifdef __cplusplus
extern "C" {
#endif

void logdb_dump_raw(LogDB *db, size_t dump_bytes, size_t offset);
void logdb_dump_record(LogDB *db);

// FIXME: This belongs somewhere else
void dump_array_bulk(uint8_t *buf, size_t buf_len, bool show_ascii, bool ansi_color);

#ifdef __cplusplus
}
#endif

#endif // LOG_INFO_H
