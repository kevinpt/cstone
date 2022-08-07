#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "FreeRTOS.h"
#include "semphr.h"

#include "cstone/console.h"
#include "cstone/platform_io.h"

#include "cstone/storage.h"
#include "util/hex_dump.h"


void storage_dump_raw(StorageConfig *store, size_t dump_bytes, size_t offset) {
  uint8_t block[4*16];  // Four lines of data

  size_t read_pos = offset & ~(size_t)0x0Ful; // Align to 16-byte boundary
  size_t end_pos = offset + dump_bytes;
  size_t log_size = store->sector_size * store->num_sectors;
  if(end_pos > log_size) end_pos = log_size;

  DumpArrayState das;

  while(read_pos < end_pos) {
    size_t block_size = sizeof(block);
    if(block_size > end_pos - read_pos)
      block_size = end_pos - read_pos;

    if(store->read_block(store->ctx, read_pos, block, block_size)) {
      dump_array_init(&das, block, block_size, /*show_ascii*/true, /*ansi_color*/true);
      das.line_addr = read_pos;

    BLOCK_IO_START();
      dump_array_state(&das);
    BLOCK_IO_END();
    }

    read_pos += block_size;
  }
}
