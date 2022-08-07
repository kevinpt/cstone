#ifndef STORAGE_H
#define STORAGE_H

typedef void (*EraseSector)(void *storage_ctx, size_t sector_start, size_t sector_size);
typedef bool (*ReadBlock)(void *storage_ctx, size_t block_start, uint8_t *dest, size_t block_size);
typedef bool (*WriteBlock)(void *storage_ctx, size_t block_start, uint8_t *src, size_t block_size);

typedef struct {
  // Storage geometry
  size_t sector_size;
  size_t num_sectors;
  void  *ctx; // Backend specific context for the callbacks

  // Low level callbacks
  EraseSector erase_sector;
  ReadBlock   read_block;
  WriteBlock  write_block;
} StorageConfig;


#ifdef __cplusplus
extern "C" {
#endif

void storage_dump_raw(StorageConfig *store, size_t dump_bytes, size_t offset);

#ifdef __cplusplus
}
#endif

#endif // STORAGE_H
