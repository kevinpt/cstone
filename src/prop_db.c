#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>

#include "util/dhash.h"
#include "util/mempool.h"

#include "build_config.h"
#include "cstone/platform.h"
#include "cstone/prop_id.h"
#include "cstone/prop_db.h"
#include "cstone/prop_serialize.h"
#include "cstone/blocking_io.h"
#include "cstone/term_color.h"


#define MAX_PROP_NAME_LEN  48

// PropDB.transactions is declared as uint32_t but we will use it
// here as atomic_uint. This avoids the need for an opaque type.
_Static_assert(sizeof(uint32_t) >= sizeof(atomic_uint), "PropDB.transactions too small");


#define LOCK()    xSemaphoreTake(db->lock, portMAX_DELAY)
#define UNLOCK()  xSemaphoreGive(db->lock)



static void prop_item_destroy(dhKey key, void *value, void *ctx) {
  PropDB *db = (PropDB *)ctx;
  PropDBEntry *entry = (PropDBEntry *)value;

  // Free resources based on entry kind
  switch(entry->kind) {
  case P_KIND_UINT:
  case P_KIND_INT:
    break;

  case P_KIND_STRING:
  case P_KIND_BLOB:
    if(mp_free(db->pool_set, (void *)entry->value))
      entry->value = (uintptr_t)NULL;
    break;

  default: break;
  }
}

static bool prop_item_replace(dhKey key, void *old_value, void *new_value, void *ctx) {
  PropDB *db = (PropDB *)ctx;
  PropDBEntry *old_entry = (PropDBEntry *)old_value;
  PropDBEntry *new_entry = (PropDBEntry *)new_value;

  if(old_entry->readonly) // Can't replace
    return false;

  // Preserve original attribute settings for this prop
  new_entry->readonly = old_entry->readonly;
  new_entry->persist  = old_entry->persist;

  if(new_entry->kind == P_KIND_NONE)
    new_entry->kind = old_entry->kind;

  // Attempt to free string and binary values
  if(old_entry->kind == P_KIND_STRING || old_entry->kind == P_KIND_BLOB) {
    if(mp_free(db->pool_set, (void *)old_entry->value))
      old_entry->value = (uintptr_t)NULL;
  }

  if(new_entry->persist)  // Allow prop transaction end events
    db->persist_updated = true;

  return true; // Allow replacement
}

static dhIKey prop_gen_hash(dhKey key) {
  dhIKey ikey = (uintptr_t)key.data; // Data pointer is a prop ID

  // This will be hashed by dh__hash_int()
  return ikey;
}


static bool prop_equal_hash_keys(dhKey key1, dhKey key2, void *ctx) {
  return (uint32_t)(uintptr_t)key1.data == (uint32_t)(uintptr_t)key2.data;
}



bool prop_db_init(PropDB *db, size_t init_capacity, size_t max_storage, mpPoolSet *pool_set) {
  memset(db, 0, sizeof(*db));

  db->pool_set  = pool_set;

  db->lock = xSemaphoreCreateBinary();
  xSemaphoreGive(db->lock);

  atomic_init((atomic_uint *)&db->transactions, 0);
  db->persist_updated = false;

  // Setup hash table for prop:PropDBEntry pairs
  dhConfig hash_cfg = {
    .init_buckets = init_capacity,
    .value_size   = sizeof(PropDBEntry),
    .max_storage  = max_storage,
    .destroy_item = prop_item_destroy,
    .replace_item = prop_item_replace,
    .gen_hash     = prop_gen_hash,
    .is_equal     = prop_equal_hash_keys
  };

  return dh_init(&db->hash, &hash_cfg, db);
}

void prop_db_free(PropDB *db) {
  vSemaphoreDelete(db->lock);
  db->lock = 0;
  dh_free(&db->hash);
}


void prop_db_set_defaults(PropDB *db, const PropDefaultDef *defaults) {
  const PropDefaultDef *cur = defaults;

  while(cur->prop != 0) {
    PropDBEntry value = {
      .value = cur->value,
      .kind = cur->kind,
      .readonly = (bool)(cur->attributes & P_READONLY),
      .persist  = (bool)(cur->attributes & P_PERSIST)
    };

    if(cur->kind == P_KIND_STRING) {
      value.size = strlen((char *)cur->value);
    }

    prop_set(db, cur->prop, &value, 0);

    cur++;
  }
}


void prop_db_set_msg_hub(PropDB *db, UMsgTarget *msg_hub) {
  db->msg_hub = msg_hub;
}


void prop_db_transact_begin(PropDB *db) {
  atomic_fetch_add((atomic_uint *)&db->transactions, 1);
}


void prop_db_transact_end(PropDB *db) {
  atomic_fetch_sub((atomic_uint *)&db->transactions, 1);

  if(atomic_load((atomic_uint *)&db->transactions) == 0 && db->persist_updated) {
    if(db->msg_hub) { // Notify end of transaction
      UMsg msg = {
        .id     = P_EVENT_STORAGE_PROP_UPDATE,
        .source = 0
      };

      umsg_hub_send(db->msg_hub, &msg, NO_TIMEOUT);
      db->persist_updated = false;
    }
  }
}


