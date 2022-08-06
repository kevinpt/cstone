#ifndef LOG_COMPRESS_H
#define LOG_COMPRESS_H

#ifdef __cplusplus
extern "C" {
#endif


bool logdb_compress_block(LogDBBlock *block, LogDBBlock **compressed_block);

size_t logdb_uncompressed_size(LogDBBlock *compressed_block);
size_t logdb_decompress_block(LogDBBlock *compressed_block, uint8_t **decompressed);

#ifdef __cplusplus
}
#endif

#endif // LOG_COMPRESS_H
