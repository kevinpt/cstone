#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "FreeRTOS.h"
#include "semphr.h"

#include "util/list_ops.h"

#include "cstone/console.h"

/*
I/O configuration
-----------------

All console I/O is channeled through a pair of FIFOs tx_queue and rx_queue within a Console
object. The queue implementation is designed to be safe for independent reader and writer
access even without FreeRTOS running. These queues are also safe to access from an ISR. This
allows us to print output in the early stages of system startup once stdio_init() has run but
the RTOS scheduler hasn't been given control.

On the TX side, we have C stdio plumbed in so that all the standard text output functions
can be used as normal. As an example, from printf(), text is received by the _write() callback
for Newlib in syscalls.c. That then forwards characters to the tx_queue via
console_send() in this module. At this enqueueing stage we perform optional newline translation
so that bare '\n' is replaced with "\r\n" for terminal emulation.

If the queue becomes full we drop any overflow characters. The _write() callback does support
returning the actual bytes written but Newlib will hang waiting for the queue to clear. We want
to be able to print from real-time tasks that can't tolerate such delays so dropping output is
preferable.

There is an API for blocking output that can be used for printing large volumes of text that
would rapidly fill the TX queue. The blocking_io module provides bprintf(), bputs(), and
bfputs() clones that will block until the TX queue empties out before attempting to print.
These should only be used from tasks without rigorous timing requirements. There is a
blocking_stdout() function that can be used to make the normal stdout functions (printf(),
puts(), etc.) block in the same manner. Don't enable this unless you are certain universal
blocking output will be safe.

For the RX side we don't use stdio interop since scanf() and friends aren't very useful.
Instead we have the console_shell driver process characters directly from rx_queue.
Characters are inserted into the RX queue by the Console backend I/O driver.

*/


static Console *s_consoles = NULL;
static Console *s_default_console = NULL;


// ******************** Console management ********************

void console_add(Console *con) {
  ll_slist_push(&s_consoles, con);
  if(!s_default_console)
    s_default_console = con;
}


bool console_remove(Console *con) {
  if(con == s_default_console)
    return false;

  return ll_slist_remove(&s_consoles, con);
}


bool console_set_default(Console *con) {
  if(!ll_slist_find(s_consoles, con))
    return false;

  s_default_console = con;
  return true;
}


#define THREAD_STORE_CONSOLE  0

Console *active_console(void) {
  if(xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
    Console *con = pvTaskGetThreadLocalStoragePointer(NULL, THREAD_STORE_CONSOLE);
    if(con)
      return con;
  }

  return s_default_console;
}


Console *first_console(void) {
  return s_consoles;
}


void task_set_console(TaskHandle_t task, Console *con) {
//  if(xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED)
    vTaskSetThreadLocalStoragePointer(task, THREAD_STORE_CONSOLE, con);
}


Console *console_find(ConsoleID id) {
  Console *cur = s_consoles;

  while(cur) {
    if((cur->id.kind == id.kind) && (id.id < 0 || cur->id.id == id.id))
      return cur;

    cur = cur->next;
  }

  return NULL;
}


#ifdef PLATFORM_EMBEDDED
#  define IN_ISR()  xPortIsInsideInterrupt()
#else
#  define IN_ISR()  0
#endif

size_t console_rx_unqueue(Console *con, uint8_t *data, size_t len) {
  // Single writer (ISR) is safe
  // Multiple readers (tasks) needs lock
  bool have_lock = false;

  if(con->rx_lock && !IN_ISR()) {
    xSemaphoreTake(con->rx_lock, portMAX_DELAY);
    have_lock = true;
  }

  // Copy data from RX queue
  size_t num_read = isr_queue_pop(con->rx_queue, (uint8_t *)data, len);

  if(have_lock)
    xSemaphoreGive(con->rx_lock);

  return num_read;
}


// Add data to tx_queue with bare NL expanded to CRLF
static size_t console__tx_nl_xlate(Console *con, uint8_t *data, size_t len) {
  size_t copied = 0;

  while(len && !isr_queue_is_full(con->tx_queue)) {
    uint8_t ch = *data;

    if(!con->prev_cr && !con->injected_cr && ch == '\n') { // Insert CR before NL
      ch = '\r';
      isr_queue_push_one(con->tx_queue, &ch);
      con->injected_cr = true;

    } else {  // Normal char
      isr_queue_push_one(con->tx_queue, &ch);
      data++;
      len--;
      copied++;
      con->injected_cr = false;
      con->prev_cr = (ch == '\r');
    }
  }

  return copied;
}



size_t console_send(Console *con, uint8_t *data, size_t len) {
  size_t copied = 0;

  if(!con->io_send)
    return 0;

  if(con->blocking_stdout) // Wait until FIFO is empty
    xSemaphoreTake(con->tx_empty, portMAX_DELAY);


  // Single reader (ISR) is safe
  // Multiple writers (tasks) needs lock
  bool have_lock = false;
  if(con->tx_lock && !IN_ISR()) {
    xSemaphoreTake(con->tx_lock, portMAX_DELAY);
    have_lock = true;
  }

  // Copy data to queue with optional conversions for bare newlines
  if(con->nl_xlat_off)
    copied = isr_queue_push(con->tx_queue, data, len);
  else  // NL --> CRLF
    copied = console__tx_nl_xlate(con, data, len);

  if(have_lock)
    xSemaphoreGive(con->tx_lock);

  con->io_send(con);

  return copied;
}


