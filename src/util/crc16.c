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
#include "crc16.h"

#define CRC16_POLY  0x8005  // (x^16) + x^15 + x^2 + x^0  CRC-16/CMS
// Hamming distance 4 up to 32751 data bits (4093 bytes)
// https://users.ece.cmu.edu/~koopman/crc/c16/0xa001_len.txt
// https://reveng.sourceforge.io/crc-catalogue/all.htm


/*
CRC-16 params:
Init:         0xFFFF
Reflect in:   no
Reflect out:  no
XOR out:      0x0000
*/

/*
Initialize a CRC-16

Returns:
  Initial CRC state
*/
uint16_t crc16_init(void) {
  return 0xFFFF;
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
uint16_t crc16_update_serial(uint16_t crc, uint8_t data) {
  crc ^= ((uint16_t)data) << (16-8);
  for(uint8_t i = 0; i < 8; i++) {
    if(crc & (1ul << (16-1)))
      crc = (crc << 1) ^ CRC16_POLY;
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
uint16_t crc16_update_serial_block(uint16_t crc, const uint8_t *data, size_t data_len) {
  for(size_t i = 0; i < data_len; i++) {
    crc ^= ((uint16_t)data[i]) << (16-8);
    for(uint8_t j = 0; j < 8; j++) {
      if(crc & (1ul << (16-1)))
        crc = (crc << 1) ^ CRC16_POLY;
      else
        crc <<= 1;
    }
  }
  return crc;
}


static const uint16_t s_crc16_table[256] = {
  0x0000, 0x8005, 0x800F, 0x000A, 0x801B, 0x001E, 0x0014, 0x8011,
  0x8033, 0x0036, 0x003C, 0x8039, 0x0028, 0x802D, 0x8027, 0x0022,
  0x8063, 0x0066, 0x006C, 0x8069, 0x0078, 0x807D, 0x8077, 0x0072,
  0x0050, 0x8055, 0x805F, 0x005A, 0x804B, 0x004E, 0x0044, 0x8041,
  0x80C3, 0x00C6, 0x00CC, 0x80C9, 0x00D8, 0x80DD, 0x80D7, 0x00D2,
  0x00F0, 0x80F5, 0x80FF, 0x00FA, 0x80EB, 0x00EE, 0x00E4, 0x80E1,
  0x00A0, 0x80A5, 0x80AF, 0x00AA, 0x80BB, 0x00BE, 0x00B4, 0x80B1,
  0x8093, 0x0096, 0x009C, 0x8099, 0x0088, 0x808D, 0x8087, 0x0082,
  0x8183, 0x0186, 0x018C, 0x8189, 0x0198, 0x819D, 0x8197, 0x0192,
  0x01B0, 0x81B5, 0x81BF, 0x01BA, 0x81AB, 0x01AE, 0x01A4, 0x81A1,
  0x01E0, 0x81E5, 0x81EF, 0x01EA, 0x81FB, 0x01FE, 0x01F4, 0x81F1,
  0x81D3, 0x01D6, 0x01DC, 0x81D9, 0x01C8, 0x81CD, 0x81C7, 0x01C2,
  0x0140, 0x8145, 0x814F, 0x014A, 0x815B, 0x015E, 0x0154, 0x8151,
  0x8173, 0x0176, 0x017C, 0x8179, 0x0168, 0x816D, 0x8167, 0x0162,
  0x8123, 0x0126, 0x012C, 0x8129, 0x0138, 0x813D, 0x8137, 0x0132,
  0x0110, 0x8115, 0x811F, 0x011A, 0x810B, 0x010E, 0x0104, 0x8101,
  0x8303, 0x0306, 0x030C, 0x8309, 0x0318, 0x831D, 0x8317, 0x0312,
  0x0330, 0x8335, 0x833F, 0x033A, 0x832B, 0x032E, 0x0324, 0x8321,
  0x0360, 0x8365, 0x836F, 0x036A, 0x837B, 0x037E, 0x0374, 0x8371,
  0x8353, 0x0356, 0x035C, 0x8359, 0x0348, 0x834D, 0x8347, 0x0342,
  0x03C0, 0x83C5, 0x83CF, 0x03CA, 0x83DB, 0x03DE, 0x03D4, 0x83D1,
  0x83F3, 0x03F6, 0x03FC, 0x83F9, 0x03E8, 0x83ED, 0x83E7, 0x03E2,
  0x83A3, 0x03A6, 0x03AC, 0x83A9, 0x03B8, 0x83BD, 0x83B7, 0x03B2,
  0x0390, 0x8395, 0x839F, 0x039A, 0x838B, 0x038E, 0x0384, 0x8381,
  0x0280, 0x8285, 0x828F, 0x028A, 0x829B, 0x029E, 0x0294, 0x8291,
  0x82B3, 0x02B6, 0x02BC, 0x82B9, 0x02A8, 0x82AD, 0x82A7, 0x02A2,
  0x82E3, 0x02E6, 0x02EC, 0x82E9, 0x02F8, 0x82FD, 0x82F7, 0x02F2,
  0x02D0, 0x82D5, 0x82DF, 0x02DA, 0x82CB, 0x02CE, 0x02C4, 0x82C1,
  0x8243, 0x0246, 0x024C, 0x8249, 0x0258, 0x825D, 0x8257, 0x0252,
  0x0270, 0x8275, 0x827F, 0x027A, 0x826B, 0x026E, 0x0264, 0x8261,
  0x0220, 0x8225, 0x822F, 0x022A, 0x823B, 0x023E, 0x0234, 0x8231,
  0x8213, 0x0216, 0x021C, 0x8219, 0x0208, 0x820D, 0x8207, 0x0202
};

/*
Add data to CRC

Uses big table algorithm

Args:
  crc:  Current CRC state
  data: New data for CRC

Returns:
  New CRC state
*/
uint16_t crc16_update(uint16_t crc, uint8_t data) {
  uint8_t top = crc >> (16-8);
  crc = (crc << 8) ^ s_crc16_table[top ^ data];
  return crc;
}


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
uint16_t crc16_update_block(uint16_t crc, const uint8_t *data, size_t data_len) {
  for(size_t i = 0; i < data_len; i++) {
    uint8_t top = crc >> (16-8);
    crc = (crc << 8) ^ s_crc16_table[top ^ data[i]];
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
uint16_t crc16_finish(uint16_t crc) {
  return crc;
}




#ifdef TEST_CRC16

#include <stdio.h>
#include <string.h>

static void crc16_gen_table(void) {
  printf("static const uint16_t s_crc16_table[256] = {\n ");

  for(uint16_t i = 0; i <= 255; i++) {
    uint16_t crc = i << (16-8);
    for(int j = 0; j < 8; j++) {
      if(crc & (1ul << (16-1)))
        crc = (crc << 1) ^ CRC16_POLY;
      else
        crc <<= 1;
    }

    printf(" 0x%04X,", crc);
    if((i+1) % 8 == 0)
      printf("\n ");
  }

  puts("\n};");
}


int main(void) {
  const char check[] = "123456789";

  uint16_t scrc = crc16_init();
  uint16_t tcrc = crc16_init();
  for(int i = 0; i < strlen(check); i++) {
    scrc = crc16_update_serial(scrc, check[i]);
    tcrc = crc16_update(tcrc, check[i]);
  }

  scrc = crc16_finish(scrc);
  tcrc = crc16_finish(tcrc);

  printf("CRC-16: serial=%04X  table=%04X  %s\n", scrc, tcrc, scrc == tcrc ? "ok" : "BAD");

  uint16_t bcrc = crc16_init();
  bcrc = crc16_update_block(bcrc, (const uint8_t *)check, strlen(check));
  bcrc = crc16_finish(bcrc);

  printf("\tblock=%04X\n", bcrc);

//  crc16_gen_table();

  return 0;
}

#endif