void prop_db_transact_end_no_update(PropDB *db) {
  atomic_fetch_sub((atomic_uint *)&db->transactions, 1);
}


// ******************** Retrieval ********************

bool prop_set(PropDB *db, uint32_t prop, PropDBEntry *value, uint32_t source) {
  bool status;

  if(!prop_is_valid(prop, /*allow_mask*/ false)) return false;

  prop_db_transact_begin(db);

  dhKey key = {
    .data = (void *)(uintptr_t)prop,
    .length = sizeof(uint32_t)
  };

  if(value->kind == P_KIND_STRING && value->size == 0) {
    value->size = strlen((char *)value->value);
  }

  LOCK();
    if(value) {
      value->dirty = true;
      status = dh_insert(&db->hash, key, value);
      // replace_item callback ensures that persist attribute remains unchanged
      // from original call to prop_set(). It changes the value struct to reflect
      // the current attributes.
      if(value->persist) {
//        printf(">> PERSIST P%08lX  %c\n", prop,  old_persist ? 'P' : 'e');
        db->persist_updated = true;
      } else {
//        printf(">> EPHEMERAL P%08lX\n", prop);
      }

    } else {
      status = dh_remove(&db->hash, key, NULL);
    }
  UNLOCK();

  if(db->msg_hub) {
    // Send message
    UMsg msg = {
      .id     = prop,
      .source = source
    };

    if(value->kind == P_KIND_UINT || value->kind == P_KIND_INT) {
      msg.payload = value->value;
    }

    umsg_hub_send(db->msg_hub, &msg, NO_TIMEOUT);
  }

  prop_db_transact_end(db);

  return status;
}


bool prop_set_str(PropDB *db, uint32_t prop, char *value, uint32_t source) {
  PropDBEntry entry = {
    .value = (uintptr_t)value,
    .size = strlen(value),
    .kind = P_KIND_STRING
  };

  return prop_set(db, prop, &entry, source);
}

bool prop_set_int(PropDB *db, uint32_t prop, int32_t value, uint32_t source) {
  PropDBEntry entry = {
    .value = (uintptr_t)value,
    .kind = P_KIND_INT
  };

  return prop_set(db, prop, &entry, source);
}

bool prop_set_uint(PropDB *db, uint32_t prop, uint32_t value, uint32_t source) {
  PropDBEntry entry = {
    .value = (uintptr_t)value,
    .kind = P_KIND_UINT
  };

  return prop_set(db, prop, &entry, source);
}

bool prop_get(PropDB *db, uint32_t prop, PropDBEntry *value) {
  dhKey key = {
    .data = (void *)(uintptr_t)prop,
    .length = sizeof(uint32_t)
  };

  LOCK();
    bool status = dh_lookup(&db->hash, key, value);
  UNLOCK();

  return status;
}


bool prop_set_attributes(PropDB *db, uint32_t prop, uint8_t attributes) {
  dhKey key = {
    .data = (void *)(uintptr_t)prop,
    .length = sizeof(uint32_t)
  };

  PropDBEntry *entry;

  LOCK();
    bool status = dh_lookup_in_place(&db->hash, key, (void **)&entry);

    if(status && entry) {
      if(!entry->persist && (attributes & P_PERSIST))
        db->persist_updated = true;

      entry->persist = attributes & P_PERSIST;
      entry->readonly = attributes & P_READONLY;
    }
  UNLOCK();

  return status;
}

bool prop_get_attributes(PropDB *db, uint32_t prop, uint8_t *attributes) {
  if(!attributes)
    return false;

  PropDBEntry entry;
  if(prop_get(db, prop, &entry)) {
    uint8_t attrs = 0;
    if(entry.persist)  attrs |= P_PERSIST;
    if(entry.readonly) attrs |= P_READONLY;

    *attributes = attrs;
    return true;
  }

  return false;
}


// ******************** Utility ********************


size_t prop_db_count(PropDB *db) {
  LOCK();
    size_t count = dh_num_items(&db->hash);
  UNLOCK();
  return count;
}


static void prop__print_entry(uint32_t prop, PropDBEntry *entry) {
  char name[MAX_PROP_NAME_LEN + 1];
  prop_get_name(prop, name, sizeof(name));
  static int s_longest_name = 20;

  if(strlen(name) > (size_t)s_longest_name)
    s_longest_name = strlen(name);

  if(entry->persist)
    fputs(A_CYN, stdout);

  bprintf(PROP_ID "  %-*s (%s", prop, s_longest_name, name, entry->readonly ? "ro" : "rw");

  if(entry->persist)
    fputs(",p)  = ", stdout);
  else
    fputs(")    = ", stdout);

  switch(entry->kind) {
  case P_KIND_UINT:   printf("%" PRIu32 " (%08" PRIX32 ")", (uint32_t)entry->value, (uint32_t)entry->value); break;
  case P_KIND_INT:    printf("%" PRId32, (int32_t)entry->value); break;
  case P_KIND_STRING: printf("'%s'", (char *)entry->value); break;
  default: puts("?"); break;
  }

  if(entry->persist)
    puts(A_NONE);
  else
    putnl();
}

