#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <ctype.h>

#include "cstone/platform.h"

#include "FreeRTOS.h"
#include "semphr.h"

#include "cstone/console.h"
#include "cstone/term_color.h"
#include "cstone/blocking_io.h"

#include "task.h"
#include "cstone/prop_id.h"
#include "cstone/umsg.h"

#include "util/minmax.h"
#include "util/string_ops.h"
#include "bsd/string.h"


#ifndef COUNT_OF
#  define COUNT_OF(a) (sizeof(a) / sizeof(*(a)))
#endif

// https://vt100.net/docs/vt100-ug/chapter3.html
// https://vt100.net/docs/vt510-rm/chapter8
// https://invisible-island.net/xterm/ctlseqs/ctlseqs.html#h2-PC-Style-Function-Keys

// CSI final chars
#define VT100_UP    'A'
#define VT100_DOWN  'B'
#define VT100_RIGHT 'C'
#define VT100_LEFT  'D'
#define VT100_HOME  'H'
#define VT100_END   'F'
#define VT100_EXT_KEY   '~'
#define VT100_EXT_DEL   3
#define VT100_EXT_HOME  1
#define VT100_EXT_END   4


// Terminal control
#define VT100_CSI           "\33["
#define VT100_CURS_RIGHT    "C"
#define VT100_CURS_LEFT     "D"
#define VT100_CURS_RIGHT_1  "\33[1C"
#define VT100_CURS_LEFT_1   "\33[1D"
#define VT100_CURS_CLEAR_RIGHT "\33[0K"
#define VT100_CURS_SAVE     "\33" "7"
#define VT100_CURS_RESTORE  "\33" "8"
#define VT100_LN_CLR        "\33[2K"
#define VT100_LN_HOME       "\33[200D"  // Move left 200 chars to force into home column


static const char s_cmd_delims[] = "\r\n\t "; // Split on whitespace



// ******************** Line buffer management ********************

static inline void line_clear(LineBuffer *line) {
  memset(line->buf, 0, line->buf_size);
  line->cursor    = 0;
  line->line_end  = 0; // Location of terminating NUL
}

static inline bool line_is_full(LineBuffer *line) {
  return line->line_end >= line->buf_size-1;
}

static inline bool line_cursor_at_end(LineBuffer *line) {
  return line->cursor >= line->line_end;
}

static inline bool line_insert(LineBuffer *line, uint8_t ch) {
  if(line_is_full(line))
    return false;

  if(!line_cursor_at_end(line)) { // Shift trailing text right
    memmove(&line->buf[line->cursor+1], &line->buf[line->cursor], line->line_end - line->cursor);
  }

  line->buf[line->cursor++] = ch;
  line->line_end++;
  return true;
}


static inline bool line_backspace(LineBuffer *line) {
  if(line->cursor == 0) // Nothing to delete to left of cursor
    return false;

  if(!line_cursor_at_end(line)) { // Shift trailing text left
    memmove(&line->buf[line->cursor-1], &line->buf[line->cursor], line->line_end - line->cursor);
  }

  line->cursor--;
  line->line_end--;
  line->buf[line->line_end] = '\0';
  return true;
}


static inline bool line_delete(LineBuffer *line) {
  if(line_cursor_at_end(line)) // Nothing to delete under the cursor
    return false;

  // Shift trailing text left
  memmove(&line->buf[line->cursor], &line->buf[line->cursor+1], line->line_end - line->cursor);

  line->line_end--;
  line->buf[line->line_end] = '\0';
  return true;
}


static inline bool line_move_left(LineBuffer *line, LineIndex count) {
  if(line->cursor < count || count == 0)
    return false;

  line->cursor -= count;
  return true;
}

static inline bool line_move_right(LineBuffer *line, LineIndex count) {
  if(line->cursor+count > line->line_end || count == 0)
    return false;

  line->cursor += count;
  return true;
}


static void line_redraw(LineBuffer *line, LineIndex old_end) {
  // Redraw the portion of the line after the cursor

  // Save cursor
  fputs(VT100_CURS_SAVE, stdout);

  // Draw chars from cursor to end
  for(LineIndex i = line->cursor; i < line->line_end; i++) {
    putc(line->buf[i], stdout);
  }

  // Overstrike old end
  for(LineIndex i = line->line_end; i < old_end; i++) {
    putc(' ', stdout);
  }

  // Restore cursor
  fputs(VT100_CURS_RESTORE, stdout);
}



