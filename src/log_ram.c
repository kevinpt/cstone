#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "cstone/log_db.h"
#include "cstone/log_info.h"
#include "cstone/log_ram.h"


// ******************** Log DB RAM callbacks ********************


void log_ram_erase_sector(void *ctx, size_t sector_start, size_t sector_size) {
  memset((uint8_t *)ctx + sector_start, 0xFF, sector_size);
}

bool log_ram_read_block(void *ctx, size_t block_start, uint8_t *dest, size_t block_size) {
//  printf("## RAM R %u @ %u --> %p\n", block_size, block_start, ctx + block_start);
  memcpy(dest, (uint8_t *)ctx + block_start, block_size);
  return true;
}

bool log_ram_write_block(void *ctx, size_t block_start, uint8_t *src, size_t block_size) {
//  printf("## RAM W %u @ %u --> %p\n", block_size, block_start, ctx + block_start);
  memcpy((uint8_t *)ctx + block_start, src, block_size);
  return true;
}

