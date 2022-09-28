#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "util/minmax.h"
#include "cstone/error_log.h"
#include "cstone/prop_id.h"
#include "cstone/umsg.h"
#include "cstone/debug.h"
#include "cstone/blocking_io.h"


static inline bool is_valid_entry(ErrorEntry *entry) {
  return entry->id != 0xFFFFFFFFul;
}

static inline size_t offset_to_sector(ErrorLog *el, size_t offset) {
  return offset / el->storage.sector_size;
}

static inline size_t sector_to_offset(ErrorLog *el, size_t sector, size_t entry) {
  return sector*el->storage.sector_size + entry*sizeof(ErrorEntry);
}


void errlog_init(ErrorLog *el, StorageConfig *cfg) {
  memset(el, 0, sizeof(*el));
  memcpy(&el->storage, cfg, sizeof(*cfg));

  el->entries_per_sector = el->storage.sector_size / sizeof(ErrorEntry);
}


size_t errlog_size(ErrorLog *el) {
  return el->storage.num_sectors * el->storage.sector_size;
}


// Reset read iterator
void errlog_read_init(ErrorLog *el) {
  el->read_offset = sector_to_offset(el, el->tail_sector, 0);
  el->read_iter_start = true;
}


static bool errlog__verify_empty(ErrorLog *el, size_t offset, size_t len) {
  uint32_t buf[8];
  size_t read_bytes;

  while(len > 0) {
    read_bytes = min(sizeof(buf), len);
    el->storage.read_block(el->storage.ctx, offset, (uint8_t *)buf, read_bytes);

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


void errlog_format(ErrorLog *el) {
//  puts("## LOG FORMAT");
  for(size_t i = 0; i < el->storage.num_sectors; i++) {
    // Confirm sector isn't already erased
    bool need_erase = !errlog__verify_empty(el, i*el->storage.sector_size, el->storage.sector_size);

    if(need_erase) {
//      printf("## Erase sector %lu\n", i);
      el->storage.erase_sector(el->storage.ctx, i*el->storage.sector_size, el->storage.sector_size);
    }
  }

  el->latest_offset = 0;
  el->head_offset = 0;
  el->tail_sector = 0;

  errlog_read_init(el);
}



// Returns: -1 if empty
static ptrdiff_t find_last_entry(ErrorLog *el, size_t sector_num) {
  ErrorEntry entry;
  size_t i;
  for(i = 0; i < el->entries_per_sector; i++) {
    size_t offset = sector_to_offset(el, sector_num, i); //sector_num*el->storage.sector_size + i*sizeof(entry);
    el->storage.read_block(el->storage.ctx, offset, (uint8_t *)&entry, sizeof(entry));
    if(!is_valid_entry(&entry))
      break;
  }

  return (ptrdiff_t)i - 1;
}


static ptrdiff_t find_tail_sector(ErrorLog *el) {
  size_t head_sector = offset_to_sector(el, el->head_offset);

  size_t next_sector = (head_sector+1) % el->storage.num_sectors;
  while(next_sector != head_sector) {
    ErrorEntry entry;
    el->storage.read_block(el->storage.ctx, sector_to_offset(el, next_sector, 0),
                          (uint8_t *)&entry, sizeof(entry));
    if(is_valid_entry(&entry)) // Found tail sector
      return next_sector;

    next_sector = (next_sector+1) % el->storage.num_sectors;
  }

  // If no other occupied sector is found the tail must be the same
  // as the head sector.
  return head_sector;
}


bool errlog_mount(ErrorLog *el) {
/*
  Scan sectors to find tail and head
  Search for log head:
    Find a partially filled sector that starts with a valid entry and ends with erased data
    This is the head.

  A sector is never filled to the last entry without erasing the next sector first. This
  guarantees that a full log always has a sector that can be identified as the head.
*/

  // Special case for single sector log
  if(el->storage.num_sectors == 1) {
    ptrdiff_t last_entry = find_last_entry(el, 0);
    if(last_entry < 0) {  // Empty sector
      el->latest_offset = 0;
      el->head_offset = 0;
      el->tail_sector = 0;

    } else {  // Partial sector
      el->latest_offset = sector_to_offset(el, 0, last_entry);
      el->head_offset = el->latest_offset + sizeof(ErrorEntry);
      el->tail_sector = 0;
    }
    return true;
  }


  ErrorEntry entry;
  size_t last_entry_offset = (el->entries_per_sector-1) * sizeof(entry);
  size_t first_empty_sector = el->storage.num_sectors; // Invalid value
  bool empty_log = true;

  for(size_t i = 0; i < el->storage.num_sectors; i++) {
    // Read first entry
    el->storage.read_block(el->storage.ctx, i*el->storage.sector_size, (uint8_t *)&entry, sizeof(entry));
    if(is_valid_entry(&entry)) {  // Found used sector
      empty_log = false;

      // Read last entry
      el->storage.read_block(el->storage.ctx, i*el->storage.sector_size + last_entry_offset,
                    (uint8_t *)&entry, sizeof(entry));

      if(!is_valid_entry(&entry)) {  // Found partial sector. This is the head
        DPUTS("Mount on partial");
        // Scan sector to find last used entry
        ptrdiff_t last_entry = find_last_entry(el, i);
        el->latest_offset = sector_to_offset(el, i, last_entry);
        el->head_offset = el->latest_offset + sizeof(entry);
        el->tail_sector = find_tail_sector(el);
        return true;
      }

    } else if(first_empty_sector == el->storage.num_sectors) { // Track first fully erased sector
      first_empty_sector = i;
    }
  }

  // It is possible that there are no partial sectors if the last write filled out a
  // complete sector. In that case we use the first empty sector as the location for the
  // head entry.
  // The other possibility is that this is a freshly formatted log and no entries exist at all.

  if(empty_log) { // Freshly formatted with no data
    DPUTS("Mount empty");
    el->latest_offset = 0;
    el->head_offset = 0;
    el->tail_sector = 0;

  } else {  // Last write ended on sector bound
    // Get previous sector
    DPUTS("Mount on bound");
    size_t last_full_sector = (first_empty_sector + el->storage.num_sectors - 1) % el->storage.num_sectors;
    el->latest_offset = last_full_sector * el->storage.sector_size;
    el->head_offset = first_empty_sector * el->storage.sector_size;
    el->tail_sector = find_tail_sector(el);
  }

  return true;
}


static bool errlog__prep_for_write(ErrorLog *el) {
  // We must erase the next sector if we will be writing the last entry in
  // the current sector

  size_t write_offset = el->head_offset;
  size_t write_sector = write_offset / el->storage.sector_size;

  if(write_sector >= el->storage.num_sectors) { // Wrap around
    write_offset = 0;
    write_sector = 0;
  }

  size_t write_index = (write_offset - write_sector * el->storage.sector_size) / sizeof(ErrorEntry);


  // If the sector size is not a multiple of ErrorEntry size then we will need to make
  // an adjustment to the write offset 
  if(write_index >= el->entries_per_sector) {
    write_sector = (write_sector+1) % el->storage.num_sectors;
    write_offset = write_sector * el->storage.sector_size;
    write_index  = 0;
  }

  el->head_offset = write_offset;

  if(write_index == el->entries_per_sector-1) { // About to be full
    // We must erase the next sector before filling this one
    size_t next_sector = (write_sector+1) % el->storage.num_sectors;

    el->storage.erase_sector(el->storage.ctx, next_sector * el->storage.sector_size,
                             el->storage.sector_size);

    if(next_sector == el->tail_sector)
      el->tail_sector = (el->tail_sector+1) % el->storage.num_sectors;

    if(el->storage.num_sectors == 1) {  // We just erased sector 0 so reset the offsets
      el->head_offset   = 0;
      el->latest_offset = 0;
    }
  }

  return true;
}


bool errlog_write(ErrorLog *el, ErrorEntry *entry) {
  if(!errlog__prep_for_write(el)) {
    report_error(P1_ERROR | P2_STORAGE | P3_TARGET | P4_UPDATE, __LINE__);
    return false;
  }

//  printf("## EW @ %" PRIuz "\n", el->head_offset);
  if(el->storage.write_block(el->storage.ctx, el->head_offset, (uint8_t *)entry, sizeof(*entry))) {
    el->latest_offset = el->head_offset;
    el->head_offset += sizeof(*entry);

    return true;
  }

  report_error(P1_ERROR | P2_STORAGE | P3_TARGET | P4_UPDATE, __LINE__);
  return false;
}


static inline void errlog__read_entry(ErrorLog *el, size_t entry_offset, ErrorEntry *entry) {
  el->storage.read_block(el->storage.ctx, entry_offset, (uint8_t *)entry, sizeof(*entry));
//  printf("## ENTRY @ %ld = " PROP_ID "\n", entry_offset, entry->id);
}



bool errlog_read_next(ErrorLog *el, ErrorEntry *entry) {

  if(el->read_offset != el->tail_sector * el->storage.sector_size || el->read_iter_start) {
    errlog__read_entry(el, el->read_offset, entry);
    if(!is_valid_entry(entry))
      return false;

    el->read_iter_start = false;

    el->read_offset += sizeof(*entry);
    if(el->read_offset >= el->storage.num_sectors * el->storage.sector_size) { // Wrap around
      el->read_offset = 0;

    } else {  // Check for sector size not a multiple of ErrorEntry
      size_t read_sector = el->read_offset / el->storage.sector_size;
      size_t read_index = (el->read_offset - read_sector * el->storage.sector_size) / sizeof(ErrorEntry);
      if(read_index >= el->entries_per_sector) {
        read_sector = (read_sector+1) % el->storage.num_sectors;
        el->read_offset = read_sector * el->storage.sector_size;
        read_index  = 0;
      }
    }

    return true;
  }

  return false;
}


bool errlog_at_end(ErrorLog *el) {
  return el->read_offset == el->latest_offset;
}



bool errlog_read_raw(ErrorLog *el, size_t block_start, uint8_t *dest, size_t block_size) {
  return el->storage.read_block(el->storage.ctx, block_start, dest, block_size);
}


void errlog_dump_raw(ErrorLog *el, size_t dump_bytes, size_t offset) {
  puts("\nError log:");
  storage_dump_raw(&el->storage, dump_bytes, offset);
}



void errlog_print_all(ErrorLog *el) {
  ErrorEntry entry;
  char buf[64];
  unsigned count = 0;

  errlog_read_init(el);
  while(errlog_read_next(el, &entry)) {
    count++;
  }
  bprintf("Error log (%d entries):\n", count);

  errlog_read_init(el);
  while(errlog_read_next(el, &entry)) {
    bprintf("  " PROP_ID "  %s = %" PRIu32 "\n", entry.id, prop_get_name(entry.id, buf, sizeof(buf)),
           entry.data);
  }
}

