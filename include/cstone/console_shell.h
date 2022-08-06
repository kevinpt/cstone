#ifndef CONSOLE_SHELL_H
#define CONSOLE_SHELL_H

// ******************** Configuration ********************
#define USE_COMMAND_HELP_DETAIL
#define USE_SORTED_COMMAND_HELP
#define USE_CONSOLE_HISTORY


#ifdef USE_CONSOLE_HISTORY
#  include "cstone/console_history.h"
#endif

#define CONSOLE_MAX_ARGS  8
#define CONSOLE_ARGV_LEN  (1+CONSOLE_MAX_ARGS+1) // Command parameter plus NULL entry for argv[] array

// Special return codes for console commands
#define CONSOLE_NO_PROMPT   INT32_MIN     // Suppress prompt after command returns
#define CONSOLE_RUN_TASK    (INT32_MIN+1) // Command launches task
#define CONSOLE_NO_CMD      (INT32_MIN+2) // Command not found

#define DISPLAY_PROMPT(s) ((s) > CONSOLE_RUN_TASK)


// ******************** Key codes ********************
#define CH_CTRL_A   0x01
#define CH_CTRL_C   0x03
#define CH_CTRL_E   0x05
#define CH_CTRL_K   0x0B
#define CH_CTRL_U   0x15
#define CH_CTRL_W   0x17
#define CH_CTRL_SLASH 0x1F

#define CH_BS       0x08
#define CH_ESC      0x1B
#define CH_CSI      '['
#define CH_DEL      0x7F

// Pseudo key codes for escape sequences
#define VT100_KEY       0x0800
#define VT100_KEY_UP    (VT100_KEY | 'A')
#define VT100_KEY_DOWN  (VT100_KEY | 'B')
#define VT100_KEY_RIGHT (VT100_KEY | 'C')
#define VT100_KEY_LEFT  (VT100_KEY | 'D')
#define VT100_KEY_DEL   (VT100_KEY | '3')

#define VT100_MOD_META    0x0100
#define VT100_KEY_META_B  (VT100_MOD_META | 'b')
#define VT100_KEY_META_D  (VT100_MOD_META | 'd')
#define VT100_KEY_META_F  (VT100_MOD_META | 'f')


typedef struct Console Console;

typedef int16_t KeyCode;

typedef uint8_t (*ConsolePrompt)(void *eval_ctx);
typedef int32_t (*ConsoleCommand)(uint8_t argc, char *argv[], void *eval_ctx);
typedef void (*ConsoleInputRedirect)(Console *con, KeyCode key_code, void *eval_ctx);
typedef bool (*ShellCommandHandler)(char *input, void *ctx);

typedef struct {
  const char      name[16];
  ConsoleCommand  cmd;
#ifdef USE_COMMAND_HELP_DETAIL
  const char      help[24];
#endif
} ConsoleCommandDef;

#define MAX_COMMAND_SETS  4
typedef struct {
  const ConsoleCommandDef *cmd_sets[MAX_COMMAND_SETS];
  unsigned total_cmds;
} ConsoleCommandSuite;


#ifdef USE_COMMAND_HELP_DETAIL
#  define CMD_DEF(name, func, help)  {name, func, help}
#else
// Omit help strings
#  define CMD_DEF(name, func, help)  {name, func}
#endif

#define CMD_END   CMD_DEF("",       NULL,    "")

// Index into line buffer
typedef uint8_t LineIndex;


typedef struct {
  char       *buf;
  LineIndex   buf_size;
  LineIndex   cursor;
  LineIndex   line_end;
} LineBuffer;


enum EscParseState {
  ESC_IDLE = 0,
  ESC_GOT_ESC,
  ESC_GOT_CSI,
  ESC_GOT_MIDDLE,
  ESC_ERR
};


#define MAX_ESC_PARAMS  6

typedef struct {
  enum EscParseState state;
  int16_t params[MAX_ESC_PARAMS];
  uint8_t param_num;
  char  mid_ch;
  char  final_ch;
  bool  is_escape;
  bool  is_meta;
} EscParser;





typedef struct {
  LineBuffer          line;

  ConsoleCommandSuite *cmd_suite;

  ConsolePrompt       show_prompt;
  ConsoleInputRedirect input_redirect;
  void               *eval_ctx;     // App state passed to show_prompt() and input_redirect()

  ShellCommandHandler  command_handler;
  void                *command_handler_ctx;

#ifdef USE_CONSOLE_HISTORY
  char               *con_hist_buf; // FIXME: Move into con_hist
  size_t              con_hist_size;
  ConsoleHistory     con_hist;
#endif

  EscParser     escape_parser;

  char         *argv[CONSOLE_ARGV_LEN];
  uint8_t       argc;

  uint8_t       prompt_len;
  bool          echo;
  char          mask_ch; // Character to echo back (for password prompts)
  bool          suppress_prompt;
} ConsoleShell;


struct ConsoleConfigFull;


#ifdef __cplusplus
extern "C" {
#endif

void command_suite_init(ConsoleCommandSuite *suite);
bool command_suite_add(ConsoleCommandSuite *suite, const ConsoleCommandDef *cmds);

void shell_init(ConsoleShell *shell, struct ConsoleConfigFull *cfg);

void shell_process_rx(Console *con);

void shell_set_echo(ConsoleShell *shell, bool echo_on);
void shell_mask_echo(ConsoleShell *shell, char mask_ch);
void shell_unmask_echo(ConsoleShell *shell);

void shell_show_prompt(ConsoleShell *shell);
void shell_show_boot_prompt(ConsoleShell *shell);
void shell_suppress_prompt(ConsoleShell *shell, bool suppress);
void shell_reset(ConsoleShell *shell);

//void console_set_task_cancel(Console *con, ConsoleTaskCancel task_cancel);
void shell_redirect_input(ConsoleShell *shell, ConsoleInputRedirect input_redirect);
void shell_cancel_redirect(ConsoleShell *shell);

#ifdef USE_CONSOLE_HISTORY
void shell_show_history(ConsoleShell *shell);
#endif

bool gets_async(ShellCommandHandler command_handler, void *ctx);


#ifdef __cplusplus
}
#endif

#endif // CONSOLE_SHELL_H
