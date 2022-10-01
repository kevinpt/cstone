#ifndef LOG_STM32_H
#define LOG_STM32_H


#ifdef __cplusplus
extern "C" {
#endif

void log_stm32_erase_sector(void *ctx, size_t sector_start, size_t sector_size);
bool log_stm32_read_block(void *ctx, size_t block_start, uint8_t *dest, size_t block_size);
bool log_stm32_write_block(void *ctx, size_t block_start, uint8_t *src, size_t block_size);

// STM32F4 (and other families) need a callback to map addresses to flash sectors
uint32_t flash_sector_index(uint8_t *addr);

#ifdef __cplusplus
}
#endif

#endif // LOG_STM32_H