static inline void line_replace(LineBuffer *line, char *text, ConsoleShell *shell) {
  memset(line->buf, 0, line->buf_size); // line_insert() doesn't add NULs so re-init now
  size_t text_len = strlcpy(line->buf, text, line->buf_size);
  line->line_end = min(text_len, line->buf_size-1);
  line->cursor = line->line_end;

  // Clear line and move cursor to column 0
  fputs(VT100_LN_CLR VT100_LN_HOME, stdout);

  // Print prompt
  shell_show_prompt(shell);

  // Print command
  for(LineIndex i = 0; i < line->line_end; i++) {
    putc(line->buf[i], stdout);
  }
}


static void line_kill_after(LineBuffer *line, LineIndex count) {
  // Remove count chars after cursor, 0 for all chars
  LineIndex killed = line->line_end - line->cursor;

  if(killed == 0) // Nothing to do
    return;

  if(count > 0 && killed > count)
    killed = count;

  if(line->cursor + killed < line->line_end) {  // Partial kill
    size_t move_count = line->line_end - (line->cursor + killed);
    memmove(&line->buf[line->cursor], &line->buf[line->cursor+killed], move_count);
    memset(&line->buf[line->cursor+move_count], 0, move_count);
  } else {  // Kill entire remainder of line
    memset(&line->buf[line->cursor], 0, killed);
  }

  line->line_end -= killed;
}


static LineIndex line_kill_before(LineBuffer *line, LineIndex count) {
  // Remove count chars before cursor, 0 for all chars
  LineIndex killed = line->cursor;

  if(killed == 0) // Nothing to do
    return 0;

  if(count > 0 && killed > count)
    killed = count;

  size_t move_count = line->line_end - line->cursor;
  if(!line_cursor_at_end(line)) { // Shift trailing text left
    memmove(&line->buf[line->cursor - killed], &line->buf[line->cursor], move_count);
    memset(&line->buf[line->line_end - killed], 0, move_count);
  } else {
    memset(&line->buf[line->line_end - killed], 0, killed);
  }

  line->cursor -= killed;
  line->line_end -= killed;

  return killed;
}


static LineIndex line_prev_word(LineBuffer *line) {
  // Search for word boundary before cursor
  if(line->cursor == 0)
    return 0;

  LineIndex pos = line->cursor-1;
  bool in_word = !strchr(s_cmd_delims, line->buf[pos]);
  while(pos > 0) {
    bool in_delim = strchr(s_cmd_delims, line->buf[pos]);

    if(in_word) {
      if(in_delim) { // Found word boundary
        pos++;
        break;
      }
    } else if(!in_delim) { // Found end of word
      in_word = true;
    }
    pos--;
  }

  return pos;
}

static LineIndex line_next_word(LineBuffer *line) {
  // Search for word boundary after cursor

  LineIndex pos = line->cursor;
  bool in_word = !strchr(s_cmd_delims, line->buf[pos]);
  while(pos < line->line_end) {
    bool in_delim = strchr(s_cmd_delims, line->buf[pos]);

    if(in_word) {
      if(in_delim) { // Found word boundary
        break;
      }
    } else if(!in_delim) { // Found start of word
      in_word = true;
    }
    pos++;
  }

  return pos;
}


static inline void line_strip_whitespace(LineBuffer *line) {
  // Remove leading whitespace from line buffer

  LineIndex i;
  for(i = 0; i < line->line_end; i++) {
    if(!strchr(s_cmd_delims, line->buf[i]))
      break;
  }

  if(i > 0) {
    memmove(line->buf, &line->buf[i], line->line_end+1 - i);
    line->line_end -= i;
  }
}


// ******************** Command suites ********************

static unsigned command__set_count(const ConsoleCommandDef *cmds) {
  unsigned i;
  for(i = 0; cmds[i].cmd; i++) {}

  return i;
}


