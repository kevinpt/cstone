#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#include "cstone/platform.h"
#ifdef PLATFORM_HAS_ATOMICS
#  include <stdatomic.h>
#else
#  include "locking.h"
#endif

#include "util/range_strings.h"
#include "util/search.h"
#include "util/dhash.h"
#include "util/string_ops.h"
#include "util/list_ops.h"

#include "cstone/prop_id.h"



#ifndef COUNT_OF
#  define COUNT_OF(a) (sizeof(a) / sizeof(*(a)))
#endif


#ifdef USE_PROP_ID_FIELD_NAMES
// Create a table cross referencing field values with their string name
// The names are prefixed with "Pn" to ensure names duplicated at different
// levels stay unique when hashed.


// NOTE: This is sorted by qsort() so can't be const
static PropFieldDef s_prop_fields[] = {
  PROP_LIST(PROP_FIELD_DEF)
};

#else
static PropFieldDef s_prop_fields[1] = {0};

#endif // USE_PROP_ID_FIELD_NAMES

static PropNamespace s_global_prop_namespace = {
  .prop_defs = s_prop_fields,
  .prop_defs_len = COUNT_OF(s_prop_fields)
};

static PropNamespace *s_prop_namespaces = &s_global_prop_namespace;


static void prop_init_namespace(PropNamespace *ns);


/*
Add a new namespace to manage property lookups

Args:
  ns:   New namespace object to add
*/
void prop_add_namespace(PropNamespace *ns) {
  // Search for position in namespace list where mask matches that of new namespace
  PropNamespace *cur_ns = s_prop_namespaces;
  while(cur_ns) {
    if(cur_ns->mask <= ns->mask)
      break;
    cur_ns = cur_ns->next;
  }

  if(cur_ns) {
    prop_init_namespace(ns);
    ll_slist_add_before(&s_prop_namespaces, cur_ns, ns);
  }
}


#ifdef USE_PROP_ID_FIELD_NAMES
static PropNamespace *prop__get_namespace(int level, uint32_t prop) {
  PropNamespace *cur_ns = s_prop_namespaces;
  while(cur_ns) {
    if((cur_ns->prefix == 0) || 
        ((prop & cur_ns->mask) == cur_ns->prefix && (PROP_MASK(level) & cur_ns->mask) == 0))
      return cur_ns;
    cur_ns = cur_ns->next;
  }

  return NULL;
}
#endif



#ifdef USE_PROP_ID_REV_HASH

// Build a hash table to do reverse lookups from string names to field values

static void index_item_destroy(dhKey key, void *value, void *ctx) {
  (void)key;
  (void)value;
  (void)ctx;
}


static bool index_equal_hash_keys(dhKey key1, dhKey key2, void *ctx) {
  (void)ctx;
  return !stricmp(key1.data, key2.data);
}

static void prop__index_namespace(PropNamespace *ns) {
  dhConfig hash_cfg = {
                                          // Extra 10% to avoid hash growing
    .init_buckets = ns->prop_defs_len + (ns->prop_defs_len / 10),
    .value_size   = sizeof(PropFieldDef *), // Point into prop_defs[]
    .destroy_item = index_item_destroy,
    .gen_hash     = dh_gen_hash_string_no_case,
    .is_equal     = index_equal_hash_keys
  };

  dh_init(&ns->name_index, &hash_cfg, NULL);

  // Build hash of field names
  for(unsigned int i = 0; i < ns->prop_defs_len; i++) {
    dhKey key = {
      .data = ns->prop_defs[i].name,
      .length = strlen(ns->prop_defs[i].name)
    };

    // Using a pointer as the value since we already have static storage
    PropFieldDef *def = &ns->prop_defs[i];
    dh_insert(&ns->name_index, key, &def);
  }
}


