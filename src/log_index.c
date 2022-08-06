#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#include "cstone/platform.h"

#include "util/dhash.h"
#include "cstone/log_db.h"
#include "cstone/log_index.h"
#include "cstone/debug.h"


typedef struct {
  uint32_t data_len;
  uint32_t block_start;
} LogDBIndexItem;


bool logdb_index_update(LogDBIndex *index, LogDBBlock *block, size_t block_start) {
  LogDBIndexItem item = {
    .data_len     = block->data_len,
    .block_start  = block_start
  };

//  printf("## Index build key=%u @%lu  len=%u  %016lX\n", block->kind, block_start, block->data_len, *(uint64_t*)&item);


  dhKey key = {
    .data = (void *)(uintptr_t)block->kind,
    .length = 1
  };
  return dh_insert(&index->hash, key, &item);
}


static void block_ix_item_destroy(dhKey key, void *value, void *ctx) {
}


bool logdb_index_create(LogDB *db, LogDBIndex *index) {
  dhConfig hash_cfg = {
    .init_buckets = 8,
    .value_size   = sizeof(LogDBIndexItem),
    .max_storage  = 0,
    .ext_storage  = NULL,
    .destroy_item = block_ix_item_destroy,
    .gen_hash     = dh_gen_hash_int,
    .is_equal     = dh_equal_hash_keys_int
  };

  if(!dh_init(&index->hash, &hash_cfg, index))
    return false;


  logdb_read_init(db);

  LogDBBlock header;
  size_t block_start;
  while(logdb_read_next_header(db, &header, &block_start)) {
    DPRINT("LOG HEADER %d  %u  @ %" PRIuz, header.kind, header.data_len, block_start);
    logdb_index_update(index, &header, block_start);
  }

  return true;
}


void logdb_index_free(LogDBIndex *index) {
  dh_free(&index->hash);
}

static inline bool logdb__index_lookup(LogDBIndex *index, uint8_t kind, LogDBIndexItem *item) {
  dhKey key = {
    .data = (void *)(uintptr_t)kind,
    .length = 1
  };

  if(!dh_lookup(&index->hash, key, item))
    return false;

  return true;
}


bool logdb_index_read(LogDB *db, LogDBIndex *index, uint8_t kind, LogDBBlock *block) {
  LogDBIndexItem item = {0};

  if(!logdb__index_lookup(index, kind, &item)) {
    block->data_len = 0;
    return false;
  }

  size_t buf_data_len = block->data_len;

//  printf("## Index read @ %u  len=%u\n", item.block_start, item.data_len);
  // Get header
  logdb_read_raw(db, item.block_start, (uint8_t *)block, sizeof(*block));

  if(!logdb_validate_header(block))
    return false;

  if(block->data_len <= buf_data_len) { // Get full block
    logdb_read_raw(db, item.block_start, (uint8_t *)block, sizeof(*block) + block->data_len);
  }

  return true;
}

