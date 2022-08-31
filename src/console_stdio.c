#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
// Newlib doesn't provide full <sys/termios.h> so only support this on a real *nix
#ifdef __unix__
#  include <termios.h>
#endif

#include "FreeRTOS.h"
#include "semphr.h"

#include "cstone/console.h"
#include "cstone/console_stdio.h"


#ifdef __unix__
static struct termios s_saved_termios = {0};

static void restore_terminal(void) {
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &s_saved_termios);
}
#endif


void configure_posix_terminal(void) {
#ifdef __unix__
// Set stdin to raw mode
// https://viewsourcecode.org/snaptoken/kilo/02.enteringRawMode.html

  tcgetattr(STDIN_FILENO, &s_saved_termios);
  atexit(restore_terminal);

  struct termios raw = s_saved_termios;
  raw.c_lflag &= ~(ECHO | ICANON); // Echo off, Canonical off
  raw.c_iflag &= ~(IXON); // Flow control off
//  raw.c_oflag &= ~(OPOST); // CR/LF translation off
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
#endif
}


// Callback for Console object
static void stdio_send(DualStream *stream) {
  uint8_t *data;
  size_t data_len;

  // Dump tx_queue to stdout
  data_len = isr_queue_peek(stream->tx_queue, &data);
  while(data_len) {
    for(size_t i = 0; i < data_len; i++)
      fputc(data[i], stdout);
    isr_queue_discard(stream->tx_queue, data_len);
    data_len = isr_queue_peek(stream->tx_queue, &data);
  }
}


bool stdio_console_init(ConsoleConfigBasic *cfg) {

  Console *con = console_alloc(cfg);
  if(con) {
    con->stream.io_send = stdio_send;
    con->id = (ConsoleID){.kind = CON_STDIO, .id = 0};
    console_add(con);

    return true;
  }

  return false;
}
