#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "build_config.h" // Get build-specific platform settings

#define STM32_HAL_LEGACY  // Inhibit legacy header
#if defined PLATFORM_STM32F1
#  include "stm32f1xx_hal.h"
#else
#  include "stm32f4xx_hal.h"
#endif

#include "cstone/log_db.h"
#include "cstone/log_info.h"
#include "cstone/log_stm32.h"


// ******************** Log DB STM32 flash callbacks ********************


void log_stm32_erase_sector(void *ctx, size_t sector_start, size_t sector_size) {
  if(HAL_FLASH_Unlock() != HAL_OK)
    return;


#if defined PLATFORM_STM32F1
  FLASH_EraseInitTypeDef erase = {
    .TypeErase    = FLASH_TYPEERASE_PAGES,
    .Banks        = FLASH_BANK_1, // NOTE: XL-density devices have a bank 2
    .PageAddress  = sector_start,
    .NbPages      = 1,
  };

#elif defined BOARD_STM32F429I_DISC1
  uint32_t sector = flash_sector_index((uint8_t *)ctx + sector_start);
//  printf("## ERASE sector 0x%08X  %lu...\n", (uintptr_t)sector_start, sector);

  FLASH_EraseInitTypeDef erase = {
    .TypeErase    = FLASH_TYPEERASE_SECTORS,
    .Banks        = sector <= FLASH_SECTOR_11 ? FLASH_BANK_1 : FLASH_BANK_2,
    .Sector       = sector,
    .NbSectors    = 1,
    .VoltageRange = FLASH_VOLTAGE_RANGE_3 // 2.7V - 3.6V (No Vpp)
  };
#elif defined BOARD_STM32F401_BLACK_PILL
  uint32_t sector = flash_sector_index((uint8_t *)ctx + sector_start);
//  printf("## ERASE sector 0x%08X  %lu...\n", (uintptr_t)sector_start, sector);
  // STM32F401 Sectors 0-3 = 16K, 4 = 64K, 5-7 = 128K
  FLASH_EraseInitTypeDef erase = {
    .TypeErase    = FLASH_TYPEERASE_SECTORS,
    .Banks        = FLASH_BANK_1,
    .Sector       = sector,
    .NbSectors    = 1,
    .VoltageRange = FLASH_VOLTAGE_RANGE_3 // 2.7V - 3.6V (No Vpp)
  };
#else
#  error "Unknown device for flash layout"
#endif

  uint32_t error;
  HAL_FLASHEx_Erase(&erase, &error);
//  if(HAL_FLASHEx_Erase(&erase, &error) == HAL_OK)
//    puts("\tdone");

#if !defined PLATFORM_STM32F1
  __HAL_FLASH_DATA_CACHE_RESET();
#endif
  HAL_FLASH_Lock();
}

bool log_stm32_read_block(void *ctx, size_t block_start, uint8_t *dest, size_t block_size) {
//  printf("## READ  %p\n", block_start);
  memcpy(dest, (uint8_t *)ctx + block_start, block_size);
  return true;
}


bool log_stm32_write_block(void *ctx, size_t block_start, uint8_t *src, size_t block_size) {
//  printf("## WRITE  %p  sz=%u\n", block_start, block_size);
#if defined PLATFORM_STM32F1
  // STM32F1 requires half-word alignment for flash writes
  if((block_start & 0x01) || (block_size & 0x01))
    return false;
#endif

  if(HAL_FLASH_Unlock() != HAL_OK)
    return false;

#if defined PLATFORM_STM32F1
  __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_WRPERR | FLASH_FLAG_PGERR | \
                         FLASH_FLAG_OPTVERR);
#else
  __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR | \
                          FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR);
#endif

  uint8_t *dest = (uint8_t *)ctx + block_start;

#if defined PLATFORM_STM32F1
  // Align to WORD
  while((uintptr_t)dest & 0x03ul && block_size > 1) {
    HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, (uint32_t)dest, *src);
    dest += 2;
    src += 2;
    block_size -= 2;
  }
#else
  // Align to WORD
  while((uintptr_t)dest & 0x03ul && block_size > 0) {
    HAL_FLASH_Program(FLASH_TYPEPROGRAM_BYTE, (uint32_t)dest++, *src++);
    block_size--;
  }
#endif

  // Write WORDs (NOTE: DWORD writes require Ext Vpp)
  while(block_size >= 4) {
    HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, (uint32_t)dest, *(uint32_t *)src);
    dest += 4;
    src += 4;
    block_size -= 4;
  }

  // Finish remaining bytes
#if defined PLATFORM_STM32F1
  while(block_size > 1) {
    HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, (uint32_t)dest, *src);
    dest += 2;
    src += 2;
    block_size -= 2;
  }
#else

  while(block_size > 0) {
    HAL_FLASH_Program(FLASH_TYPEPROGRAM_BYTE, (uint32_t)dest++, *src++);
    block_size--;
  }
#endif

#if !defined PLATFORM_STM32F1
  __HAL_FLASH_DATA_CACHE_RESET();
#endif
  HAL_FLASH_Lock();

  return true;
}