bool prop_print(PropDB *db, uint32_t prop) {
  PropDBEntry entry;

  bool exists = prop_get(db, prop, &entry);

  if(exists)
    prop__print_entry(prop, &entry);

  return exists;
}


void prop_db_dump(PropDB *db) {
  printf("Prop DB (%" PRIuz " items):\n", dh_num_items(&db->hash));

  // NOTE: We do not lock the DB to prevent long lockouts while printing.
  // The iterator may end up invalid if the DB is modified.
  dhKey key;
  PropDBEntry *entry;

  dhIter it;
  dh_iter_init(&db->hash, &it);

  while(dh_iter_next(&it, &key, (void **)&entry)) {
    uint32_t prop = (uintptr_t)key.data;
    prop__print_entry(prop, entry);
  }

}


bool prop_db_serialize(PropDB *db, LogDBBlock **block) {
  dhIter it;
  dhKey key;
  PropDBEntry *entry;

  /*  Block:
      [header] [data]
  */

  LOCK();
    // Get size of block
    dh_iter_init(&db->hash, &it);
    size_t data_len = 0;
    while(dh_iter_next(&it, &key, (void **)&entry)) {
      if(!entry->persist || entry->readonly)
        continue;

      data_len += prop_encoded_bytes((uintptr_t)key.data, entry);
    }

    LogDBBlock *new_block = cs_malloc(sizeof(LogDBBlock) + data_len);
    if(!new_block) {
      *block = NULL;
      UNLOCK();
      return false;
    }

    new_block->kind       = BLOCK_KIND_PROP_DB;
    new_block->compressed = 0;
    new_block->data_len   = data_len;

    // Serialize props
    uint8_t *pos = new_block->data;
    uint8_t *end = pos + data_len;

    dh_iter_init(&db->hash, &it);
    while(dh_iter_next(&it, &key, (void **)&entry)) {
      if(!entry->persist || entry->readonly)
        continue;

  //    printf("## ENCODE: P%04lX\n", (uint32_t)key.data);
      pos += prop_encode((uintptr_t)key.data, entry, pos, end - pos);
    }
  UNLOCK();
//  puts("BLOCK W:");
//  dump_array((uint8_t *)new_block, sizeof(*new_block) + data_len);

  *block = new_block;
  return true;
}


unsigned prop_db_deserialize(PropDB *db, uint8_t *data, size_t data_len) {
  unsigned count = 0;
  uint8_t *pos = data;
  uint8_t *end = pos + data_len;
  uint32_t prop;
  PropDBEntry entry;

  prop_db_transact_begin(db);

    while(pos < end) {
      pos += prop_decode(&prop, &entry, pos);
//      printf("## RESTORE P%08" PRIX32 "\n", prop);
      prop_set(db, prop, &entry, 0);
      count++;
    }

  prop_db_transact_end(db);

  return count;
}


size_t prop_db_all_keys(PropDB *db, uint32_t **keys) {
  uint32_t *key_vec = NULL;

  LOCK();
    size_t num_keys = dh_num_items(&db->hash);

    if(num_keys > 0)
      key_vec = cs_malloc(num_keys * sizeof(uint32_t));

    if(!key_vec) {
      *keys = NULL;
      UNLOCK();
      return 0;
    }

    uint32_t *cur_key = &key_vec[0];

    dhIter it;
    dhKey key;
    PropDBEntry *entry;

    // Copy all keys
    dh_iter_init(&db->hash, &it);
    while(dh_iter_next(&it, &key, (void **)&entry)) {
      *cur_key++ = (uintptr_t)key.data;
    }
  UNLOCK();

  *keys = key_vec;
  return num_keys;
}


static int comp_keys(const void *a, const void *b) {
  char a_buf[64];
  char b_buf[64];

// This will not be efficient, only expedient

  prop_get_name(*(uint32_t *)a, a_buf, sizeof(a_buf));
  prop_get_name(*(uint32_t *)b, b_buf, sizeof(b_buf));

  return strcmp(a_buf, b_buf);
}


void prop_db_sort_keys(PropDB *db, uint32_t *keys, size_t keys_len) {
  qsort(keys, keys_len, sizeof(*keys), comp_keys);
}


void prop_db_dump_keys(PropDB *db, uint32_t *keys, size_t keys_len) {
  printf("Prop DB (%" PRIuz " items):\n", dh_num_items(&db->hash));

  // NOTE: We do not lock the DB to prevent long lockouts while printing.
  // The iterator may end up invalid if the DB is modified.
  for(size_t cur_key = 0; cur_key < keys_len; cur_key++) {
    PropDBEntry entry;
    uint32_t prop = keys[cur_key];
    prop_get(db, prop, &entry);
    prop__print_entry(prop, &entry);
  }
}

