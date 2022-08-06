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

/*
------------------------------------------------------------------------------

------------------------------------------------------------------------------
*/

#ifndef PROP_DB_H
#define PROP_DB_H

#include "cstone/log_db.h"
#include "cstone/umsg.h"
#include "util/dhash.h"
#include "util/mempool.h"

#include "FreeRTOS.h"
#include "semphr.h"


#define P_KIND_NONE    0x00
#define P_KIND_UINT    0x01
#define P_KIND_INT     0x02
#define P_KIND_STRING  0x03
#define P_KIND_BLOB    0x04
#define P_KIND_FUNC    0x05


#define P_EVENT_STORAGE_PROP_UPDATE    (P1_EVENT | P2_STORAGE | P3_PROP | P4_UPDATE)



// Property state stored in hash table
typedef struct {
  uintptr_t value;  // Integer value, pointer to data, or callback function
  size_t    size;   // Size of data

  // NOTE: This struct is padded out to sizeof(uintptr_t) in the dhash.
  // With 32-bit pointers we can have four bytes of field values before bitfields
  // are needed.
  uint8_t   kind;     // Type of data stored in value
  bool      readonly; // prop_set() not allowed
  bool      persist;  // Properties that belong in non-volatile storage
  bool      dirty;    // Modified by prop_set()
} PropDBEntry;


typedef struct {
  uint32_t  prop;
  uintptr_t value;
  uint8_t   kind;
  uint8_t   attributes;
} PropDefaultDef;

// Attribute flags for PropDefaultDef
#define P_READONLY 0x01
#define P_PERSIST  0x02

// Macros for defining array of default props
#define P_UINT(p, val, attr)  {(p), (val), P_KIND_UINT, (attr)}
#define P_INT(p,  val, attr)  {(p), (val), P_KIND_INT, (attr)}
#define P_STR(p,  val, attr)  {(p), (uintptr_t)(val), P_KIND_STRING, (attr)}
#define P_END_DEFAULTS        {0, 0, P_KIND_NONE, 0}

typedef struct {
  dhash       hash;         // Contains the properties
  mpPoolSet  *pool_set;     // Memory pool for allocated values
  UMsgTarget *msg_hub;      // Message hub for announcing prop updates
  SemaphoreHandle_t lock;
  uint32_t    transactions; // Number of pending transactions (NOTE: This is actually atomic_uint)
  bool        persist_updated; // Persisted properties have been changed
} PropDB;


#ifdef __cplusplus
extern "C" {
#endif

// ******************** Resource management ********************
bool prop_db_init(PropDB *db, size_t init_capacity, size_t max_storage, mpPoolSet *pool_set);
void prop_db_free(PropDB *db);
void prop_db_set_defaults(PropDB *db, const PropDefaultDef *defaults);
void prop_db_set_msg_hub(PropDB *db, UMsgTarget *msg_hub);

void prop_db_transact_begin(PropDB *db);
void prop_db_transact_end(PropDB *db);
void prop_db_transact_end_no_update(PropDB *db);

// ******************** Retrieval ********************
bool prop_set(PropDB *db, uint32_t prop, PropDBEntry *value, uint32_t source);
bool prop_set_str(PropDB *db, uint32_t prop, char *value, uint32_t source);
bool prop_set_int(PropDB *db, uint32_t prop, int32_t value, uint32_t source);
bool prop_set_uint(PropDB *db, uint32_t prop, uint32_t value, uint32_t source);
bool prop_get(PropDB *db, uint32_t prop, PropDBEntry *value);

bool prop_set_attributes(PropDB *db, uint32_t prop, uint8_t attributes);
bool prop_get_attributes(PropDB *db, uint32_t prop, uint8_t *attributes);

// ******************** Utility ********************
static inline void *prop_db_alloc(PropDB *db, size_t size, size_t *alloc_size) {
  return mp_alloc(db->pool_set, size, alloc_size);
}

static inline bool prop_db_dealloc(PropDB *db, void *element) {
  return mp_free(db->pool_set, element);
}

size_t prop_db_count(PropDB *db);
bool prop_print(PropDB *db, uint32_t prop);
void prop_db_dump(PropDB *db);

bool prop_db_serialize(PropDB *db, LogDBBlock **block);
unsigned prop_db_deserialize(PropDB *db, uint8_t *data, size_t data_len);

size_t prop_db_all_keys(PropDB *db, uint32_t **keys);
void prop_db_sort_keys(PropDB *db, uint32_t *keys, size_t keys_len);
void prop_db_dump_keys(PropDB *db, uint32_t *keys, size_t keys_len);

#ifdef __cplusplus
}
#endif


#endif // PROP_DB_H
