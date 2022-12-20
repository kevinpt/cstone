#ifndef OBJ_METADATA_H
#define OBJ_METADATA_H

#include <limits.h>

// https://interrupt.memfault.com/blog/device-firmware-update-cookbook
// https://mcuoneclipse.com/2015/04/26/crc-checksum-generation-with-srecord-tools-for-gnu-and-eclipse/
// https://stackoverflow.com/questions/24150030/storing-crc-into-an-axf-elf-file


#define OBJ_METADATA_V1  1


#define OBJ_KIND_APP     1



// This needs to be usable from 64-bit utility code so all pointers are stored
// as 32-bit integers.
#if defined(__WORDSIZE) && __WORDSIZE == 64
typedef uint32_t Addr32;
#else
typedef void * Addr32;
#endif

typedef struct {
  Addr32 start;
  Addr32 end; // One-past-end for the region
} ObjMemRegion;


typedef struct {
  uint16_t kind;
  uint16_t reserved;
  uint32_t value;
} TraitDescriptor;


#define OBJ_MAX_REGIONS  4

typedef struct {
  uint32_t  obj_crc;      // 32-bit CRC over all mem_regions
  uint16_t  meta_crc;     // 16-bit CRC over following fields

  uint16_t  meta_version : 4; // Format version of this struct
  uint16_t  obj_kind     : 4; // Type of this object
  uint16_t  active_image : 1;
  uint16_t  debug_build  : 1;
  uint16_t  reserved1    : 6;


  ObjMemRegion mem_regions[OBJ_MAX_REGIONS];  // Flash memory regions covered by app_crc
  uint32_t  obj_version;  // Monotonic integer version
  uint32_t  git_sha;      // Output of `git rev-parse --short=8 HEAD`
  char      obj_name[64];
  uint16_t  reserved2;
  
  // Additional metadata stored in optional trait array
  // This can be used for things like associating GPIO pins with functions so that
  // a generic bootloader can discover them rather than hardcoding them into the bootloader.
  uint16_t  trait_count;
#if defined(__WORDSIZE) && __WORDSIZE == 64
  const Addr32 traits;
#else
  const TraitDescriptor *traits;
#endif
} ObjectMetadata;



#ifdef __cplusplus
extern "C" {
#endif

void validate_metadata(void);

#ifdef __cplusplus
}
#endif

#endif // OBJ_METADATA_H
