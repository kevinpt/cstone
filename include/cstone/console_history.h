#ifndef CONSOLE_HISTORY_H
#define CONSOLE_HISTORY_H


#define CONSOLE_HISTORY_BUF_LEN_MAX   255


typedef struct ConsoleHistory {
//  HistFifo  fifo;
  void     *fifo; // BipBuf FIFO
  char     *cur_chunk;
  size_t    chunk_len;
  size_t    chunk_pos;
} ConsoleHistory;


#ifdef __cplusplus
extern "C" {
#endif

void history_init(ConsoleHistory *hist, char *buf, size_t buf_len);

void history_reset_iter(ConsoleHistory *hist);
void history_pop_command(ConsoleHistory *hist);
void history_push_command(ConsoleHistory *hist, char *cmd);
char *history_next_command(ConsoleHistory *hist);
char *history_prev_command(ConsoleHistory *hist);

#ifdef __cplusplus
}
#endif

#endif // CONSOLE_HISTORY_H
