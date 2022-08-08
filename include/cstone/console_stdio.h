#ifndef CONSOLE_STDIO_H
#define CONSOLE_STDIO_H

#ifdef __cplusplus
extern "C" {
#endif

void configure_posix_terminal(void);

bool stdio_console_init(ConsoleConfigBasic *cfg);

#ifdef __cplusplus
}
#endif

#endif // CONSOLE_STDIO_H
