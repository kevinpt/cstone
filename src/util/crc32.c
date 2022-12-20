/* SPDX-License-Identifier: MIT
Copyright 2021 Kevin Thibedeau
(kevin 'period' thibedeau 'at' gmail 'punto' com)

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice (including the next
paragraph) shall be included in all copies or substantial portions of the
Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.
*/

#include <stdint.h>
#include <stdlib.h>
#include "crc32.h"

#define CRC32_POLY  0x04C11DB7   // Ethernet CRC-32
// Hamming distance 4 up to 91607 data bits (11450 bytes)
// https://users.ece.cmu.edu/~koopman/crc/c32/0x82608edb_len.txt
// https://users.ece.cmu.edu/~koopman/crc/c32/0x82608edb.txt

/*
CRC-32 params:
Init:         0xFFFFFFFF
Reflect in:   no
Reflect out:  no
XOR out:      0x00000000
*/

/*
Initialize a CRC-32

Returns:
  Initial CRC state
*/
uint32_t crc32_init(void) {
  return 0xFFFFFFFF;
}

/*
Add data to CRC

Uses bit-serial algorithm

Args:
  crc:  Current CRC state
  data: New data for CRC

Returns:
  New CRC state
*/
uint32_t crc32_update_serial(uint32_t crc, uint8_t data) {
  crc ^= ((uint32_t)data) << (32-8);
  for(uint8_t i = 0; i < 8; i++) {
    if(crc & (1ul << (32-1)))
      crc = (crc << 1) ^ CRC32_POLY;
    else
      crc <<= 1;
  }
  return crc;
}


/*
Add data block to CRC

Uses bit-serial algorithm

Args:
  crc:      Current CRC state
  data:     Array of data to compute CRC over
  data_len: Size of data array

Returns:
  New CRC state
*/
uint32_t crc32_update_serial_block(uint32_t crc, const uint8_t *data, size_t data_len) {
  for(size_t i = 0; i < data_len; i++) {
    crc ^= ((uint16_t)data[i]) << (32-8);
    for(uint8_t j = 0; j < 8; j++) {
      if(crc & (1ul << (32-1)))
        crc = (crc << 1) ^ CRC32_POLY;
      else
        crc <<= 1;
    }
  }
  return crc;
}


//  python3 -m pycrc --width=32 --poly=0x4C11DB7 --xor-in=0xFFFFFFFF --reflect-in=0 --reflect-out=0
//          --xor-out=0 --algorithm table-driven --table-idx-width=4 --generate c -o crc.c

static const uint32_t s_crc32_table_small[16] = {
  0x00000000, 0x04c11db7, 0x09823b6e, 0x0d4326d9, 0x130476dc, 0x17c56b6b, 0x1a864db2, 0x1e475005,
  0x2608edb8, 0x22c9f00f, 0x2f8ad6d6, 0x2b4bcb61, 0x350c9b64, 0x31cd86d3, 0x3c8ea00a, 0x384fbdbd
};


/*
Add data block to CRC

Uses small table algorithm

Args:
  crc:      Current CRC state
  data:     Array of data to compute CRC over
  data_len: Size of data array

Returns:
  New CRC state
*/
uint32_t crc32_update_small_block(uint32_t crc, const uint8_t *data, size_t data_len) {
  const uint8_t *d8 = data;
  unsigned int ix;

  while (data_len--) {
    ix = (crc >> 28) ^ (*d8 >> 4);
    crc = s_crc32_table_small[ix & 0x0f] ^ (crc << 4);
    ix = (crc >> 28) ^ (*d8++ >> 0);
    crc = s_crc32_table_small[ix & 0x0f] ^ (crc << 4);
  }

  return crc;
}


/*
Add data to CRC

Uses small table algorithm

Args:
  crc:  Current CRC state
  data: New data for CRC

Returns:
  New CRC state
*/
uint32_t crc32_update(uint32_t crc, uint8_t data) {
  unsigned int ix;
  ix = (crc >> 28) ^ (data >> 4);
  crc = s_crc32_table_small[ix & 0x0f] ^ (crc << 4);
  ix = (crc >> 28) ^ (data >> 0);
  crc = s_crc32_table_small[ix & 0x0f] ^ (crc << 4);

  return crc;
}


