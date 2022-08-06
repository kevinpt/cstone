#ifndef LOG_RAM_H
#define LOG_RAM_H


#ifdef __cplusplus
extern "C" {
#endif

void log_ram_erase_sector(void *ctx, size_t sector_start, size_t sector_size);
bool log_ram_read_block(void *ctx, size_t block_start, uint8_t *dest, size_t block_size);
bool log_ram_write_block(void *ctx, size_t block_start, uint8_t *src, size_t block_size);

#ifdef __cplusplus
}
#endif

#endif // LOG_RAM_H
