#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "cstone/prop_db.h"
#include "cstone/prop_serialize.h"
#include "bsd/string.h"
#include "util/mempool.h"

extern mpPoolSet g_pool_set;


static uint32_t zigzag_encode(int32_t n) {
  return (n << 1) ^ (n >> 31);
}

static int32_t zigzag_decode(uint32_t n) {
  return (n >> 1) ^ (-(n & 0x01));
}


// __builtin_clz() added in GCC 3.4 and Clang 5
#if (defined __GNUC__ && __GNUC__ >= 4) || (defined __clang__ && __clang_major__ >= 5)
#  define HAVE_BUILTIN_CLZ
#endif

#ifdef HAVE_BUILTIN_CLZ
#  define clz(x)  __builtin_clz(x)
#else
// Count leading zeros
// From Hacker's Delight 2nd ed. Fig 5-12. Modified to support 16-bit ints.
static int clz(unsigned x) {
  static_assert(sizeof(x) <= 4, "clz() only supports a 32-bit or 16-bit argument");
  unsigned y;
  int n = sizeof(x) * 8;

  if(sizeof(x) > 2) { // 32-bit x
    y = x >> 16; if(y) {n -= 16; x = y;}
  }
  y = x >> 8;  if(y) {n -= 8; x = y;}
  y = x >> 4;  if(y) {n -= 4; x = y;}
  y = x >> 2;  if(y) {n -= 2; x = y;}
  y = x >> 1;  if(y) return n - 2;

  return n - x;
}
#endif

unsigned varint_encoded_bytes(uint32_t n) {
  if(n == 0)
    return 1;

#if 0
  unsigned count = 0;

  while(n) {
    count++;
    n >>= 7;
  }
  return count;

#else
  unsigned bits = sizeof(n) * 8 - clz(n);
  return (bits + 6) / 7;
#endif

}


// Encode as LEB128
int varint_encode(uint32_t n, uint8_t *buf, size_t buf_size) {
  unsigned num_bytes = varint_encoded_bytes(n);

  if(buf_size < num_bytes)
    return -num_bytes;

  while(n & ~0x7Ful) {
    *buf++ = (uint8_t)n | 0x80; // low bytes have MSB set
    n >>= 7;
  }
  *buf = (uint8_t)n; // Upper byte has MSB clear

  return num_bytes;
}


int varint_decode(uint32_t *n, uint8_t *buf) {
  uint32_t val = 0;
  uint32_t b7;
  unsigned bit_offset = 0;
  int num_bytes = 0;

  do {
    b7 = *buf++;
    val |= (uint32_t)(b7 & 0x7F) << bit_offset;
    bit_offset += 7;
    num_bytes++;
  } while(b7 & 0x80);

  *n = val;
  return num_bytes;
}


int uint32_encode(uint32_t n, uint8_t *buf, size_t buf_size) {
  if(buf_size < sizeof(n))
    return -(int)sizeof(n);

  while(n) {
    *buf++ = (uint8_t)(n & 0xFF);
    n >>= 8;
  }

  *buf = (uint8_t)n;

  return sizeof(n);
}


int uint32_decode(uint32_t *n, uint8_t *buf) {
  uint32_t val = 0;
  uint32_t b8;
  unsigned bit_offset = 0;

  for(unsigned num_bytes = sizeof(val); num_bytes > 0; num_bytes--) {
    b8 = *buf++;
    val |= b8 << bit_offset;
    bit_offset += 8;
  }

  *n = val;
  return sizeof(val);
}


static inline unsigned string_encoded_bytes(char *str, size_t str_len) {
  if(str_len == 0)
    str_len = strlen(str);
  unsigned num_bytes = varint_encoded_bytes(str_len);

  num_bytes += str_len;

  return num_bytes;
}


int string_encode(char *str, uint8_t *buf, size_t buf_size) {
  size_t str_len = strlen(str);

  unsigned num_bytes = string_encoded_bytes(str, str_len);
  if(num_bytes > buf_size)
    return -num_bytes;

  buf += varint_encode(str_len, buf, buf_size);

  while(*str != '\0') {
    *buf++ = (uint8_t)*str++;
  }

  return num_bytes;
}


int string_decode(char *str, size_t str_size, uint8_t *buf) {
  int num_bytes = 0;

  uint32_t str_len;
  num_bytes += varint_decode(&str_len, buf);
  buf += num_bytes; // Move to string data

  num_bytes += str_len;

  if(str_len+1 > str_size)  // Make sure we have space in string
    return -num_bytes;  // Does not include NUL

  strlcpy(str, (char *)buf, str_len+1);

  return num_bytes;
}


static inline unsigned blob_encoded_bytes(uint8_t *data, size_t data_size) {
  unsigned num_bytes = varint_encoded_bytes(data_size);

  num_bytes += data_size;

  return num_bytes;
}


int blob_encode(uint8_t *data, size_t data_size, uint8_t *buf, size_t buf_size) {
  unsigned num_bytes = blob_encoded_bytes(data, data_size);
  if(num_bytes > buf_size)
    return -num_bytes;

  buf += varint_encode(data_size, buf, buf_size);
  memcpy(buf, data, data_size);

  return num_bytes;
}



unsigned prop_encoded_bytes(uint32_t prop, PropDBEntry *entry) {
  unsigned num_bytes = sizeof(prop) + 1; // Prop ID + kind byte

  switch(entry->kind) {
  case P_KIND_UINT:
    num_bytes += varint_encoded_bytes(entry->value);
    break;

  case P_KIND_INT:
    num_bytes += varint_encoded_bytes(zigzag_encode(entry->value));
    break;

  case P_KIND_STRING:
  case P_KIND_BLOB:
    num_bytes += varint_encoded_bytes(entry->size);
    num_bytes += entry->size;
    break;

  default:
    break;
  }

  return num_bytes;
}


