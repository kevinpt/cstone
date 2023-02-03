#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

#include "build_config.h"
#include "cstone/platform.h"
#include "cstone/prop_id.h"
#include "cstone/prop_db.h"
#include "cstone/log_db.h"
#include "cstone/log_compress.h"
#include "cstone/debug.h"
#include "cstone/timing.h"
#include "cstone/rtc_device.h"

#include "cstone/log_props.h"
#include "util/random.h"

#define USE_PROP_COMPRESSION


/*static inline uint32_t rot_left(uint32_t n, unsigned bits) {*/
/*  const unsigned mask = 8*sizeof(n) - 1;*/
/*  bits &= mask;*/

/*  return (n << bits) | (n >> ((-bits) & mask));*/
/*}*/


static inline uint32_t rot_right(uint32_t n, unsigned bits) {
  const unsigned mask = 8*sizeof(n) - 1;
  bits &= mask;

  return (n >> bits) | (n << ((-bits) & mask));
}


void update_prng_seed(PropDB *db) {
  uint32_t seed = millis() * micros();

  if(rtc_valid_time(rtc_sys_device()))
    seed ^= (uint32_t)rtc_get_time(rtc_sys_device());

  PropDBEntry entry;
  if(prop_get(db, P_SYS_PRNG_LOCAL_VALUE, &entry))
    seed ^= rot_right(entry.value, 8);

  prop_set_uint(db, P_SYS_PRNG_LOCAL_VALUE, seed, 0);
  prop_set_attributes(db, P_SYS_PRNG_LOCAL_VALUE, P_PROTECT | P_PERSIST);
  DPRINT("New seed: %08"PRIX32"\n", seed);
}


bool save_props_to_log(PropDB *db, LogDB *log_db, bool compress) {
  LogDBBlock *block;

  if(prop_db_serialize(db, &block)) {
#ifdef USE_PROP_COMPRESSION
    // Attempt to compress block
    LogDBBlock *compressed_block;

    if(compress && logdb_compress_block(block, &compressed_block)) {
      DPRINT("Writing compressed block  %u --> %u", block->data_len, compressed_block->data_len);

      logdb_write_block(log_db, compressed_block);  // Save compressed
      cs_free(compressed_block);

    } else
#endif
    {  // Compression less than 1.0x or disabled
      logdb_write_block(log_db, block); // Save uncompressed
    }

    cs_free(block);
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

  block = (LogDBBlock *)cs_malloc(sizeof(*block) + header.data_len);
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
          cs_free(decompressed);
        }
      }
      break;

    default:
      break;
    }
  }

  cs_free(block);

  return count;
}