bool console_blocking_stdout(Console *con, bool mode) {
  bool old_mode = con->blocking_stdout;
  con->blocking_stdout = mode;
  return old_mode;
}


// Expand the console kind X-macro into switch body
#define CON_KIND_CASE(K, V)  case CON_##K: return #K; break;

/*
Translate an console kind into a string

Args:
  kind: enum value

Returns:
  The corresponding string or "<unknown>"
*/
const char *console_kind(enum ConsoleKind kind) {
  switch(kind) {
    CON_KIND_LIST(CON_KIND_CASE);
    default: break;
  }

  return "<unknown>";
}

void console_name(Console *con, char *name, size_t name_len) {
  snprintf(name, name_len, "%s%d", console_kind(con->id.kind), con->id.id);
}



// ******************** Console creation ********************

void console_init(Console *con, ConsoleConfigFull *cfg) {
  memset(con, 0, sizeof(*con));

  con->tx_queue     = cfg->tx_queue;
  con->rx_queue     = cfg->rx_queue;
  con->io_send      = cfg->io_send;
  con->io_ctx       = cfg->io_ctx;

  con->tx_lock = xSemaphoreCreateBinary();
  xSemaphoreGive(con->tx_lock);

  con->rx_lock = xSemaphoreCreateBinary();
  xSemaphoreGive(con->rx_lock);

  con->tx_empty = xSemaphoreCreateBinary();
  xSemaphoreGive(con->tx_empty);

  con->term_size = (TerminalSize){80, 25};

  shell_init(&con->shell, cfg);
  console_query_terminal_size(con);
}




// Default console prompt

__attribute__((weak))
uint8_t show_prompt(void *eval_ctx) {
  fputs("> ", stdout);
  return 2; // Inform console driver of cursor position in line (0-based)
}


Console *console_alloc(ConsoleConfigBasic *cfg) {

  char *line_buf = NULL;
#ifdef USE_CONSOLE_HISTORY
  char *hist_buf = NULL;
#endif
  Console *con = NULL;
  IsrQueue *tx_queue = NULL;
  IsrQueue *rx_queue = NULL;
  ConsoleConfigFull full_cfg;

  line_buf = (char *)malloc(cfg->line_buf_size * sizeof(char));
  if(!line_buf)
    return NULL;

#ifdef USE_CONSOLE_HISTORY
  if(cfg->hist_buf_size > 0) {
    hist_buf = (char *)malloc(cfg->hist_buf_size * sizeof(char));
    if(!hist_buf)
      goto cleanup;
  }
#endif

  con = (Console *)malloc(sizeof(Console));

  tx_queue = isr_queue_alloc(cfg->tx_queue_size, /*overwrite*/false);
  rx_queue = isr_queue_alloc(cfg->rx_queue_size, /*overwrite*/false);

  if(!con || !tx_queue || !rx_queue)
    goto cleanup;


  full_cfg = (ConsoleConfigFull){
    .line_buf       = line_buf,
    .line_buf_size  = cfg->line_buf_size,
#ifdef USE_CONSOLE_HISTORY
    .con_hist_buf   = hist_buf,
    .con_hist_size  = cfg->hist_buf_size,
#endif
    .cmd_suite      = cfg->cmd_suite,
    .show_prompt    = show_prompt,
    .eval_ctx       = NULL,
    .tx_queue       = tx_queue,
    .rx_queue       = rx_queue,
    .io_send        = NULL,
    .io_ctx         = NULL
  };

  console_init(con, &full_cfg);

  return con;

cleanup:
  if(line_buf)  free(line_buf);
#ifdef USE_CONSOLE_HISTORY
  if(hist_buf)  free(hist_buf);
#endif
  if(con)       free(con);
  if(tx_queue)  free(tx_queue);
  if(rx_queue)  free(rx_queue);
  return NULL;
}



int console_printf(Console *con, const char *fmt, ...) {
  char buf[64];

  va_list args;
  va_start(args, fmt);

  int rval = vsnprintf(buf, sizeof buf, fmt, args);
  va_end(args);

  size_t send_bytes = ((size_t)rval >= sizeof buf) ? (sizeof buf) - 1 : (size_t)rval;
  console_send(con, (uint8_t *)buf, send_bytes);

  return rval;
}


static void console_cursor_move(Console *con, short col, short row) {
  if(col < 1) col = 1;
  if(row < 1) row = 1;

  console_printf(con, "\33[%d;%dH", row, col);
}


void console_query_cursor_pos(Console *con) {
  console_printf(con, "\33[6n");   // Request DSR
}


#define VT100_CURS_SAVE     "\33" "7"
#define VT100_CURS_RESTORE  "\33" "8"

void console_query_terminal_size(Console *con) {
  // Save cursor position
  console_printf(con, VT100_CURS_SAVE);

  // Move cursor to bottom right
  console_cursor_move(con, 999, 999);

  con->size_query = 1;
  console_query_cursor_pos(con);

  // Restore cursor position
  console_printf(con, VT100_CURS_RESTORE);

  // Response processed asynchronously by shell_process_rx()
}