int prop_encode(uint32_t prop, PropDBEntry *entry, uint8_t *buf, size_t buf_size) {
  int num_bytes = prop_encoded_bytes(prop, entry);

  if((unsigned)num_bytes > buf_size)
    return -num_bytes;

  // Serialize the prop kind
  *buf++ = entry->kind;
  buf_size--;

  // Serialize prop ID
  int encode_size = uint32_encode(prop, buf, buf_size);
  buf += encode_size;
  buf_size -= encode_size;

  // Serialize the value
  switch(entry->kind) {
  case P_KIND_UINT:
    varint_encode(entry->value, buf, buf_size);
    break;

  case P_KIND_INT:
    varint_encode(zigzag_encode(entry->value), buf, buf_size);
    break;

  case P_KIND_STRING:
    string_encode((char *)entry->value, buf, buf_size);
    break;

  case P_KIND_BLOB:
    blob_encode((uint8_t *)entry->value, entry->size, buf, buf_size);
    break;

  default:
    break;
  }

  return num_bytes;
}


int prop_decode(uint32_t *prop, PropDBEntry *entry, uint8_t *buf) {
  int num_bytes = 0;
  uint32_t val;

  memset(entry, 0, sizeof *entry);

  entry->kind = *buf;
  buf++;
  num_bytes++;

  int encode_size = uint32_decode(prop, buf);
  buf += encode_size;
  num_bytes += encode_size;

  switch(entry->kind) {
  case P_KIND_UINT:
    encode_size = varint_decode(&val, buf);
    entry->value = val;
    buf += encode_size;
    num_bytes += encode_size;
//    printf("## DEC UINT: %u\n", entry->value);
    break;

  case P_KIND_INT:
    encode_size = varint_decode(&val, buf);
    entry->value = zigzag_decode(val);
    buf += encode_size;
    num_bytes += encode_size;
//    printf("## DEC INT: %d\n", entry->value);
    break;

  case P_KIND_STRING:
  {
    // Get length
    encode_size = varint_decode(&val, buf);
    entry->size = val;
    buf += encode_size;
    num_bytes += encode_size + val;

    char *str = mp_alloc(&g_pool_set, entry->size+1, NULL);
    if(str) {
      strlcpy(str, (char *)buf, entry->size+1);
      entry->value = (uintptr_t)str;
      //printf("## DEC STR: %u, '%s'\n", entry->size, (char *)entry->value);
    } else {
      entry->value = (uintptr_t)NULL;
    }
    break;
  }

  case P_KIND_BLOB:
  {
    // Get length
    encode_size = varint_decode(&val, buf);
    entry->size = val;
    buf += encode_size;
    num_bytes += encode_size + val;

    char *data = mp_alloc(&g_pool_set, entry->size, NULL);
    if(data) {
      memcpy(data, (char *)buf, entry->size);
      entry->value = (uintptr_t)data;
      //printf("## DEC BLB: %u\n", entry->size);
    } else {
      entry->value = (uintptr_t)NULL;
    }

    // All blob data is "sytem origin" so we don't want users overwriting it from console
    entry->protect = true;
    break;
  }

  default:
    break;
  }

  entry->persist = true;
  entry->readonly = false;

  return num_bytes;
}



#ifdef TEST_SERIALIZE

#include <stdio.h>
#include "util/hex_dump.h"

int main(void) {
  uint8_t buf[256] = {0};

  uint8_t *pos = buf;

#if 0
  for(int i = 0; i < 30; i++) {
    pos += varint_encode(zigzag_encode(-i*200), pos, sizeof(buf) - (pos - buf));
  }
#endif

  uint32_t prop = 0x20304050;
  PropDBEntry entry;

  memset(&entry, 0, sizeof(entry));
  char sval[] = "SVAL";
  entry.value = (uintptr_t)sval;
  entry.size = strlen(sval);
  entry.kind = P_KIND_STRING;

  pos += prop_encode(prop, &entry, pos, sizeof(buf) - (pos - buf));
  printf("## Prop encode size: %ld\n", pos - buf);

  prop = 0xaabbccdd;
  entry.value = -42;
  entry.size = 0;
  entry.kind = P_KIND_INT;
  pos += prop_encode(prop, &entry, pos, sizeof(buf) - (pos - buf));

  size_t encoded_len = pos - buf;

  pos = buf;
  for(int i = 0; i < 2; i++) {
    int decode_bytes = prop_decode(&prop, &entry, pos);
    printf("## DECODE %d\n", decode_bytes);
    if(decode_bytes <= 0)
      break;
    pos += decode_bytes;
    printf("## PROP: %08X %d %d\n", prop, entry.kind, (int32_t)entry.value);
    printf("## Prop decode size: %u\n", decode_bytes);
    if(entry.kind == P_KIND_STRING)
      printf("## STR: '%s'\n", (char *)entry.value);

    if((size_t)(pos - buf) >= encoded_len)
      break;
  }

#if 0
  pos = buf;
  for(int i = 0; i < 30; i++) {
    uint32_t val;
    pos += varint_decode(&val, pos);
    printf("%d\n", zigzag_decode(val));
  }
#endif

  dump_array(buf, sizeof(buf));

  return 0;
}

#endif
