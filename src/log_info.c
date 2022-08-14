#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#include "cstone/platform.h"
#include "cstone/debug.h"

#include "cstone/log_db.h"
#include "cstone/log_compress.h"
#include "cstone/log_info.h"

#include "cstone/prop_id.h"
#include "cstone/prop_db.h"
#include "cstone/prop_serialize.h"


extern mpPoolSet g_pool_set;


void logdb_dump_raw(LogDB *db, size_t dump_bytes, size_t offset) {
  puts("\nLog DB:");
  storage_dump_raw(&db->storage, dump_bytes, offset);
}


// Extract newest record into temporary prop DB and dump it
void logdb_dump_record(LogDB *db) {
  LogDBBlock *block;

  // Get block size
  LogDBBlock header;
  header.data_len = 0;
  logdb_read_last(db, &header);
  if(header.data_len == 0) // No valid blocks in log FS
    return;

  //printf("## Get block: %u\n", sizeof(*block) + header.data_len);
  block = cs_malloc(sizeof(*block) + header.data_len);
  if(!block)
    return;

  block->data_len = header.data_len;
  if(logdb_read_last(db, block) && block->kind == BLOCK_KIND_PROP_DB) {

    // Create temporary prop DB to hold decoded block data
    PropDB *temp_db = cs_malloc(sizeof(PropDB));
    if(temp_db) {
      // Decode into prop DB
      prop_db_init(temp_db, 32, 0, &g_pool_set);

      if(!block->compressed) {
        prop_db_deserialize(temp_db, block->data, block->data_len);

      } else {
        uint8_t *decompressed;
        size_t data_len = logdb_decompress_block(block, &decompressed);
        DPRINT("Decode compressed %u --> %" PRIuz, block->data_len, data_len);
        if(data_len > 0) {
          prop_db_deserialize(temp_db, decompressed, data_len);
          cs_free(decompressed);
        }
      }

      prop_db_dump(temp_db);
      prop_db_free(temp_db);
      cs_free(temp_db);
    }
  }
  cs_free(block);
}

