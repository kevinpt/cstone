
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "cstone/prop_id.h"
#include "cstone/prop_flags.h"
#include "cstone/blocking_io.h"


#ifndef COUNT_OF
#  define COUNT_OF(a) (sizeof(a) / sizeof(*(a)))
#endif

#define FLAG_MASK   (~P4_MSK)


/*
Initialize a property flag set

The prefix parameter must have a P3 array field so that P4
serves as a flag index.

Args:
  pf:               Property flags to init
  prefix:           Property these flags fall under within the ID code space
  index_names:      Optional array of named flag indices to print
  index_names_len:  Length of index_names array
  flag_values:      Optional array of initial encoded flag values from :c:func:`prop_flags_get_all`

Returns:
  true if initialization is a success
*/
bool prop_flags_init(PropFlags *pf, uint32_t prefix, const char **index_names,
                      size_t index_names_len, const uint8_t flag_values[32]) {
  memset(pf, 0, sizeof(*pf));

  if(!PROP_FIELD_IS_ARRAY(PROP_FIELD(prefix, 3)))  // P3 must be an array
    return false;

  pf->prefix = prefix & FLAG_MASK;

  if(index_names) {
    pf->index_names = index_names;
    pf->index_names_len = index_names_len;

    // Get longest flag name
    for(unsigned i = 0; i < index_names_len; i++) {
      int name_len = strlen(index_names[i]);
      if(name_len > pf->max_name_len)  pf->max_name_len = name_len;
    }
  }

  if(flag_values)
    memcpy(pf->flags, flag_values, sizeof(pf->flags));

  return true;
}


/*
Retrieve array of all current flag values

Args:
  pf:           Property flags
  flag_values:  Array to populate with flag values
*/
void prop_flags_get_all(PropFlags *pf, uint8_t flag_values[32]) {
  memcpy(flag_values, pf->flags, sizeof(pf->flags));
}


static bool valid_flag_prop(PropFlags *pf, uint32_t prop) {
  if(PROP_FIELD_IS_ARRAY(PROP_FIELD(prop, 3)) && // P3 must be an array
    (prop & FLAG_MASK) == pf->prefix)  // Prefix must match
    return true;

  return false;
}


/*
Set a flag value

Args:
  pf:     Property flags
  prop:   Property flag to set
  value:  New value for the flag

Returns:
  true if the flag is valid for the flag set
*/
bool prop_flags_set(PropFlags *pf, uint32_t prop, bool value) {
  if(!valid_flag_prop(pf, prop))
    return false;

  unsigned index = PROP_GET_INDEX(prop, 3);
  uint8_t byte_mask = 1u << (index % 8);
  index /= 8;
  uint8_t byte = pf->flags[index];
  if(value) // Set bit
    byte |= byte_mask;
  else      // Clear bit
    byte &= ~byte_mask;

  pf->flags[index] = byte;

  return true;
}


/*
Set a flag value using its name

Args:
  pf:         Property flags
  flag_name:  Name of the flag to set
  value:      New value for the flag

Returns:
  true if the flag is valid for the flag set
*/
bool prop_flags_set_by_name(PropFlags *pf, const char *flag_name, bool value) {
  int flag = prop_flags_lookup_name(pf, flag_name);
  if(flag < 0)
    return false;

  uint32_t prop = pf->prefix | P3_ARR((uint8_t)flag);
  return prop_flags_set(pf, prop, value);
}


/*
Get a flag value

Args:
  pf:     Property flags
  prop:   Property flag to get

Returns:
  The flag value or false if the property is invalid
*/
bool prop_flags_get(PropFlags *pf, uint32_t prop) {
  if(!valid_flag_prop(pf, prop))
    return false;

  unsigned index = PROP_GET_INDEX(prop, 3);
  uint8_t byte_mask = 1u << (index % 8);
  index /= 8;
  uint8_t byte = pf->flags[index];

  return byte & byte_mask;
}


int prop_flags_lookup_name(PropFlags *pf, const char *flag_name) {
  if(pf->index_names) {
    for(size_t i = 0; i < pf->index_names_len; i++) {
      if(!strcmp(flag_name, pf->index_names[i]))
        return (int)i;
    }
  }

  return -1;
}

