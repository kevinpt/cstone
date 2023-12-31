#ifndef STRING_OPS_H
#define STRING_OPS_H

#ifdef __cplusplus
extern "C" {
#endif

#if defined _POSIX_VERSION
#  include <strings.h>
#  define stricmp(s1, s2)  strcasecmp(s1, s2)

#elif defined _WIN32
#  include <string.h>
#  define stricmp(s1, s2)  _stricmp(s1, s2)

#else
#  define PRIVATE_STRICMP 1
int stricmp(const char *str1, const char *str2);
#endif

bool str_is_bool(const char *value, bool *bool_value);
bool str_to_bool(const char *value);
size_t str_to_upper(char *str);
size_t str_to_lower(char *str);
bool str_ends_with(const char *str, const char *suffix);
int str_split(char *str, const char *delims, char *fields[], int fields_max);

int str_to_fixed(const char *str, unsigned fp_scale, char **endptr);

const char *str_ltrim(const char *str);
unsigned str_break(const char *str, unsigned columns, bool space_only);
void str_print_wrapped(const char *str, unsigned columns, unsigned indent, bool space_only);

#ifdef __cplusplus
}
#endif

#endif // STRING_OPS_H
