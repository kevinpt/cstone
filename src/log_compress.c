#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdlib.h>

#include "heatshrink_encoder.h"
#include "heatshrink_decoder.h"

#include "cstone/platform.h"
#include "util/unaligned_access.h"
#include "cstone/log_db.h"
#include "cstone/log_compress.h"



#define COMPRESS_WINDOW_SIZE    8
#define COMPRESS_LOOKAHEAD_SIZE 4

bool logdb_compress_block(LogDBBlock *block, LogDBBlock **compressed_block) {
  /*  Compressed block:
      [header] [uncompressed len][compressed data]
  */

  if(block->compressed) {
    *compressed_block = NULL;
    return false;
  }

  // Compress the raw data
  LogDBBlock *new_block = cs_malloc(sizeof(LogDBBlock) + sizeof(uint16_t) + block->data_len);

  if(!new_block) {
    *compressed_block = NULL;
    return false;
  }

  // Serialize uncompressed len
  set_unaligned_le((uint16_t)block->data_len, (uint16_t *)new_block->data);

  uint8_t *in_pos = block->data;
  uint8_t *in_end = in_pos + block->data_len;
  size_t in_size;

  uint8_t *out_pos = &new_block->data[2];
  uint8_t *out_end = out_pos + block->data_len;
  size_t out_size;

  heatshrink_encoder *hse = heatshrink_encoder_alloc(COMPRESS_WINDOW_SIZE,COMPRESS_LOOKAHEAD_SIZE);

  bool rval = true;
  while(in_pos < in_end) {
    heatshrink_encoder_sink(hse, in_pos, in_end - in_pos, &in_size);
    in_pos += in_size;

    HSE_poll_res estat;
    do {
      estat = heatshrink_encoder_poll(hse, out_pos, out_end - out_pos, &out_size);
      out_pos += out_size;
      if(out_pos >= out_end) { // Compressed data is larger than original, abort
        rval = false;
        goto cleanup;
      }
    } while(estat == HSER_POLL_MORE);
  }

  while(heatshrink_encoder_finish(hse) == HSER_FINISH_MORE) {
    heatshrink_encoder_poll(hse, out_pos, out_end - out_pos, &out_size);
    out_pos += out_size;

    if(out_pos >= out_end) { // Compressed data is larger than original, abort
      rval = false;
      goto cleanup;
    }
  }


  new_block->kind       = block->kind;
  new_block->compressed = 1;
  new_block->data_len   = out_pos - new_block->data;  // Includes 2-byte uncompressed len

cleanup:
  heatshrink_encoder_free(hse);

  if(!rval)
    cs_free(new_block);

  *compressed_block = rval ? new_block : NULL;
  return rval;
}


size_t logdb_uncompressed_size(LogDBBlock *compressed_block) {
  if(compressed_block->compressed)
    return get_unaligned_le((uint16_t *)compressed_block->data);

  return 0;
}


size_t logdb_decompress_block(LogDBBlock *compressed_block, uint8_t **decompressed) {
  uint8_t *compressed = &compressed_block->data[sizeof(uint16_t)];
  size_t compressed_len = compressed_block->data_len - sizeof(uint16_t);

  if(!compressed_block->compressed) {
    *decompressed = NULL;
    return 0;
  }

  uint16_t decompressed_len = get_unaligned_le((uint16_t *)compressed_block->data);

  *decompressed = cs_malloc(decompressed_len);
  if(!*decompressed)
    return 0;

  heatshrink_decoder *hsd = heatshrink_decoder_alloc(64, COMPRESS_WINDOW_SIZE,COMPRESS_LOOKAHEAD_SIZE);
  if(!hsd) {
    cs_free(*decompressed);
    *decompressed = NULL;
    return 0;
  }

  uint8_t *in_pos = compressed;
  uint8_t *in_end = in_pos + compressed_len;
  size_t in_size;

  uint8_t *out_pos = *decompressed;
  uint8_t *out_end = out_pos + decompressed_len;
  size_t out_size;


  while(in_pos < in_end) {
    heatshrink_decoder_sink(hsd, in_pos, in_end - in_pos, &in_size);
    in_pos += in_size;

    HSD_poll_res dstat;
    do {
      dstat = heatshrink_decoder_poll(hsd, out_pos, out_end - out_pos, &out_size);
      out_pos += out_size;
      if(out_pos >= out_end) { // Compressed data is larger than original, abort
        break;
      }
    } while(dstat == HSDR_POLL_MORE);
  }

  heatshrink_decoder_finish(hsd);

//  dump_array((uint8_t *)compressed_block, sizeof(LogDBBlock) + compressed_block->data_len);
//  printf("## Decompressed: %zu\n", out_pos - *decompressed);
//  dump_array(*decompressed, out_pos - *decompressed);

  heatshrink_decoder_free(hsd);

  // Validate decompressed data length
  if((size_t)decompressed_len != (size_t)(out_pos - *decompressed)) {
    cs_free(*decompressed);
    *decompressed = NULL;
    return 0;
  }

  return decompressed_len;
}