static unsigned command__suite_count(ConsoleCommandSuite *suite) {
  unsigned total_cmds = 0;
  for(int i = 0; i < MAX_COMMAND_SETS; i++) {
    if(suite->cmd_sets[i] == NULL)
      break;

    total_cmds += command__set_count(suite->cmd_sets[i]);
  }

  return total_cmds;
}


void command_suite_init(ConsoleCommandSuite *suite) {
  // For a statically defined suite we won't be calling command_suite_add() so get the
  // total count now.
  suite->total_cmds = command__suite_count(suite);
}


bool command_suite_add(ConsoleCommandSuite *suite, const ConsoleCommandDef *cmds) {
  int i;
  for(i = 0; i < MAX_COMMAND_SETS; i++) {
    if(suite->cmd_sets[i] == NULL) {
      suite->cmd_sets[i] = cmds;
      break;
    }
  }

  if(suite->total_cmds == 0)
    suite->total_cmds = command__suite_count(suite);
  else
    suite->total_cmds += command__set_count(cmds);

  return i < MAX_COMMAND_SETS;
}


// ******************** Console control ********************

void shell_init(ConsoleShell *shell, ConsoleConfigFull *cfg) {
  shell->line.buf       = cfg->line_buf;
  shell->line.buf_size  = cfg->line_buf_size;
  line_clear(&shell->line);

#ifdef USE_CONSOLE_HISTORY
  shell->con_hist_buf   = cfg->con_hist_buf;
  shell->con_hist_size  = cfg->con_hist_size;
  history_init(&shell->con_hist, shell->con_hist_buf, shell->con_hist_size);
#endif

  shell->cmd_suite    = cfg->cmd_suite;
  shell->show_prompt  = cfg->show_prompt;
  shell->eval_ctx     = cfg->eval_ctx;

  shell->echo         = true;
}


static uint8_t shell__parse_args(ConsoleShell *shell) {
  char *save_state;

  shell->argc = 0;
  memset(shell->argv, 0, CONSOLE_ARGV_LEN * sizeof(char *));

  char *tok = strtok_r(shell->line.buf, s_cmd_delims, &save_state);
  while(tok && shell->argc < CONSOLE_MAX_ARGS+1) {
    shell->argv[shell->argc++] = tok;
    tok = strtok_r(NULL, s_cmd_delims, &save_state);
  }

  return shell->argc;
}


static bool match_command(const char *cmd_name, const char *input) {
  bool prev_cap = false;
  bool match = false;

  // We need to do case insensitive match on both the full command name and
  // its abbreviated form using the "SHORTfull" format in the ConsoleCommandDef.
  while(*cmd_name != '\0' && *input != '\0') {
    char cmd_ch = tolower(*cmd_name);
    if(*input != cmd_ch)  // Mismatch
      break;

    prev_cap = isupper(*cmd_name);
    cmd_name++;
    input++;
  }

  // Match if we reached end of input string and command string
  // or we reached the end of input and the abbreviated command.
  if(*input == '\0') {
    if(*cmd_name == '\0' || (prev_cap && !isupper(*cmd_name))) {
      match = true;
    }
  }

  return match;
}


static int32_t shell__eval(ConsoleShell *shell) {
  const ConsoleCommandDef **cmd_sets = shell->cmd_suite->cmd_sets;

  for(int i = 0; i < MAX_COMMAND_SETS; i++) {
    if(!cmd_sets[i])
      break;

    const ConsoleCommandDef *cur_cmd = cmd_sets[i];

    // Search for command in array
    while(cur_cmd->cmd) {
      if(match_command(cur_cmd->name, shell->argv[0])) {
        int32_t status = cur_cmd->cmd(shell->argc, shell->argv, shell->eval_ctx);
        if(status != 0 && DISPLAY_PROMPT(status))
          printf("ERROR: %" PRIi32 "\n", status);

        return status;
      }
      cur_cmd++;
    }

  }

  printf("ERROR: Unknown command '%s'\n", shell->argv[0]);
  return CONSOLE_NO_CMD;
}


static inline void print__cmd_help(const ConsoleCommandDef *cmd) {
      bprintf("  %-6s", cmd->name);

#ifdef USE_COMMAND_HELP_DETAIL
      printf("\t%s\n", cmd->help);
#else // No help text
      putnl();
#endif
}


