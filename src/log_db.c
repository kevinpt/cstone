#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "util/crc8.h"
#include "util/crc16.h"
#include "util/minmax.h"
#include "cstone/log_db.h"
#include "cstone/prop_id.h"
#include "cstone/umsg.h"

void logdb_init(LogDB *db, StorageConfig *cfg) {
  memset(db, 0, sizeof(*db));
  memcpy(&db->storage, cfg, sizeof(*cfg));
}

size_t logdb_size(LogDB *db) {
  return db->storage.num_sectors * db->storage.sector_size;
}

// Reset read iterator
void logdb_read_init(LogDB *db) {
  db->read_offset = db->tail_sector * db->storage.sector_size;
  db->read_iter_start = true;
}


static bool logdb__verify_empty(LogDB *db, size_t offset, size_t len) {
  uint32_t buf[8];
  size_t read_bytes;

  while(len > 0) {
    read_bytes = min(sizeof(buf), len);
    db->storage.read_block(db->storage.ctx, offset, (uint8_t *)buf, read_bytes);

    offset += read_bytes;
    len -= read_bytes;

    size_t read_words = read_bytes / sizeof(*buf);
    for(size_t i = 0; i < read_words; i++) {
      if(buf[i] != 0xFFFFFFFF)
        return false;
    }

    read_bytes -= read_words * sizeof(*buf);
    for(size_t i = 0; i < read_bytes; i++) {
      if(((uint8_t *)&buf[read_words])[i] != 0xFF)
        return false;
    }

  }

  return true;
}


void logdb_format(LogDB *db) {
//  puts("## LOG FORMAT");
  for(size_t i = 0; i < db->storage.num_sectors; i++) {
    // Confirm sector isn't already erased
    bool need_erase = !logdb__verify_empty(db, i*db->storage.sector_size, db->storage.sector_size);

    if(need_erase) {
//      printf("## Erase sector %lu\n", i);
      db->storage.erase_sector(db->storage.ctx, i*db->storage.sector_size, db->storage.sector_size);
    }
  }

  db->latest_offset = 0;
  db->head_offset = 0;
  db->tail_sector = 0;
  db->generation = false;
  db->tail_filled = false;

  logdb_read_init(db);
}


bool logdb_validate_header(LogDBBlock *block) {
  // Check header
  LogDBBlock copy = *block;
  copy.header_crc = 0;
  uint8_t header_crc = crc8_init();
  header_crc = crc8_update_small_block(header_crc, (uint8_t *)&copy, sizeof(copy));
  return header_crc == block->header_crc;
}


static bool logdb__validate_block(LogDBBlock *block) {
  if(!logdb_validate_header(block))
    return false;

  // Check data
  uint16_t data_crc = crc16_init();
  data_crc = crc16_update_block(data_crc, block->data, block->data_len);

  return data_crc == block->data_crc;
}





bool logdb_mount(LogDB *db) {
  // Scan sectors for valid blocks and find the most recent one

  LogDBBlock header;

  // If there is any valid data then at least one sector must start with
  // a valid block.
  size_t head_sector = db->storage.num_sectors; // Invalid value
  for(size_t i = 0; i < db->storage.num_sectors; i++) {
    db->storage.read_block(db->storage.ctx, i*db->storage.sector_size, (uint8_t *)&header, sizeof(header));
    if(logdb_validate_header(&header)) {
      head_sector = i;
      break;
    }
  }

  if(head_sector == db->storage.num_sectors) { // No valid block found
    // Storage should be empty
    logdb_format(db);
    return true;

  } else { // Valid block found
    db->generation = header.generation;

    db->tail_sector = head_sector;
    // Find wrap point
    for(size_t i = head_sector + 1; i < db->storage.num_sectors; i++) {
      db->storage.read_block(db->storage.ctx, i*db->storage.sector_size,
                             (uint8_t *)&header, sizeof(header));
      if(logdb_validate_header(&header)) {
        if(header.generation != db->generation) {
          db->tail_sector = i;
          break;
        } else {  // Still in same generation
          head_sector = i;
        }
      }
    }

    if(head_sector != db->tail_sector)
      db->tail_filled = true;


    logdb_read_init(db);

    // Scan through head sector to find last block
    size_t head_offset = head_sector*db->storage.sector_size;
    db->storage.read_block(db->storage.ctx, head_offset, (uint8_t *)&header, sizeof(header));
    size_t block_len = 0;
    while(logdb_validate_header(&header)) {
      block_len = sizeof(header) + header.data_len;
      head_offset += block_len;
      if(head_offset / db->storage.sector_size > head_sector)
        break;

      db->storage.read_block(db->storage.ctx, head_offset, (uint8_t *)&header, sizeof(header));
    }

    db->head_offset = head_offset;
    db->latest_offset = head_offset - block_len;

    // Verify remainder of sector is unprogrammed
    size_t remaining = ((head_sector+1) * db->storage.sector_size) - db->head_offset;
    if(logdb__verify_empty(db, db->head_offset, remaining))
      return true;
  }

  return false;
}


