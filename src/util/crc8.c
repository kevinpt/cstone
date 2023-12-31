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
#include "util/crc8.h"

#define CRC8_POLY  0x31   // (x^8) + x^5 +x^4 +x^0  CRC-8/NRSC-5
// Hamming distance 4 up to 119 data bits (14 bytes)
// https://users.ece.cmu.edu/~koopman/crc/c08/0x98_len.txt
// https://reveng.sourceforge.io/crc-catalogue/all.htm

/*
CRC-8 params:
Init:         0xFF
Reflect in:   no
Reflect out:  no
XOR out:      0x00
*/

/*
Initialize a CRC-8

Returns:
  Initial CRC state
*/
uint8_t crc8_init(void) {
  return 0xFF;
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
uint8_t crc8_update_serial(uint8_t crc, uint8_t data) {
  crc ^= data;
  for(uint8_t i = 0; i < 8; i++) {
    if(crc & (1ul << (8-1)))
      crc = (crc << 1) ^ CRC8_POLY;
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
uint8_t crc8_update_serial_block(uint8_t crc, const uint8_t *data, size_t data_len) {
  for(size_t i = 0; i < data_len; i++) {
    crc ^= data[i];
    for(uint8_t j = 0; j < 8; j++) {
      if(crc & (1ul << (8-1)))
        crc = (crc << 1) ^ CRC8_POLY;
      else
        crc <<= 1;
    }
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
uint8_t crc8_finish(uint8_t crc) {
  return crc;
}



static const uint8_t s_crc8_table_small[16] = {
  0x00, 0x31, 0x62, 0x53, 0xC4, 0xF5, 0xA6, 0x97,
  0xB9, 0x88, 0xDB, 0xEA, 0x7D, 0x4C, 0x1F, 0x2E
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
uint8_t crc8_update_small_block(uint8_t crc, const uint8_t *data, size_t data_len) {
  const uint8_t *d8 = data;

  while(data_len--) {
    crc = (crc << 4) ^ s_crc8_table_small[(crc ^ *d8) >> 4]; // Upper nibble
    crc = (crc << 4) ^ s_crc8_table_small[((crc >> 4) ^ *d8++) & 0x0F]; // Lower nibble
  }

  return crc;
}



static const uint8_t s_crc8_table[256] = {
  0x00, 0x31, 0x62, 0x53, 0xC4, 0xF5, 0xA6, 0x97,
  0xB9, 0x88, 0xDB, 0xEA, 0x7D, 0x4C, 0x1F, 0x2E,
  0x43, 0x72, 0x21, 0x10, 0x87, 0xB6, 0xE5, 0xD4,
  0xFA, 0xCB, 0x98, 0xA9, 0x3E, 0x0F, 0x5C, 0x6D,
  0x86, 0xB7, 0xE4, 0xD5, 0x42, 0x73, 0x20, 0x11,
  0x3F, 0x0E, 0x5D, 0x6C, 0xFB, 0xCA, 0x99, 0xA8,
  0xC5, 0xF4, 0xA7, 0x96, 0x01, 0x30, 0x63, 0x52,
  0x7C, 0x4D, 0x1E, 0x2F, 0xB8, 0x89, 0xDA, 0xEB,
  0x3D, 0x0C, 0x5F, 0x6E, 0xF9, 0xC8, 0x9B, 0xAA,
  0x84, 0xB5, 0xE6, 0xD7, 0x40, 0x71, 0x22, 0x13,
  0x7E, 0x4F, 0x1C, 0x2D, 0xBA, 0x8B, 0xD8, 0xE9,
  0xC7, 0xF6, 0xA5, 0x94, 0x03, 0x32, 0x61, 0x50,
  0xBB, 0x8A, 0xD9, 0xE8, 0x7F, 0x4E, 0x1D, 0x2C,
  0x02, 0x33, 0x60, 0x51, 0xC6, 0xF7, 0xA4, 0x95,
  0xF8, 0xC9, 0x9A, 0xAB, 0x3C, 0x0D, 0x5E, 0x6F,
  0x41, 0x70, 0x23, 0x12, 0x85, 0xB4, 0xE7, 0xD6,
  0x7A, 0x4B, 0x18, 0x29, 0xBE, 0x8F, 0xDC, 0xED,
  0xC3, 0xF2, 0xA1, 0x90, 0x07, 0x36, 0x65, 0x54,
  0x39, 0x08, 0x5B, 0x6A, 0xFD, 0xCC, 0x9F, 0xAE,
  0x80, 0xB1, 0xE2, 0xD3, 0x44, 0x75, 0x26, 0x17,
  0xFC, 0xCD, 0x9E, 0xAF, 0x38, 0x09, 0x5A, 0x6B,
  0x45, 0x74, 0x27, 0x16, 0x81, 0xB0, 0xE3, 0xD2,
  0xBF, 0x8E, 0xDD, 0xEC, 0x7B, 0x4A, 0x19, 0x28,
  0x06, 0x37, 0x64, 0x55, 0xC2, 0xF3, 0xA0, 0x91,
  0x47, 0x76, 0x25, 0x14, 0x83, 0xB2, 0xE1, 0xD0,
  0xFE, 0xCF, 0x9C, 0xAD, 0x3A, 0x0B, 0x58, 0x69,
  0x04, 0x35, 0x66, 0x57, 0xC0, 0xF1, 0xA2, 0x93,
  0xBD, 0x8C, 0xDF, 0xEE, 0x79, 0x48, 0x1B, 0x2A,
  0xC1, 0xF0, 0xA3, 0x92, 0x05, 0x34, 0x67, 0x56,
  0x78, 0x49, 0x1A, 0x2B, 0xBC, 0x8D, 0xDE, 0xEF,
  0x82, 0xB3, 0xE0, 0xD1, 0x46, 0x77, 0x24, 0x15,
  0x3B, 0x0A, 0x59, 0x68, 0xFF, 0xCE, 0x9D, 0xAC,
 
};

/*
Add data block to CRC

Uses big table algorithm

Args:
  crc:      Current CRC state
  data:     Array of data to compute CRC over
  data_len: Size of data array

Returns:
  New CRC state
*/
uint16_t crc8_update_block(uint8_t crc, const uint8_t *data, size_t data_len) {
  for(size_t i = 0; i < data_len; i++) {
    crc = s_crc8_table[crc ^ data[i]];
  }
  return crc;
}



#ifdef TEST_CRC8

#include <stdio.h>
#include <string.h>

static void crc8_gen_table_small(void) {
  printf("static const uint8_t s_crc8_table_small[16] = {\n ");

  for(uint16_t i = 0; i < 16; i++) {
    uint8_t crc = i << (8-4);
    for(int j = 0; j < 4; j++) {
      if(crc & (1ul << (8-1)))
        crc = (crc << 1) ^ CRC8_POLY;
      else
        crc <<= 1;
    }

    printf(" 0x%02X,", crc);
    if((i+1) % 8 == 0)
      printf("\n ");
  }

  puts("\n};");
}

static void crc8_gen_table(void) {
  printf("static const uint8_t s_crc8_table[256] = {\n ");

  for(uint16_t i = 0; i <= 255; i++) {
    uint8_t crc = i;
    for(int j = 0; j < 8; j++) {
      if(crc & (1ul << (8-1)))
        crc = (crc << 1) ^ CRC8_POLY;
      else
        crc <<= 1;
    }

    printf(" 0x%02X,", crc);
    if((i+1) % 8 == 0)
      printf("\n ");
  }

  puts("\n};");
}


int main(void) {
  const char *check = "123456789";

  uint8_t scrc = crc8_init();
  for(int i = 0; i < strlen(check); i++) {
    scrc = crc8_update_serial(scrc, check[i]);
  }

  scrc = crc8_finish(scrc);

  printf("CRC-8: serial=%02X\n", scrc);

  uint8_t hcrc = crc8_init();
  hcrc = crc8_update_small_block(hcrc, (const uint8_t *)check, strlen(check));
  hcrc = crc8_finish(hcrc);

  uint8_t tcrc = crc8_init();
  tcrc = crc8_update_block(tcrc, (const uint8_t *)check, strlen(check));
  tcrc = crc8_finish(tcrc);


  printf("hcrc=%02X  tcrc=%02X\n", hcrc, tcrc);

  //crc8_gen_table_small();
  //crc8_gen_table();

  return 0;
}

#endif