// qsort callback
static int cmd_sort_compar(const void *a, const void *b) {
  const ConsoleCommandDef *cmd_a = *(const ConsoleCommandDef **)a;
  const ConsoleCommandDef *cmd_b = *(const ConsoleCommandDef **)b;
  return stricmp(cmd_a->name, cmd_b->name);
}


// Print command help invoked by '?' input
static inline void shell__show_help(ConsoleShell *shell) {
  const ConsoleCommandDef **cmd_sets = shell->cmd_suite->cmd_sets;

  puts("Commands:");

#ifdef USE_SORTED_COMMAND_HELP
  // Sort the command definitions
  const ConsoleCommandDef **sorted = cs_malloc(shell->cmd_suite->total_cmds * sizeof(ConsoleCommandDef *));
  if(sorted) {
    const ConsoleCommandDef **pos = sorted;
    // Populate the sorted array
    for(int i = 0; i < MAX_COMMAND_SETS; i++) {
      if(!cmd_sets[i])
        break;

      for(const ConsoleCommandDef *cur = cmd_sets[i]; cur && cur->cmd; cur++) {
        *pos++ = cur;
      }
    }

    qsort(sorted, shell->cmd_suite->total_cmds, sizeof(ConsoleCommandDef *), cmd_sort_compar);

    for(unsigned i = 0; i < shell->cmd_suite->total_cmds; i++) {
      print__cmd_help(sorted[i]);
    }

    cs_free(sorted);
    return;
  }
  // Malloc failed. Fallback to printing unsorted list.
#endif

  // Print unsorted command list
  for(int i = 0; i < MAX_COMMAND_SETS; i++) {
    if(!cmd_sets[i])
      break;

    for(const ConsoleCommandDef *cur = cmd_sets[i]; cur && cur->cmd; cur++) {
      print__cmd_help(cur);
    }
  }
}


static bool parse_escape_code(EscParser *ep, char next_ch) {
#define IS_MIDDLE_BYTE(ch)  ((ch) >= 0x20 && (ch) <= 0x2F)
#define IS_FINAL_BYTE(ch)  ((ch) >= 0x40 && (ch) <= 0x7E)

  switch(ep->state) {
  case ESC_IDLE:
    if(next_ch == CH_ESC) {
      ep->state = ESC_GOT_ESC;
    } else { // Normal character
      ep->is_escape = false;
      ep->final_ch = next_ch;
      return true;
    }
    break;

  case ESC_GOT_ESC:
    if(next_ch == CH_CSI) { // ANSI escape sequence
      ep->state = ESC_GOT_CSI;

    } else if(isalnum(next_ch)) { // Alt + key
      ep->final_ch  = next_ch;
      ep->is_meta   = true;
      ep->state     = ESC_IDLE;
      return true;

    } else {  // Invalid
      ep->state = ESC_ERR;
    }
    break;

  case ESC_GOT_CSI:
    // Parse numeric params until a middle or final char is received
    if(IS_MIDDLE_BYTE(next_ch)) {
      ep->mid_ch  = next_ch;
      ep->state   = ESC_GOT_MIDDLE;

    } else if(IS_FINAL_BYTE(next_ch)) {
      ep->final_ch  = next_ch;
      ep->state     = ESC_IDLE;
      ep->is_escape = true;
      return true;

    } else if(isdigit(next_ch)) {
      if(ep->param_num == 0)
        ep->param_num++;

      int16_t val = ep->params[ep->param_num-1];
      val = val * 10 + (next_ch - '0');
      ep->params[ep->param_num-1] = val;

    } else if(next_ch == ';') {
      if(ep->param_num < MAX_ESC_PARAMS)
        ep->param_num++;
      else
        ep->state = ESC_ERR;
    }
    break;

  case ESC_GOT_MIDDLE:
    if(IS_FINAL_BYTE(next_ch)) {
      ep->final_ch  = next_ch;
      ep->state     = ESC_IDLE;
      ep->is_escape = true;
      return true;
    } else {
      ep->state = ESC_ERR;
    }
    break;

  case ESC_ERR:
    if(IS_FINAL_BYTE(next_ch)) {
      ep->state = ESC_IDLE;
    }
    break;

  default:
    break;
  }

  return false;
}