// Lookup a field value from its string name
static uint32_t prop__get_field_id(int level, uint32_t prefix, char *field) {
  PropNamespace *ns = prop__get_namespace(level, prefix);

  dhKey key = {
    .data = field,
    .length = strlen(field)
  };

  PropFieldDef *value;

  if(dh_lookup(&ns->name_index, key, &value)) {
//    printf("## LOOKUP '%s' %u = %08lX\n", field, key.length, value->field);
    return value->field;
  } else if(ns != &s_global_prop_namespace) {
    ns = &s_global_prop_namespace;
    if(dh_lookup(&ns->name_index, key, &value))
      return value->field;
  }
  return 0;
}

#endif // USE_PROP_ID_REV_HASH



#ifdef USE_PROP_ID_FIELD_NAMES
static int prop_sort_cmp(const void *a, const void *b) {
  uint32_t aa = ((PropFieldDef *)a)->field;
  uint32_t bb = ((PropFieldDef *)b)->field;

  return aa < bb ? -1 : (aa > bb ? 1 : 0);
}
#endif

static void prop_init_namespace(PropNamespace *ns) {
#ifdef USE_PROP_ID_FIELD_NAMES
  // Sort prop definitions so we can use binary search in find_field_def()
  qsort(ns->prop_defs, ns->prop_defs_len, sizeof(PropFieldDef), prop_sort_cmp);
#endif

#ifdef USE_PROP_ID_REV_HASH
  prop__index_namespace(ns);
#endif

  // Define mask if missing
  if(ns->mask == 0 && ns->prefix != 0) {
    ns->mask = PROP_GET_MASK(ns->prefix);
    ns->prefix = ns->prefix & ns->mask;
  }
}


/*
Initialize property system

*/
void prop_init(void) {
  prop_init_namespace(&s_global_prop_namespace);
}



#ifdef USE_PROP_ID_FIELD_NAMES
static inline int field_cmp(const void *key, const void *item) {
  uint32_t k = *(uint32_t *)key;
  uint32_t i = ((PropFieldDef *)item)->field;

  return k < i ? -1 : k > i ? 1 : 0;
}

// Binary search with inlined comparison function
static inline GEN_BSEARCH_FUNC(prop_bsearch_fast, field_cmp)

// Lookup a field definition from its numeric value
static inline PropFieldDef *find_field_def(PropNamespace *ns, uint32_t field) {
  PropFieldDef *def = prop_bsearch_fast(&field, ns->prop_defs,
                                          ns->prop_defs_len, sizeof(PropFieldDef));

  return def;
}
#endif // USE_PROP_ID_FIELD_NAMES

// Append a field name to a string
static bool append_field(AppendRange *rng, int level, uint32_t prop) {
  bool status = true;
  int copied;

  bool prev_array = PROP_HAS_ARRAY(prop & PROP_MASK(level-1));

  uint32_t field = prop & PROP_MASK(level);
  bool field_array = PROP_HAS_ARRAY(field);
  if(field_array)
    field = PROP_FROM_ARRAY(field);

#ifdef USE_PROP_ID_FIELD_NAMES
  PropNamespace *ns = prop__get_namespace(level, prop);
  PropFieldDef *def = find_field_def(ns, field);
  if(!def && ns != &s_global_prop_namespace)
    def = find_field_def(&s_global_prop_namespace, field);
#endif

//  printf("## APPEND: %08lX  %p\n", field, def);

  if(prev_array) {  // This is an index
    field >>= 8*(4-level);
    copied = range_cat_fmt(rng, "%"PRIu32"]", field);
    status = copied < 0 || !status ? false : true;

    if(level != 4) {
      copied = range_cat_char(rng, '.');
      status = copied < 0 || !status ? false : true;
    }

  } else {  // This is a field
#ifdef USE_PROP_ID_FIELD_NAMES
    // Add field name if known
    if(def) {
      copied = range_cat_str(rng, &def->name[2]);
      status = copied < 0 || !status ? false : true;
    } else
#endif
    { // Unknown name
      copied = range_cat_fmt(rng, "<%"PRIu32">", PROP_FIELD(field, level));
      status = copied < 0 || !status ? false : true;
    }

    // Add separator
    if(field_array) {
      copied = range_cat_char(rng, '[');
      status = copied < 0 || !status ? false : true;
    } else if(level != 4) {
      copied = range_cat_char(rng, '.');
      status = copied < 0 || !status ? false : true;
    }
  }

  return status;
}


