#ifndef ERROR_LOG_H
#define ERROR_LOG_H

#include "cstone/storage.h"

typedef struct {
  StorageConfig storage;

  size_t  entries_per_sector;
  size_t  latest_offset;  // Location of newest valid block
  size_t  head_offset;    // Location for next block
  size_t  read_offset;    // Read iterator position
  size_t  tail_sector;    // Oldest valid sector
  bool    read_iter_start;
} ErrorLog;


typedef struct {
  uint32_t id;
  uint32_t data;
} ErrorEntry;


#ifdef __cplusplus
extern "C" {
#endif

void errlog_init(ErrorLog *el, StorageConfig *cfg); // Configure error log instance
size_t errlog_size(ErrorLog *el);
void errlog_format(ErrorLog *el); // Wipe all data
bool errlog_mount(ErrorLog *el);  // Scan for current log offset

bool errlog_write(ErrorLog *el, ErrorEntry *entry);

void errlog_read_init(ErrorLog *el);  // Reset read iterator to start of log
bool errlog_read_next(ErrorLog *el, ErrorEntry *entry); // Read from iterator and advance
//bool errlog_read_last(ErrorLog *el, ErrorEntry *entry); // Read newest block
bool errlog_at_end(ErrorLog *el);  // Read iterator is at end of log

bool errlog_read_raw(ErrorLog *el, size_t block_start, uint8_t *dest, size_t block_size);
void errlog_dump_raw(ErrorLog *el, size_t dump_bytes, size_t offset);
void errlog_print_all(ErrorLog *el);

#ifdef __cplusplus
}
#endif

#endif // ERROR_LOG_H