static KeyCode decode_escape_code(EscParser *ep) {
  KeyCode key_code = '\0';

  if(!ep->is_escape) {
    key_code = ep->final_ch;
    if(ep->is_meta)
      key_code |= VT100_MOD_META;

  } else {  // Decode parsed escape
    switch(ep->final_ch) {
#ifdef USE_CONSOLE_HISTORY
    case VT100_UP:
      key_code = VT100_KEY_UP; break;

    case VT100_DOWN:
      key_code = VT100_KEY_DOWN; break;
#endif
    case VT100_RIGHT:
      key_code = VT100_KEY_RIGHT; break;

    case VT100_LEFT:
      key_code = VT100_KEY_LEFT; break;

    case VT100_HOME:
      key_code = CH_CTRL_A; break;

    case VT100_END:
      key_code = CH_CTRL_E; break;

    case VT100_EXT_KEY:
      // Key codes of the form "CSI n ~"
      if(ep->param_num == 1) {  // No modifiers
        switch(ep->params[0]) {
        case VT100_EXT_DEL:
          key_code = VT100_KEY_DEL; break;

        case VT100_EXT_HOME:
          key_code = CH_CTRL_A; break;

        case VT100_EXT_END:
          key_code = CH_CTRL_E; break;

        default:
          break;
        }
      }
      break;

    default:
      break;
    }
  }

  return key_code;
}


static bool decode__dsr_response(Console *con) {
  if(!(con->shell.escape_parser.is_escape && con->shell.escape_parser.final_ch == 'R'))
    return false;

  if(con->shell.escape_parser.param_num == 2) { // Valid response to DSR query
    uint32_t event_id;
    if(con->size_query) { // Response to console_query_terminal_size()
      con->term_size = (TerminalSize){con->shell.escape_parser.params[1],
                                      con->shell.escape_parser.params[0]};
      con->size_query = 0;
      event_id = (P1_EVENT | P2_CON | P2_ARR(console_event_id(con)) | P4_SIZE);

    } else {  // General cursor position query
      event_id = (P1_EVENT | P2_CON | P2_ARR(console_event_id(con)) | P4_LOC);
    }

    // Report DSR response
    uintptr_t event_pos = (con->shell.escape_parser.params[1] << 8) | (con->shell.escape_parser.params[0] & 0xFF);
    report_event(event_id, event_pos);
  }

  return true;
}


