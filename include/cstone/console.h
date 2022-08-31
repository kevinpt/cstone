#ifndef CONSOLE_H
#define CONSOLE_H

#include "cstone/isr_queue.h"
#include "cstone/console_shell.h"
#include "cstone/dual_stream.h"



// Basic configuration struct for console_alloc() and helpers
typedef struct ConsoleConfigBasic {
  size_t              tx_queue_size;
  size_t              rx_queue_size;
  LineIndex           line_buf_size;
#ifdef USE_CONSOLE_HISTORY
  size_t              hist_buf_size;
#endif

  ConsoleCommandSuite *cmd_suite;
} ConsoleConfigBasic;


// Full configuration struct for console_init()
// This permits static allocation of all internal objects if needed
typedef struct ConsoleConfigFull {
  char               *line_buf;
  LineIndex           line_buf_size;

#ifdef USE_CONSOLE_HISTORY
  char               *con_hist_buf;
  size_t              con_hist_size;
#endif

  ConsoleCommandSuite *cmd_suite;
  ConsolePrompt       show_prompt;
  void               *eval_ctx;

  DualStreamConfig  stream;

} ConsoleConfigFull;


// ******************** ConsoleID ********************
// The following use X-macro expansion to enable string names
// in console_kind().

#define CON_KIND_LIST(M) \
  M(INVALID,  0) \
  M(UART,     1) \
  M(USB,      2) \
  M(STDIO,    3)

#define CON_ENUM_ITEM(K, V) CON_##K = V,

enum ConsoleKind {
  CON_KIND_LIST(CON_ENUM_ITEM)
};

typedef struct {
  enum ConsoleKind kind;
  int id;
} ConsoleID;


typedef struct {
  short cols;
  short rows;
} TerminalSize;

struct Console {
  struct Console     *next;
  ConsoleID           id;

  DualStream          stream;

  // Flags
  unsigned            blocking_stdout: 1;  // Force all prints to block
  unsigned            nl_xlat_off:     1;  // Disable NL translation

  // Newline translation state:
  unsigned            injected_cr:  1;      // CR has been added before current NL
  unsigned            prev_cr:      1;      // Previous char in TX queue was a CR

  unsigned            size_query:   1;

  TerminalSize        term_size;
  ConsoleShell        shell;
};


#ifndef putnl
#  define putnl()   fputc('\n', stdout)
#endif


#ifdef __cplusplus
extern "C" {
#endif

void console_add(Console *con);
bool console_remove(Console *con);
bool console_set_default(Console *con);
Console *active_console(void);
Console *first_console(void);
void task_set_console(TaskHandle_t task, Console *con);
Console *console_find(ConsoleID id);

static inline size_t console_rx_enqueue(Console *con, uint8_t *data, size_t len) {
  // Input to the RX queue should have a single writer so no need to lock
  return isr_queue_push(con->stream.rx_queue, data, len);
}


static inline uint8_t console_event_id(Console *con) {
  return (con->id.kind << 4) | (con->id.id & 0x0F);
}

static inline ConsoleID console_decode_id(uint8_t con_id) {
  return (ConsoleID){.kind = (enum ConsoleKind)(con_id >> 4), .id = con_id & 0x0F};
}


size_t console_rx_unqueue(Console *con, uint8_t *data, size_t len);
size_t console_send(Console *con, uint8_t *data, size_t len);

bool console_blocking_stdout(Console *con, bool mode);
const char *console_kind(enum ConsoleKind kind);
void console_name(Console *con, char *name, size_t name_len);

void console_init(Console *con, ConsoleConfigFull *cfg);
Console *console_alloc(ConsoleConfigBasic *cfg);

int console_printf(Console *con, const char *fmt, ...);
void console_query_cursor_pos(Console *con);
void console_query_terminal_size(Console *con);

__attribute__((weak))
uint8_t show_prompt(void *eval_ctx);

#ifdef __cplusplus
}
#endif

#endif // CONSOLE_H