static bool logdb__prep_for_write(LogDB *db, size_t write_len) {
  // We need enough space for the new block and it cannot cross a sector boundary
  size_t write_offset = db->head_offset;
  size_t write_sector = write_offset / db->storage.sector_size;
  size_t end_offset = write_offset + write_len-1;
  size_t end_sector = end_offset / db->storage.sector_size;
  bool erase_sector = false;

  if(end_sector != write_sector) { // Move start up to next sector
    write_offset = end_sector * db->storage.sector_size;
    write_sector = write_offset / db->storage.sector_size;
  }

  if(write_sector >= db->storage.num_sectors) { // Wrap around
    write_offset = 0;
    write_sector = 0;
    erase_sector = true;
    db->generation = !db->generation;
  }

  if(write_sector == db->tail_sector && db->tail_filled) {
    // We need to overwrite the tail
    erase_sector = true;
  }

  if(erase_sector) {
    db->storage.erase_sector(db->storage.ctx, write_sector * db->storage.sector_size,
                             db->storage.sector_size);

    // Bump tail to next sector
    if(db->tail_sector == write_sector) {
      db->tail_sector = (db->tail_sector + 1) % db->storage.num_sectors;
      logdb_read_init(db);
    }
  }

//  printf("## tail: %u\n", db->tail_sector);

  db->head_offset = write_offset;

  return true;
}


bool logdb_write_block(LogDB *db, LogDBBlock *block) {
  size_t block_size = block->data_len + sizeof(*block);

  if(block_size > db->storage.sector_size) { // Will never fit
    report_error(P1_ERROR | P2_STORAGE | P3_LIMIT | P4_VALUE, __LINE__);
    return false;
  }

  if(!logdb__prep_for_write(db, block_size)) {
    report_error(P1_ERROR | P2_STORAGE | P3_TARGET | P4_UPDATE, __LINE__);
    return false;
  }

  block->generation = db->generation;

  block->data_crc = crc16_init();
  block->data_crc = crc16_update_block(block->data_crc, block->data, block->data_len);

  block->header_crc = 0;
  uint8_t header_crc = crc8_init();
  header_crc = crc8_update_small_block(header_crc, (uint8_t *)block, sizeof(*block));
  block->header_crc = header_crc;

  if(db->storage.write_block(db->storage.ctx, db->head_offset, (uint8_t *)block, block_size)) {
    db->latest_offset = db->head_offset;
    db->head_offset += block_size;

    // If we were starting from an empty FS we need to know when the first sector is filled
    // So that it isn't erased prematurely.
    if(db->head_offset / db->storage.sector_size != db->tail_sector)
      db->tail_filled = true;

//    printf("## WRITE: head=%u  tail=%u\n", db->head_offset / db->storage.sector_size, db->tail_sector);
    return true;
  }

  report_error(P1_ERROR | P2_STORAGE | P3_TARGET | P4_UPDATE, __LINE__);
  return false;
}