/*
Convert a property value to its string representation

This requires USE_PROP_ID_FIELD_NAMES to be defined.

Args:
  prop:     Property to convert
  buf:      Destination for property name
  buf_size: Size of buf

Returns:
  Constructed property name in buf
*/
char *prop_get_name(uint32_t prop, char *buf, size_t buf_size) {
  AppendRange buf_rng;
  bool status;

  range_init(&buf_rng, buf, buf_size);

  for(int i = 1; i <= 4; i++) {
    status = append_field(&buf_rng, i, prop);
    if(!status) goto overflow;
  }

  return buf;

overflow:
  range_cat_str(&buf_rng, "..."); // Attempt to add ellipsis
  return buf;
}


/*
Convert a property in string ID form to its numeric representation

ID strings must be of the form "P[0-9A-Z]{8,8}"

Args:
  id: Property id to convert

Returns:
  Converted property value on success; 0 on failure
*/
uint32_t prop_parse_id(const char *id) {
  char *ch;
  uint32_t prop = 0;

  // Match a string of the form "P[0-9A-Z]{8,8}"
  if(id[0] == 'P' || id[0] == 'p') {
    prop = strtoul(&id[1], &ch, 16);

    if((ch - id) != 9)
      prop = 0;
  }

  return prop;
}


/*
Convert a property name to its numeric representation

This requires USE_PROP_ID_REV_HASH to be defined.

Args:
  name: Property name to parse

Returns:
  Parsed property value on success; 0 on failure
*/
uint32_t prop_parse_name(const char *name) {

  int level = 1;
  StringRange tok;
  StringRange arr_tok;
  size_t limit;

  char buf[26];
  const char *str = name;
  uint32_t prop = 0;
  uint32_t field;
  bool is_array = false;

  while(range_token(str, ".", &tok)) {
    str = NULL; // Continue parsing after tok on following iterations

    // Add prefix so we can do reverse lookup from field names
    snprintf(buf, sizeof(buf), "P%d%.*s", level, RANGE_FMT(&tok));
//    printf("## FIELD: %s\n", buf);
    field = 0;
    is_array = false;

    // Check if array
    if(strchr(buf, '[')) {
      // Get name before index brackets
      limit = range_size(&tok);
      range_token_limit(tok.start, "[]", &arr_tok, &limit);

      if(*arr_tok.start == '<' && *(arr_tok.end-1) == '>') { // Unknown field name
        field = strtoul(arr_tok.start+1, NULL, 10);
        if(field > 0 && field < 127)
          field = (field | 0x80ul) << (4-level)*8;
        else // Bad array field value
          return 0;

      } else { // Reverse name lookup
        snprintf(buf, sizeof(buf), "P%d%.*s", level, RANGE_FMT(&arr_tok));
      }

      is_array = true;
//      printf("## ARRAY %d\n", level);

    } else if(*tok.start == '<' && *(tok.end-1) == '>') { // Unknown field name
      field = strtoul(tok.start+1, NULL, 10);
      if(field >= 255) // Bad field value
        return 0;
      field <<= (4-level)*8;
//      printf("## UNK <%lu>\n", field);
    }

#ifdef USE_PROP_ID_REV_HASH
    if(field == 0)  // Lookup field value from hashed index
      field = prop__get_field_id(level, prop, buf);
#endif

    if(field == 0)
      return 0;

    if(is_array)
      field |= 0x80ul << (4-level)*8;

    prop |= field;

    if(is_array) { // Next array token is an index
      range_token_limit(NULL, "[]", &arr_tok, &limit);
      uint32_t index = strtoul(arr_tok.start, NULL, 10);
      if(index > 254)
        return 0;
//      printf("## INDEX: %lu  %d\n", index, level);
      prop = PROP_SET_INDEX(prop, level, index);
      level++;
    }


    level++;
    if(level > 5) // Too many levels; bail out
      break;
  }

  if(level != 5)
    prop = 0;

  return prop;
}


