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

#ifndef CRC8_H
#define CRC8_H

#ifdef __cplusplus
extern "C" {
#endif

uint8_t crc8_init(void);

uint8_t crc8_update_serial(uint8_t crc, uint8_t data);
uint8_t crc8_update_serial_block(uint8_t crc, const uint8_t *data, size_t data_len);

uint8_t crc8_update_small_block(uint8_t crc, const uint8_t *data, size_t data_len);
uint16_t crc8_update_block(uint8_t crc, const uint8_t *data, size_t data_len);

uint8_t crc8_finish(uint8_t crc);

#ifdef __cplusplus
}
#endif


#endif // CRC8_H
