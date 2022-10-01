#ifndef BLOCKING_IO_H
#define BLOCKING_IO_H

#ifndef putnl
#  define putnl()   fputc('\n', stdout)
#endif

#ifdef __cplusplus
extern "C" {
#endif

__attribute__((format(printf, 1, 2)))
int bprintf(const char *fmt, ...);
int bputs(const char *str);
int bfputs( const char *str, FILE *stream);

#ifdef __cplusplus
}
#endif

#endif // BLOCKING_IO_H