/*
Convert a property name or string ID to its numeric representation

This requires USE_PROP_ID_REV_HASH to be defined.
ID strings must be of the form "P[0-9A-Z]{8,8}"

Args:
  name: Property name or ID to parse

Returns:
  Parsed property value on success; 0 on failure
*/
uint32_t prop_parse_any(const char *id_name) {
  uint32_t prop = prop_parse_id(id_name);

  // Otherwise, check if it's a full name
  if(prop == 0)
    prop = prop_parse_name(id_name);

  return prop;
}


/*
Check a property for valid encoding

Args:
  prop:       Property to check
  allow_mask: Permit mask fields (0xFF) to validate

Returns:
  true for valid properties
*/
bool prop_is_valid(uint32_t prop, bool allow_mask) {
  bool prev_array = false;

  for(unsigned level = 1; level <= 4; level++) {
    uint8_t field = PROP_FIELD(prop, level);

    if(prev_array) { // Array index
      // Indices range from 0 - 254
      if(field == 0xFF && !allow_mask) return false; // Mask

      prev_array = false;

    } else if(field & 0x80ul) { // This is an array field or a mask
      if(field == 0xFF) { // Mask
        if(!allow_mask) return false;
        prev_array = false;

      } else { // Array
        field &= 0x7F;
        if(field == 0) return false; // Fields range from 1 to 126; 0 and 127 prohibited
        prev_array = true;
      }

    } else { // Normal field
      if(field == 0 || field == 0x7F) return false; // Fields range from 1 to 126; 0 and 127 prohibited
      prev_array = false;
    }
  }

  return true;
}


/*
Check if a property has a mask field

Args:
  prop:       Property to check

Returns:
  true for property with a mask
*/
bool prop_has_mask(uint32_t prop) {
  for(uint32_t mask = 0xFF000000ul; mask; mask >>= 8) {
    if((prop & mask) == mask) return true;
  }
  return false;
}


/*
Check if a property value matches a masked property

Mask fields in masked_prop are don't care values that don't have to match prop.
This allows you to match a property prefix.

Args:
  prop:         Property to check
  masked_prop:  Property with mask to compare against

Returns:
  true for valid properties
*/
bool prop_match(uint32_t prop, uint32_t masked_prop) {
  uint32_t mask = PROP_GET_MASK(masked_prop);

  return (prop & mask) == (masked_prop & mask);
}


/*
Generate a new 24-bit ID in the P1_AUX_24 field space.

Returns:
  A new sequential ID
*/
uint32_t prop_new_global_id(void) {

#ifdef PLATFORM_HAS_ATOMICS
  static volatile atomic_uint_least32_t next_id = 1;
  uint32_t id = atomic_fetch_add(&next_id, 1);

#else
  static volatile uint32_t next_id = 1;
  ENTER_CRITICAL();
    uint32_t id = next_id++;
  EXIT_CRITICAL();
#endif

  return PROP_AUX_24(id);
}



////////////////////////////////////////////////////////////////////////

#ifdef TEST_MAIN

#define P_SYS_HW_INFO_VERSION   (P1_SYS | P2_HW | P3_INFO | P4_VERSION)
#define P_NET_IPV4_SUBNET_MASK  (P1_NET | P2_IPV4 | P3_SUBNET | P4_MASK)
#define P_NET_IPV4_DOMAIN_NAME  (P1_NET | P2_IPV4 | P3_DOMAIN | P4_NAME)

