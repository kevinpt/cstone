#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

#include "cstone/prop_id.h"
#include "cstone/prop_db.h"
#include "cstone/log_db.h"
#include "cstone/log_compress.h"
#include "cstone/debug.h"

#include "cstone/log_props.h"


#define USE_PROP_COMPRESSION



bool save_props_to_log(PropDB *db, LogDB *log_db, bool compress) {
  LogDBBlock *block;

  if(prop_db_serialize(db, &block)) {
#ifdef USE_PROP_COMPRESSION
    // Attempt to compress block
    LogDBBlock *compressed_block;

    if(compress && logdb_compress_block(block, &compressed_block)) {
      DPRINT("Writing compressed block  %u --> %u", block->data_len, compressed_block->data_len);

      logdb_write_block(log_db, compressed_block);  // Save compressed
      free(compressed_block);

    } else
#endif
    {  // Compression less than 1.0x or disabled
      logdb_write_block(log_db, block); // Save uncompressed
    }

    free(block);
    return true;
  }

  return false;
}


unsigned restore_props_from_log(PropDB *db, LogDB *log_db) {
  LogDBBlock *block;
  unsigned count = 0;

  // Get block size
  LogDBBlock header;
  header.data_len = 0;
  logdb_read_last(log_db, &header);
  if(header.data_len == 0) // No valid blocks in log FS
    return 0;

  block = (LogDBBlock *)malloc(sizeof(*block) + header.data_len);
  if(!block)
    return 0;

  block->data_len = header.data_len;

  // Get block data
  if(logdb_read_last(log_db, block)) {
    // Decode into prop DB

    switch(block->kind) {
    case BLOCK_KIND_PROP_DB:
      if(!block->compressed) {
        DPUTS("Decode normal");
        count = prop_db_deserialize(db, block->data, block->data_len);

      } else {
        uint8_t *decompressed;
        size_t data_len = logdb_decompress_block(block, &decompressed);
        DPRINT("Decode compressed %u --> %" PRIuz, block->data_len, data_len);
        if(data_len > 0) {
          count = prop_db_deserialize(db, decompressed, data_len);
          free(decompressed);
        }
      }
      break;

    default:
      break;
    }
  }

  free(block);

  return count;
}
