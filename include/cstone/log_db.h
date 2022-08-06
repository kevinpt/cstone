#ifndef LOG_DB_H
#define LOG_DB_H

#include "storage.h"


typedef struct {
  StorageConfig storage;

  size_t  latest_offset;  // Location of newest valid block
  size_t  head_offset;    // Location for next block
  size_t  read_offset;    // Read iterator position
  size_t  tail_sector;    // Oldest valid sector
  bool    generation;     // Current generation for new blocks
  bool    tail_filled;
  bool    read_iter_start;
} LogDB;


typedef struct {
  uint8_t   kind        : 6;  // User defined block ID
  uint8_t   compressed  : 1;  // Block data is compressed
  uint8_t   generation  : 1;  // Flag transition between latest and oldest block
  uint8_t   header_crc;
  uint16_t  data_crc;
  uint16_t  data_len;
  uint8_t   data[];
} LogDBBlock;


#define BLOCK_KIND_PROP_DB  0x01
#define BLOCK_KIND_DEBUG2   0x02
#define BLOCK_KIND_DEBUG3   0x03


#ifdef __cplusplus
extern "C" {
#endif

void logdb_init(LogDB *db, StorageConfig *cfg); // Configure logdb instance
size_t logdb_size(LogDB *db);
void logdb_format(LogDB *db); // Wipe all data
bool logdb_mount(LogDB *db);  // Scan data for active blocks

bool logdb_write_block(LogDB *db, LogDBBlock *block);

void logdb_read_init(LogDB *db);  // Reset read iterator to oldest block
bool logdb_read_next(LogDB *db, LogDBBlock *block); // Read from iterator and advance
bool logdb_read_next_header(LogDB *db, LogDBBlock *block, size_t *block_start);
bool logdb_read_last(LogDB *db, LogDBBlock *block); // Read newest block
bool logdb_at_last_block(LogDB *db);  // Read iterator is at last block
bool logdb_validate_header(LogDBBlock *block);

bool logdb_read_raw(LogDB *db, size_t block_start, uint8_t *dest, size_t block_size);


#ifdef __cplusplus
}
#endif

#endif // LOG_DB_H