static const PropDefaultDef s_prop_defaults[] = {
  P_UINT(P_SYS_HW_INFO_VERSION,   42,         P_READONLY),
  P_UINT(P_NET_IPV4_SUBNET_MASK,  0xFFFFFF00, P_PERSIST),
  P_STR(P_NET_IPV4_DOMAIN_NAME, "localhost", P_PERSIST),
  P_END_DEFAULTS
};


int main(int argc, char *argv[]) {
  uint32_t prop = P_SYS_HW_INFO_VERSION;
  char p_name[64];

  prop_init();

  printf("## P4_DELAY: %08X\n", P4_DELAY);

  prop_get_name(prop, p_name, sizeof(p_name));
  printf("Name: P%08X = %s\n", prop, p_name);

  prop = (P1_SYS | P2_HW | P2_ARR(1) | P4_NAME);
  prop_get_name(prop, p_name, sizeof(p_name));
  printf("Name: P%08X = %s\n", prop, p_name);

  prop = (P1_SYS|P1_ARR(10) | P3_INFO | P3_ARR(20));
  prop_get_name(prop, p_name, sizeof(p_name));
  printf("Name: P%08X = %s\n", prop, p_name);


  prop = (P1_SYS | P2_HW | 0x7000 | P4_NAME);
  prop_get_name(prop, p_name, sizeof(p_name));
  printf("Name: P%08X = %s\n", prop, p_name);


  prop = (P1_SYS | P2_HW | P3_MSK | P4_NAME);
  printf("Masked: P%08X  M%08X\n", prop, PROP_GET_MASK(prop));


  PropDB db;

  prop_db_init(&db, 32, 0);
  prop_db_set_defaults(&db, s_prop_defaults);



  PropDBEntry entry = {
    .value = 42,
    .kind = 1
  };

  PropDBEntry prop_value;

/*
  prop_set(&db, 100, &entry);

  entry.value = 1000;
  entry.kind = 24;
  prop_set(&db, 200, &entry);

  entry.value = 3000;
  entry.kind = 3;
  prop_set(&db, 300, &entry);

  prop_get(&db, 200, &prop_value);
  printf("PROP: 200 = %u  kind = %d\n", prop_value.value, prop_value.kind);

  entry.value = 4000;
  prop_set(&db, 100, &entry);
*/

  uint32_t id = prop__get_field_id(1, 0, "P1APP");
  printf("APP = %08X\n", id);

  printf("ID: %08X\n", prop_parse_name("NET[15].<63>.MASK"));

  prop = P_NET_IPV4_SUBNET_MASK;
  prop_get(&db, prop, &prop_value);
  prop_get_name(prop, p_name, sizeof(p_name));
  printf("P%08X  %s (%c) = %08X\n", prop, p_name, prop_value.readonly ? 'R' : 'W',
    prop_value.value);

  prop_print(&db, P_NET_IPV4_SUBNET_MASK);


  prop = P_SYS_HW_INFO_VERSION;
  prop_get(&db, prop, &prop_value);
  prop_get_name(prop, p_name, sizeof(p_name));
  printf("P%08X  %s (%c) = %08X\n", prop, p_name, prop_value.readonly ? 'R' : 'W',
    prop_value.value);


  memset(&entry, 0, sizeof(entry));
  entry.value = (uintptr_t)"foobar";
  entry.kind = P_KIND_STRING;
  prop_set(&db, P_NET_IPV4_DOMAIN_NAME, &entry);
  prop_set_str(&db, P_NET_IPV4_DOMAIN_NAME, "xxyy");


  prop = P_NET_IPV4_DOMAIN_NAME;
  prop_get(&db, prop, &prop_value);
  prop_get_name(prop, p_name, sizeof(p_name));
  printf("P%08X  %s (%c) = %s\n", prop, p_name, prop_value.readonly ? 'R' : 'W',
    (char *)prop_value.value);


  prop_print(&db, P_NET_IPV4_DOMAIN_NAME);


  prop_db_free(&db);

  return 0;
}

#endif