typedef enum {
  BLOCK_TOO_SMALL,
  BLOCK_VALID,
  BLOCK_BAD
} BlockReadStatus;

// We depend on the caller to provide a block that can hold all of the data.
// If not, we only copy the header over if it's valid. The caller can then retry
// with a suitable block buffer.
static BlockReadStatus logdb__read_block(LogDB *db, size_t block_offset, LogDBBlock *block) {
  LogDBBlock header;
  db->storage.read_block(db->storage.ctx, block_offset, (uint8_t *)&header, sizeof(header));

  if(logdb_validate_header(&header)) {
    if(header.data_len > block->data_len) {// Block won't fit
      memcpy(block, &header, sizeof(header)); // Report required data_len back to caller
      return BLOCK_TOO_SMALL;
    }

    // Get the whole block
    db->storage.read_block(db->storage.ctx, block_offset, (uint8_t *)block,
                            sizeof(*block) + header.data_len);

    if(logdb__validate_block(block))
      return BLOCK_VALID;

  } else {  // Bad header
    block->data_len = 0;
  }

  return BLOCK_BAD;
}



bool logdb_read_next(LogDB *db, LogDBBlock *block) {

  while(db->read_offset != db->tail_sector * db->storage.sector_size || db->read_iter_start) {
    db->read_iter_start = false;
    BlockReadStatus status = logdb__read_block(db, db->read_offset, block);
    switch(status) {
    case BLOCK_VALID:
      db->read_offset += sizeof(*block) + block->data_len;
      if(db->read_offset >= db->storage.num_sectors * db->storage.sector_size) // Last block filled entire last sector
        db->read_offset = 0;
//      printf("## READ: head=%u  tail=%u\n", db->head_offset / db->storage.sector_size, db->tail_sector);
      return true;
      break;

    case BLOCK_TOO_SMALL: // Valid header
      return false;
      break;

    case BLOCK_BAD: // Invalid header; Skip to next sector
      {
        size_t head_sector = db->head_offset / db->storage.sector_size;
        size_t read_sector = db->read_offset / db->storage.sector_size;
        if(read_sector == head_sector) // No more blocks
          goto end_loop;

        read_sector = (read_sector + 1) % db->storage.num_sectors;
        db->read_offset = read_sector * db->storage.sector_size;

      }
      break;
    }

  }

end_loop:
//  printf("## END READ: r=%lu\n", read_sector);
  return false;
}

bool logdb_read_next_header(LogDB *db, LogDBBlock *block, size_t *block_start) {
  block->data_len = 0; // We only want a header

  while(db->read_offset != db->tail_sector * db->storage.sector_size || db->read_iter_start) {
    db->read_iter_start = false;
    size_t read_offset = db->read_offset;
    BlockReadStatus status = logdb__read_block(db, db->read_offset, block);
    switch(status) {
    case BLOCK_VALID:
    case BLOCK_TOO_SMALL: // Valid header
      db->read_offset += sizeof(*block) + block->data_len;
      if(db->read_offset >= db->storage.num_sectors * db->storage.sector_size) // Last block filled entire last sector
        db->read_offset = 0;
//      printf("## READ: head=%u  tail=%u\n", db->head_offset / db->storage.sector_size, db->tail_sector);
      *block_start = read_offset;
      return true;
      break;

    case BLOCK_BAD: // Invalid header; Skip to next sector
      {
        size_t head_sector = db->head_offset / db->storage.sector_size;
        size_t read_sector = db->read_offset / db->storage.sector_size;
        if(read_sector == head_sector) // No more blocks
          goto end_loop;

        read_sector = (read_sector + 1) % db->storage.num_sectors;
        db->read_offset = read_sector * db->storage.sector_size;

      }
      break;
    }

  }

end_loop:
//  printf("## END READ: r=%lu\n", read_sector);
  return false;
}



bool logdb_read_last(LogDB *db, LogDBBlock *block) {
  return logdb__read_block(db, db->latest_offset, block) == BLOCK_VALID;
}