static bool process_special_keys(ConsoleShell *shell, KeyCode key_code) {
  // Check for special chars and escape codes

  LineIndex old_end = shell->line.line_end;

  switch(key_code) {
  case CH_DEL:
  case CH_BS:
    if(line_backspace(&shell->line)) {
      if(!line_cursor_at_end(&shell->line)) { // In middle of line
        putc(CH_BS, stdout);
        line_redraw(&shell->line, old_end);
      } else { // At end of line
        // Backup cursor and overwrite with space
        putc(CH_BS, stdout);
        putc(' ',stdout);
        putc(CH_BS, stdout);
      }
    }
    break;

  case CH_CTRL_A: // Move to line beginning
    {
      LineIndex cur_move = shell->line.cursor;
      if(line_move_left(&shell->line, cur_move)) {
        printf(VT100_CSI "%u" VT100_CURS_LEFT, cur_move);
      }
    }
    break;

  case CH_CTRL_E: // Move to line end
    {
      LineIndex cur_move = shell->line.line_end - shell->line.cursor;
      if(line_move_right(&shell->line, cur_move)) {
        printf(VT100_CSI "%u" VT100_CURS_RIGHT, cur_move);
      }
    }
    break;

  case CH_CTRL_K: // Kill after cursor
    line_kill_after(&shell->line, 0);
    fputs(VT100_CURS_CLEAR_RIGHT, stdout);
    break;

  case CH_CTRL_U: // Kill before cursor
    {
      if(shell->line.cursor > 0) {
        LineIndex killed = line_kill_before(&shell->line, shell->line.cursor);
        printf(VT100_CSI "%u" VT100_CURS_LEFT, killed); // Reposition terminal cursor
        line_redraw(&shell->line, old_end);
      }
    }
    break;


  case CH_CTRL_W: // Kill word before cursor
    {
      LineIndex killed = shell->line.cursor - line_prev_word(&shell->line);
      if(killed > 0) {
        line_kill_before(&shell->line, killed);
        printf(VT100_CSI "%u" VT100_CURS_LEFT, killed); // Reposition terminal cursor
        line_redraw(&shell->line, old_end);
      }
    }
    break;

  case CH_CTRL_SLASH: // Kill whole line
    {
      LineIndex cur_move = shell->line.cursor;
      if(line_move_left(&shell->line, cur_move)) {
        printf(VT100_CSI "%u" VT100_CURS_LEFT, cur_move);

        line_kill_after(&shell->line, 0);
        fputs(VT100_CURS_CLEAR_RIGHT, stdout);
      }
    }
    break;

  case CH_CTRL_C: // Abort current line
    puts("^C");
    line_clear(&shell->line);
    shell_show_prompt(shell);
    break;

  case VT100_KEY_META_B:  // Move cursor to prev word
    {
      LineIndex word_pos = line_prev_word(&shell->line);
      if(shell->line.cursor > word_pos) {
        printf(VT100_CSI "%u" VT100_CURS_LEFT, shell->line.cursor - word_pos); // Reposition terminal cursor
        shell->line.cursor = word_pos;
      }
    }
    break;

  case VT100_KEY_META_F:  // Move cursor to next word
    {
      LineIndex word_pos = line_next_word(&shell->line);
      if(shell->line.cursor < word_pos) {
        printf(VT100_CSI "%u" VT100_CURS_RIGHT, word_pos - shell->line.cursor); // Reposition terminal cursor
        shell->line.cursor = word_pos;
      }
    }
    break;

  case VT100_KEY_META_D:  // Kill word after cursor
    {
      LineIndex killed = line_next_word(&shell->line) - shell->line.cursor;
      if(killed > 0) {
        line_kill_after(&shell->line, killed);
        line_redraw(&shell->line, old_end);
      }
    }
    break;

#ifdef USE_CONSOLE_HISTORY
  case VT100_KEY_UP:
    {
      char *cmd = history_prev_command(&shell->con_hist);
      if(cmd) // Transfer into line buf
        line_replace(&shell->line, cmd, shell);
    }
    break;

  case VT100_KEY_DOWN:
    {
      char *cmd = history_next_command(&shell->con_hist);
      if(cmd) // Transfer into line buf
        line_replace(&shell->line, cmd, shell);
      }
    break;
#endif
  case VT100_KEY_RIGHT:
    if(line_move_right(&shell->line, 1)) {
        fputs(VT100_CURS_RIGHT_1, stdout);
    }
    break;

  case VT100_KEY_LEFT:
    if(line_move_left(&shell->line, 1)) {
        fputs(VT100_CURS_LEFT_1, stdout);
    }
    break;

  case VT100_KEY_DEL:
    if(line_delete(&shell->line))
      line_redraw(&shell->line, old_end);
    break;

  default: // Regular char
    return false;
    break;
  }

  return true; // Special char
}


