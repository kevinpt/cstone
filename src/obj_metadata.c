#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <inttypes.h>

#include "cstone/debug.h"
#include "cstone/profile.h"
#include "cstone/obj_metadata.h"
#include "cstone/term_color.h"
#include "cstone/crc32_stm32.h"

#include "util/crc16.h"
//#include "util/crc32.h"

#if 0
// FIXME: Move CRC code to separate source
#include "stm32f4xx_hal.h"
#include "stm32f4xx_ll_crc.h"

static void crc32_init_hw(void) {
  __HAL_RCC_CRC_CLK_ENABLE();
  LL_CRC_ResetCRCCalculationUnit(CRC);
}


// data must be 4 byte aligned
static uint32_t crc32_update_hw(const uint8_t *data, size_t data_len) {
  if(data_len & 0x03)
    DPRINT("Warning CRC32 data_len = %d", data_len);

  for(size_t i = 0; i < data_len/4; i++) {
//    uint32_t dswap = __builtin_bswap32(*(uint32_t*)&data[i*4]);
    uint32_t dswap = *(uint32_t*)&data[i << 2];
    LL_CRC_FeedData32(CRC, dswap);
  }
  return LL_CRC_ReadData32(CRC);
}
#endif



static bool firmware_crc(const ObjMemRegion *regions, uint32_t *crc) {
  *crc = 0;

  uint32_t elf_crc;

#if 0
  elf_crc = crc32_init();
id = profile_add(0, "CRC32 Serial");
profile_start(id);

  for(int i = 0; i < OBJ_MAX_REGIONS; i++) {
    const ObjMemRegion *cur = &regions[i];
    if(cur->end - cur->start == 0) // No more regions
      break;

//    printf("## cur: %08X - %08X\n", cur->start, cur->end);
    uint8_t *block = cur->start;
    elf_crc = crc32_update_serial_block(elf_crc, block, cur->end - cur->start);
//    printf("## elf_crc: 0x%08X\n", elf_crc);
  }
profile_stop(id);
  *crc = crc32_finish(elf_crc);
  printf("## elf_crc (sw): 0x%08"PRIX32"\n", elf_crc);

#else
  crc32_init_hw();

  for(int i = 0; i < OBJ_MAX_REGIONS; i++) {
    const ObjMemRegion *cur = &regions[i];
    if(cur->end - cur->start == 0) // No more regions
      break;

//    printf("## cur: %08X - %08X\n", cur->start, cur->end);
    uint8_t *block = cur->start;
    elf_crc = crc32_update_hw(block, cur->end - cur->start);
//    printf("## elf_crc (hw): 0x%08X\n", elf_crc);
  }

  *crc = elf_crc;
#endif
  return true;
}


extern const ObjectMetadata g_metadata;
#define CHAR_PASS   A_BGRN u8"✓" A_NONE
#define CHAR_FAIL   A_BRED u8"✗" A_NONE

void validate_metadata(void) {
  // Check firmware crc
  uint32_t obj_crc;
  firmware_crc(g_metadata.mem_regions, &obj_crc);

  // Check metadata CRC
#define FIELD_SIZE(t, f)  sizeof(((t *)0)->f)
  uint16_t meta_crc = crc16_init();
  // Skip over initial CRCs in metadata struct
  size_t meta_offset = offsetof(ObjectMetadata, meta_crc) + FIELD_SIZE(ObjectMetadata, meta_crc);
  meta_crc = crc16_update_block(meta_crc, (uint8_t *)&g_metadata + meta_offset,
                                sizeof g_metadata - meta_offset);
  meta_crc = crc16_finish(meta_crc);

  printf("     App CRC: 0x%08"PRIX32" %s\n", obj_crc,
        obj_crc == g_metadata.obj_crc ? CHAR_PASS : CHAR_FAIL);
  printf("    Meta CRC: 0x%04X %s\n", meta_crc,
        meta_crc == g_metadata.meta_crc ? CHAR_PASS : CHAR_FAIL);
}

