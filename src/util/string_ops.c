#include <stdbool.h>
#include <stddef.h>
#include <ctype.h>
#include <string.h>
#include <limits.h>

#include "string_ops.h"

#ifndef COUNT_OF
#  define COUNT_OF(a) (sizeof(a) / sizeof(*(a)))
#endif


/*
Case insensitive string comparison

Args:
  str1: First stirng to compare against
  str2: Second string to compare

Returns:
  0 for match, -1 if str1 compares less than str2, or 1
*/
#ifdef PRIVATE_STRICMP
int stricmp(const char *str1, const char *str2) {
  while(*str1 && (tolower(*str1) == tolower(*str2))) {
    str1++;
    str2++;
  }

  return tolower(*str1) - tolower(*str2);
}
#endif



static const char *s_bool_true[] = {
  "1",
  "y",
  "t",
  "on",
  "yes",
  "true"
};

static const char *s_bool_false[] = {
  "0",
  "n",
  "f",
  "off",
  "no",
  "false"
};

/*
Test if a string is a boolean value

This performs a case insensitive check for predefined strings that
represent true and false values: "1", "y", "t", "on", "yes", "true",
"0", "n", "f", "off", "no", and "false".

Args:
  value:      String to test
  bool_value: Optional boolean value of value arg

Returns:
  true when a value matches any bool string
*/
bool str_is_bool(const char *value, bool *bool_value) {
  for(unsigned i = 0; i < COUNT_OF(s_bool_true); i++) {
    if(!stricmp(s_bool_true[i], value)) {
      if(bool_value)
        *bool_value = true;
      return true;
    }
  };

  for(unsigned i = 0; i < COUNT_OF(s_bool_false); i++) {
    if(!stricmp(s_bool_false[i], value)) {
      if(bool_value)
        *bool_value = false;
      return true;
    }
  };

  if(bool_value)
    *bool_value = false;
  return false;
}

/*
Convert a string to a boolean

This performs a case insensitive check for predefined strings that
represent a true value: "1", "y", "t", "on", "yes", "true".

All other strings treated as false. Use :c:func:`str_is_bool` to test
for a valid bool string.

Args:
  value: String to convert

Returns:
  true when a value matches a true string
*/
bool str_to_bool(const char *value) {
  for(unsigned i = 0; i < COUNT_OF(s_bool_true); i++) {
    if(!stricmp(s_bool_true[i], value))
      return true;
  };

  return false;
}


/*
Convert a string to upper case

Args:
  str: String to convert

Returns:
  Length of str
*/
size_t str_to_upper(char *str) {
  size_t len = 0;
  while(*str != '\0') {
    *str = toupper(*str);
    str++;
    len++;
  }

  return len;
}


/*
Convert a string to lower case

Args:
  str: String to convert

Returns:
  Length of str
*/
size_t str_to_lower(char *str) {
  size_t len = 0;
  while(*str != '\0') {
    *str = tolower(*str);
    str++;
    len++;
  }

  return len;
}


/*
Check if a string ends with a matching suffix

Args:
  str: String to test
  suffix: String to test for in str

Returns:
  True if str ends with suffix
*/
bool str_ends_with(const char *str, const char *suffix) {
  size_t str_len    = strlen(str);
  size_t suffix_len = strlen(suffix);

  if(str_len < suffix_len)
    return false;

  str += str_len - suffix_len;
  return !memcmp(str, suffix, suffix_len);
}


/*
Split a string by delimiters

The input string is destructively modified by inserting NUL chars.

Args:
  str: String to split
  delims: String of delimiter characters to split on
  fields: Array for split result
  fields_max: Number of elemends in fields

Returns:
  Number of fields split
*/
int str_split(char *str, const char *delims, char *fields[], int fields_max) {
  char *save_state;
  int fields_count = 0;

  memset(fields, 0, fields_max * sizeof(char *));

  char *tok = strtok_r(str, delims, &save_state);
  while(tok && fields_count < fields_max) {
    fields[fields_count++] = tok;
    tok = strtok_r(NULL, delims, &save_state);
  }

  return fields_count;
}


/*
Convert a decimal number from string to fixed-point

Args:
  str:      String to convert (Must be NUL terminated)
  fp_scale: Fixed-point scale factor for result
  endptr:   Optional pointer for first invalid character in str

Returns:
  Converted number scaled by fp_scale if found or 0
*/
int str_to_fixed(const char *str, unsigned fp_scale, char **endptr) {
  bool neg = false;
  int int_val = 0;
  int frac_val = 0;
  unsigned b10_scale = 1;
  unsigned digits = 0;

// Set some crude limits. These don't prevent all cases of overflow. They're just basic
// protection against pathological input.
#if INT_MAX >= 0x10000
#  define MAX_DIGITS        9  // log10(2^31)
#  define MAX_FRAC_DIGITS   7
#else // 16-bit int
#  define MAX_DIGITS        4  // log10(2^15)
#  define MAX_FRAC_DIGITS   4
#endif

  // Skip whitespace
  while(*str) {
    if(!isspace(*str))
      break;
    str++;
  }

  // Check for sign
  if(*str == '-') {
    neg = true;
    str++;
  } else if(*str == '+') {
    str++;
  }

  // Get integer part
  while(*str && digits < MAX_DIGITS) {
    char ch = *str;
    if(ch >= '0' && ch <= '9') {
      int_val *= 10;
      int_val += ch - '0';
      digits++;
    } else {
      break;
    }
    str++;
  }

  // Get fraction part
  if(*str == '.') {
    str++;
    digits = 0;
    while(*str && digits < MAX_FRAC_DIGITS) {
      char ch = *str;
      if(ch >= '0' && ch <= '9') {
        frac_val *= 10;
        frac_val += ch - '0';
        digits++;
        b10_scale *= 10;
      } else {
        break;
      }
      str++;
    }
  }

  if(endptr)
    *endptr = (char *)str;

  // Convert to fixed-point
  if(int_val > 0)
    int_val *= fp_scale;
  if(frac_val > 0)
    int_val += (frac_val * fp_scale) / b10_scale;
  if(neg) int_val = -int_val;

  return int_val;
}