// Take data from ISR RX queue and assemble command strings
// for evaluation.
void shell_process_rx(Console *con) {
  char ch;
  bool cmd_ready = false;

  if(isr_queue_count(con->rx_queue) == 0) // Nothing new to process
    return;

  // Transfer queued input to line buf until end of line
  while(isr_queue_pop_one(con->rx_queue, (uint8_t *)&ch)) {
    if(!parse_escape_code(&con->shell.escape_parser, ch))
      continue; // Still parsing escape sequence

    if(decode__dsr_response(con)) // Check for response to DSR query
      continue;

    KeyCode key_code = decode_escape_code(&con->shell.escape_parser);
    memset(&con->shell.escape_parser, 0, sizeof(con->shell.escape_parser)); // Reset parser
    if(key_code == '\0')  // Unsupported key
      continue;

    if(con->shell.input_redirect) {
      con->shell.input_redirect(con, key_code, con->shell.eval_ctx);

    } else if(!process_special_keys(&con->shell, key_code)) {
      // Normal char: Goes into line buf

      // Handle echo
      if(con->shell.echo && isprint(ch) && !line_is_full(&con->shell.line)) {
        if(con->shell.mask_ch == '\0' || !isgraph(ch))
          putc(ch, stdout);
        else // Hide echo with mask chars
          putc(con->shell.mask_ch, stdout);
      }

      // User finished command?
      if(ch == '\n' || ch == '\r') { // Ready to parse command
        cmd_ready = true;
        break;

      // Add char to edit buffer if room
      } else if(!line_is_full(&con->shell.line)) {
        if(line_insert(&con->shell.line, ch)) {
          if(!line_cursor_at_end(&con->shell.line)) // Re-render line
            line_redraw(&con->shell.line, 0);
        }
      }
    }

  } // while()

  if(cmd_ready) {
    int32_t eval_status = 0;

    if(con->shell.echo)
      putc('\n', stdout); // Echo newline

    line_strip_whitespace(&con->shell.line);

    if(con->shell.command_handler) {  // Custom command handling
      bool done = con->shell.command_handler(con->shell.line.buf, con->shell.command_handler_ctx);
      if(done) {
        con->shell.command_handler      = NULL;
        con->shell.command_handler_ctx  = NULL;
        shell_suppress_prompt(&con->shell, false);
      }

    } else {  // Standard command handling

#ifdef USE_CONSOLE_HISTORY
      // Add to history buffer
      if(strlen(con->shell.line.buf) > 0 && con->shell.line.buf[0] != '?') {
        history_push_command(&con->shell.con_hist, con->shell.line.buf);
      }
#endif

      // Parse and evaluate command
      if(shell__parse_args(&con->shell) > 0) {
        if(!strcmp("?", con->shell.argv[0])) { // Show help
          shell__show_help(&con->shell);
        } else if(strlen(con->shell.argv[0]) > 0) { // Lookup command
          eval_status = shell__eval(&con->shell);
        }
      }
    }

#if 0
    // Remove any remaining whitespace from queue
    if(isr_queue_peek_one(con->rx_queue, (uint8_t *)&ch)) {
      while(strchr(s_cmd_delims, ch)) {
        isr_queue_pop_one(con->rx_queue, (uint8_t *)&ch);
        if(!isr_queue_peek_one(con->rx_queue, (uint8_t *)&ch))
          break;
      }
    }
#endif

    // Prep for next command
    line_clear(&con->shell.line);

    if(DISPLAY_PROMPT(eval_status) && !con->shell.input_redirect) {
      shell_show_prompt(&con->shell);
    }
  }

}


void shell_set_echo(ConsoleShell *shell, bool echo_on) {
  shell->echo = echo_on;
}

void shell_mask_echo(ConsoleShell *shell, char mask_ch) {
  shell->mask_ch = mask_ch;
}

void shell_unmask_echo(ConsoleShell *shell) {
  shell->mask_ch = '\0';
}


void shell_show_prompt(ConsoleShell *shell) {
  if(!shell->suppress_prompt && shell->show_prompt)
    shell->prompt_len = shell->show_prompt(shell->eval_ctx);
}

void shell_suppress_prompt(ConsoleShell *shell, bool suppress) {
  shell->suppress_prompt = suppress;
}


void shell_show_boot_prompt(ConsoleShell *shell) {
  puts("\nEnter '?' for command list\n");

  if(shell->show_prompt)
    shell->prompt_len = shell->show_prompt(shell->eval_ctx);
}


void shell_reset(ConsoleShell *shell) {
  // Clear the screen
  fputs("\033[H\033[2J", stdout);

  shell_suppress_prompt(shell, false);
  shell_show_boot_prompt(shell);
}

void shell_redirect_input(ConsoleShell *shell, ConsoleInputRedirect input_redirect) {
  shell->input_redirect = input_redirect;
}

void shell_cancel_redirect(ConsoleShell *shell) {
  shell->input_redirect = NULL;

  putnl();
  shell_show_prompt(shell);
}


bool gets_async(ShellCommandHandler command_handler, void *ctx) {
  Console *con = active_console();
  if(!con)
    return false;

taskENTER_CRITICAL();
  if(!con->shell.command_handler) {
    con->shell.command_handler      = command_handler;
    con->shell.command_handler_ctx  = ctx;
taskEXIT_CRITICAL();
  } else {  // Another handler is already active
taskEXIT_CRITICAL();
    return false;
  }

  shell_suppress_prompt(&con->shell, true);

  return true;
}