/*
Add data block to CRC using STM32 byte order

Uses small table algorithm

The older STM32 families have an ill-conceived CRC32 peripheral that isn't configurable
and operates on little-endian words starting from the MSB. We have to swap bytes in a word
to match the hardware behavior. This will only work correctly if data_len is a multiple of 4.

Args:
  crc:      Current CRC state
  data:     Array of data to compute CRC over
  data_len: Size of data array

Returns:
  New CRC state
*/
uint32_t crc32_update_small_stm32(uint32_t crc, const uint8_t *data, size_t data_len) {
  const uint8_t *d8 = data;
  unsigned int ix;

  while(data_len) {
    for(int i = 3; i >= 0; i--) { // Swap bytes in 4-byte word
      uint8_t b = d8[i];
      ix = (crc >> 28) ^ (b >> 4);
      crc = s_crc32_table_small[ix & 0x0f] ^ (crc << 4);
      ix = (crc >> 28) ^ (b >> 0);
      crc = s_crc32_table_small[ix & 0x0f] ^ (crc << 4);
    }
    d8 += 4;
    data_len -= 4;
  }

  return crc;
}



/*
Complete CRC operation

Args:
  crc:  Current CRC state

Returns:
  Final CRC state
*/
uint32_t crc32_finish(uint32_t crc) {
  return crc;
}


#if 0
uint32_t stm32_crc32(uint32_t crc, uint32_t data) {

    int i;
    crc = crc ^ data;
    for (i = 0; i < 32; i++) {
        if (crc & 0x80000000)
            crc = ((crc << 1) ^ 0x04C11DB7) ;  
        else
            crc = (crc << 1) & 0xFFFFFFFF;
    }
    return crc;
}
#endif

#if 0

void crc_init(void) {
    LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_CRC);

    LL_CRC_SetPolynomialCoef(CRC, LL_CRC_DEFAULT_CRC32_POLY);
    LL_CRC_SetPolynomialSize(CRC, LL_CRC_POLYLENGTH_32B);
    LL_CRC_SetInitialData(CRC, LL_CRC_DEFAULT_CRC_INITVALUE);
    LL_CRC_SetInputDataReverseMode(CRC, LL_CRC_INDATA_REVERSE_WORD);
    LL_CRC_SetOutputDataReverseMode(CRC, LL_CRC_OUTDATA_REVERSE_BIT);
}

uint32_t crc32(const char* const buf, const uint32_t len) {
    uint32_t data;
    uint32_t index;

    LL_CRC_ResetCRCCalculationUnit(CRC);

    /* Compute the CRC of Data Buffer array*/
    for (index = 0; index < (len / 4); index++) {
        data = (uint32_t)((buf[4 * index + 3] << 24) |
                          (buf[4 * index + 2] << 16) |
                          (buf[4 * index + 1] << 8) | buf[4 * index]);
        LL_CRC_FeedData32(CRC, data);
    }

    /* Last bytes specific handling */
    if ((len % 4) != 0) {
        if (len % 4 == 1) {
            LL_CRC_FeedData8(CRC, buf[4 * index]);
        }
        if (len % 4 == 2) {
            LL_CRC_FeedData16(CRC, (uint16_t)((buf[4 * index + 1] << 8) |
                                              buf[4 * index]));
        }
        if (len % 4 == 3) {
            LL_CRC_FeedData16(CRC, (uint16_t)((buf[4 * index + 1] << 8) |
                                              buf[4 * index]));
            LL_CRC_FeedData8(CRC, buf[4 * index + 2]);
        }
    }

    return LL_CRC_ReadData32(CRC) ^ 0xffffffff;
}

#endif

#if 0
#include <stdio.h>
void test_crc32(void) {
  uint8_t data[] = {100,2,3,4,5,6,7,8};
  
  uint32_t crc = crc32_init();
  for(int i = 0; i < sizeof data; i++) {
    crc = crc32_update_serial(crc, data[i]);
  }
  crc = crc32_finish(crc);

  puts("CRC-32:");
  printf("\tserial = 0x%08"PRIX32"\n", crc);
  
  crc = crc32_init();
  crc = crc32_update_small_block(crc, data, sizeof data);
  crc = crc32_finish(crc);

  printf("\tsmall  = 0x%08"PRIX32"\n", crc);
}

int main(void) {
  test_crc32();
}
#endif
