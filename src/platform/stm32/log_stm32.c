#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define STM32_HAL_LEGACY  // Inhibit legacy header
#include "stm32f4xx_hal.h"

#include "cstone/log_db.h"
#include "cstone/log_info.h"
#include "cstone/log_stm32.h"


// ******************** Log DB STM32 flash callbacks ********************

#ifdef STM32F429xx
static uint32_t flash_sector_index(uint8_t *addr) {
  uint32_t flash_offset = (uint32_t)addr - 0x8000000ul;

  if(flash_offset < 0x100000ul) { // Bank 1
    if(flash_offset < 0x10000ul) {  // 16K (0-3)
      return flash_offset / (16 * 1024);
    } else if(flash_offset < 0x20000ul) { // 64K (4)
      return FLASH_SECTOR_4;
    } else {  // 128K (5-11)
      return FLASH_SECTOR_4 + flash_offset / (128 * 1024);
    }

  } else {  // Bank 2
    flash_offset -= 0x100000ul;
    if(flash_offset < 0x10000ul) {  // 16K (12-15)
      return FLASH_SECTOR_12 + flash_offset / (16 * 1024);
    } else if(flash_offset < 0x20000ul) { // 64K (16)
      return FLASH_SECTOR_16;
    } else {  // 128K (17-23)
      return FLASH_SECTOR_16 + flash_offset / (128 * 1024);
    }
  }
}
#endif

void log_stm32_erase_sector(void *ctx, size_t sector_start, size_t sector_size) {
  if(HAL_FLASH_Unlock() != HAL_OK)
    return;

  uint32_t sector = flash_sector_index((uint8_t *)ctx + sector_start);
//  printf("## ERASE sector 0x%08X  %lu...\n", (uintptr_t)sector_start, sector);

  FLASH_EraseInitTypeDef erase = {
    .TypeErase    = FLASH_TYPEERASE_SECTORS,
    .Banks        = sector <= FLASH_SECTOR_11 ? FLASH_BANK_1 : FLASH_BANK_2,
    .Sector       = sector,
    .NbSectors    = 1,
    .VoltageRange = FLASH_VOLTAGE_RANGE_3 // 2.7V - 3.6V (No Vpp)
  };

  uint32_t error;
  HAL_FLASHEx_Erase(&erase, &error);
//  if(HAL_FLASHEx_Erase(&erase, &error) == HAL_OK)
//    puts("\tdone");

  __HAL_FLASH_DATA_CACHE_RESET();

  HAL_FLASH_Lock();
}

bool log_stm32_read_block(void *ctx, size_t block_start, uint8_t *dest, size_t block_size) {
//  printf("## READ  %p\n", block_start);
  memcpy(dest, (uint8_t *)ctx + block_start, block_size);
  return true;
}


bool log_stm32_write_block(void *ctx, size_t block_start, uint8_t *src, size_t block_size) {
//  printf("## WRITE  %p  sz=%u\n", block_start, block_size);
  if(HAL_FLASH_Unlock() != HAL_OK)
    return false;

  __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR | FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR);

  uint8_t *dest = (uint8_t *)ctx + block_start;

  // Align to WORD
  while((uintptr_t)dest & 0x03ul && block_size > 0) {
    HAL_FLASH_Program(FLASH_TYPEPROGRAM_BYTE, (uint32_t)dest++, *src++);
    block_size--;
  }

  // Write WORDs (NOTE: DWORD writes require Ext Vpp)
  while(block_size >= 4) {
    HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, (uint32_t)dest, *(uint32_t *)src);
    dest += 4;
    src += 4;
    block_size -= 4;
  }

  // Finish remaining bytes
  while(block_size > 0) {
    HAL_FLASH_Program(FLASH_TYPEPROGRAM_BYTE, (uint32_t)dest++, *src++);
    block_size--;
  }

  __HAL_FLASH_DATA_CACHE_RESET();
  HAL_FLASH_Lock();

  return true;
}