const char *prop_flags_lookup_index(PropFlags *pf, uint8_t index) {
  if(!pf->index_names || index >= pf->index_names_len)
    return NULL;

  return pf->index_names[index];
}


/*
Print the values for a property flag set

Args:
  pf:               Property flags
  set_flags_only:   Only print flags that are true
  max_flag:         Upper limit for flags to print out; 0 == All flags
*/
void prop_flags_dump(PropFlags *pf, bool set_flags_only, uint8_t max_flag) {
  char name[40];
  prop_get_name(pf->prefix, name, sizeof(name));

  char *pos = strrchr(name, '[');
  if(pos) *pos = '\0'; // Strip trailing index for P4

  printf("Flags for %s:\n", name);

  if(max_flag == 0)
    max_flag = 255;

  for(unsigned i = 0; i < COUNT_OF(pf->flags); i++) {
    for(unsigned m = 0; m < 8; m++) {
      uint8_t byte_mask = 1u << m;
      unsigned index = i*8 + m;
      if(index > (unsigned)max_flag)
        return;

      uint8_t flag = pf->flags[i] & byte_mask;
      uint32_t prop = pf->prefix | P3_ARR(index);
      if(flag || !set_flags_only) {
        if(pf->index_names && index < pf->index_names_len)
          bprintf("  " PROP_ID " '%-*s' = %d\n", prop, pf->max_name_len, pf->index_names[index], flag ? 1 : 0);
        else
          bprintf("  " PROP_ID " = %d\n", prop, flag ? 1 : 0);
      }
    }
  }
}


#ifdef TEST_PROP_FLAGS

#define P_APP_STORAGE_LOCAL (P1_APP | P2_STORAGE | P3_LOCAL | P3_ARR(0))

#define TEST_FLAGS(M) \
  M(FLAG0, 0) \
  M(F1,    1) \
  M(FLAG2, 2) \
  M(FLAG3, 3) \
  M(FLAG4, 4) \
  M(FLAG5, 5) \
  M(FLAG6, 6) \
  M(FLAG7, 7)

#define FLAG_ENUM_ITEM(name, ix)  PF_APP_STORAGE_LOCAL_##name = P_APP_STORAGE_LOCAL | P3_ARR(ix),
enum TestFlags {
  TEST_FLAGS(FLAG_ENUM_ITEM)
};

#define FLAG_NAME_ITEM(name, ix)  #name,
static const char *s_flag_names[] = {
  TEST_FLAGS(FLAG_NAME_ITEM)
};

int main(void) {
  PropFlags pf;


  prop_init();

  uint8_t init_values[32] = {[0]=0xF0, [2]=0xF0};

  prop_flags_init(&pf, P_APP_STORAGE_LOCAL, s_flag_names, COUNT_OF(s_flag_names), init_values);

  prop_flags_set(&pf, PF_APP_STORAGE_LOCAL_FLAG0, 1);
  prop_flags_set(&pf, PF_APP_STORAGE_LOCAL_F1, 1);
  prop_flags_set(&pf, P_APP_STORAGE_LOCAL | P3_ARR(7), 1);
  prop_flags_set(&pf, P_APP_STORAGE_LOCAL | P3_ARR(8), 1);
  prop_flags_set(&pf, P_APP_STORAGE_LOCAL | P3_ARR(16), 1);
  prop_flags_set(&pf, P_APP_STORAGE_LOCAL | P3_ARR(254), 1);

  prop_flags_get_all(&pf, init_values);

  for(int i = 0; i < 10; i++) {
    printf("Flag %d = %d\n", i, prop_flags_get(&pf, P_APP_STORAGE_LOCAL | P3_ARR(i)));
  }

  prop_flags_dump(&pf, true, 30);

  printf("Index %d = '%s' = %d\n", 0, prop_flags_lookup_index(&pf, 0),
    prop_flags_lookup_name(&pf, prop_flags_lookup_index(&pf, 0)));

  return 0;
}
#endif