bool logdb_at_last_block(LogDB *db) {
  return db->read_offset == db->latest_offset;
}



bool logdb_read_raw(LogDB *db, size_t block_start, uint8_t *dest, size_t block_size) {
  return db->storage.read_block(db->storage.ctx, block_start, dest, block_size);
}



// ******************** Test ********************

#ifdef TEST_LOGDB

#include <stdio.h>
#include <assert.h>
#include "util/hex_dump.h"

#define SEC_SIZE  64
#define SEC_NUM   4

static uint8_t s_fs_buf[SEC_NUM * SEC_SIZE];


void erase_sector(uint8_t *sector, size_t sector_size) {
  assert((sector - s_fs_buf + sector_size) <= SEC_NUM * sector_size);

  DPUTS("Erase callback");

  memset(sector, 0xFF, sector_size);
}

bool read_block(uint8_t *block_start, uint8_t *dest, size_t block_size) {
//  printf("## READ: %04lX\n", block_start - s_fs_buf);
  assert(block_start >= s_fs_buf);
//  assert(block_start + block_size <= s_fs_buf + sizeof(s_fs_buf));
  assert(block_size <= SEC_SIZE);
//  assert((block_start - s_fs_buf) / SEC_SIZE == (block_start - s_fs_buf + block_size) / SEC_SIZE);

  memcpy(dest, block_start, block_size);
  return true;
}

bool write_block(uint8_t *block_start, uint8_t *src, size_t block_size) {
  memcpy(block_start, src, block_size);
  return true;
}


int main(void) {
  printf("Test\n");

  memset(s_fs_buf, 0xFF, sizeof(s_fs_buf));

  StorageConfig cfg = {
    .sector_size  = SEC_SIZE,
    .num_sectors  = SEC_NUM,
    .ctx     = s_fs_buf,
    .erase_sector = erase_sector,
    .read_block   = read_block,
    .write_block  = write_block,
  };


  LogDB fs;
  logdb_init(&fs, &cfg);
  logdb_format(&fs);

  bool status = logdb_mount(&fs);
  DPRINT("Mount: %c", status ? 't' : 'f');

  _Alignas(LogDBBlock)
  uint8_t data[30];


  for(int i = 1; i < 10; i++) {
    memset(data, i, sizeof(data));
    LogDBBlock *block = (LogDBBlock *)data;

    block->kind = 42;
    block->generation = 0;
    block->data_len = sizeof(data) - sizeof(*block);

    status = logdb_write_block(&fs, block);
    DPRINT("Write: %c", status ? 't' : 'f');

    //dump_array(data, sizeof(data));
  }


//  logdb_read_init(&fs);
  DPRINT("Pre mount: lo=%04lX  head=%lu  tail=%lu  woff=%04lX  roff=%04lX", fs.latest_offset, fs.head_offset / fs.storage.sector_size,
        fs.tail_sector, fs.head_offset, fs.read_offset);

#if 1
//  erase_sector(0);
//  erase_sector(3);
  logdb_mount(&fs);
//  logdb_read_init(&fs);
  DPRINT("Post mount: lo=%04lX  head=%lu  tail=%lu  woff=%04lX  roff=%04lX", fs.latest_offset, fs.head_offset / fs.storage.sector_size,
        fs.tail_sector, fs.head_offset, fs.read_offset);
#endif

  puts("\nFS:");
  dump_array(s_fs_buf, sizeof(s_fs_buf));

  LogDBBlock *read = (LogDBBlock *)data;
  logdb_read_last(&fs, read);
  printf("\nLast %d:\n", read->data_len);
  dump_array(data, sizeof(data));

  for(int i = 0; i < 11; i++) {
    read->data_len = sizeof(data) - sizeof(*read);
    if(logdb_read_next(&fs, read)) {
      printf("\nRead %d:\n", read->data_len);
      dump_array(data, sizeof(data));
    }
  }


  return 0;
}

#endif
