#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "FreeRTOS.h"
#include "semphr.h"

#include "cstone/console_history.h"
#include "cstone/console.h"

#include "cstone/bipbuf_char.h"
typedef BipFifo_char HistFifo;


void history_init(ConsoleHistory *hist, char *buf, size_t buf_len) {
  memset(hist, 0, sizeof *hist);

  HistFifo *hf = (HistFifo *)malloc(sizeof *hf); // FIXME: Convert to C
  if(hf) {
    bipfifo_init__char(hf, buf, buf_len);
    hist->fifo = hf;
  }
};


void history_reset_iter(ConsoleHistory *hist) {
  hist->cur_chunk = NULL;
};

void history_pop_command(ConsoleHistory *hist) {
    char *head_chunk = NULL;
    HistFifo *hf = (HistFifo *)hist->fifo;
    size_t chunk_len =  bipfifo_next_chunk__char(hf, &head_chunk);
    if(chunk_len > 0) {
      size_t cmd_len = strlen(head_chunk)+1;
      bipfifo_pop__char(hf, &head_chunk, cmd_len);
    }
}


void history_push_command(ConsoleHistory *hist, char *cmd) {
  history_reset_iter(hist);

  // Check if this is the same as the last command
  char *prev_cmd = history_prev_command(hist);
  if(prev_cmd && !strcmp(prev_cmd, cmd)) {
    history_reset_iter(hist);
    return;
  }

  size_t cmd_len = strlen(cmd) + 1; // Include NUL

  // Create space by popping oldest entries if too full
  HistFifo *hf = (HistFifo *)hist->fifo;
  size_t free_elems = bipfifo_pushable_elems__char(hf);
  while(free_elems < cmd_len && !bipfifo_is_empty__char(hf)) {
    //printf("## FULL  %d < %d\n", free_elems, cmd_len);
    history_pop_command(hist);
    free_elems = bipfifo_pushable_elems__char(hf);
  }

  // Add new command
  bipfifo_push__char(hf, cmd, cmd_len);
  history_reset_iter(hist);

  //printf("## HIST PUSH '%s'\n", cmd);
  //hf->debug();

}


char *history_next_command(ConsoleHistory *hist) {
  char *old_chunk      = hist->cur_chunk;
  size_t old_chunk_len = hist->chunk_len;
  size_t old_chunk_pos = hist->chunk_pos;

  if(hist->cur_chunk) // Advance iterator
    hist->chunk_pos += strlen(&hist->cur_chunk[hist->chunk_pos]) + 1;

  if(!hist->cur_chunk || hist->chunk_pos >= hist->chunk_len) {  // Get new chunk
    HistFifo *hf = (HistFifo *)hist->fifo;
    hist->chunk_len = bipfifo_next_chunk__char(hf, &hist->cur_chunk);
    hist->chunk_pos = 0;
  }

  if(hist->chunk_len == 0) { // No more strings
    // Restore chunk state
    hist->cur_chunk = old_chunk;
    hist->chunk_len = old_chunk_len;
    hist->chunk_pos = old_chunk_pos;

    return NULL;
  }

  char *cmd = &hist->cur_chunk[hist->chunk_pos];

  return cmd;
}


char *history_prev_command(ConsoleHistory *hist) {
  char *old_chunk      = hist->cur_chunk;
  size_t old_chunk_len = hist->chunk_len;
  size_t old_chunk_pos = hist->chunk_pos;

  if(!hist->cur_chunk || hist->chunk_pos == 0) {  // Get new chunk
    HistFifo *hf = (HistFifo *)hist->fifo;
    hist->chunk_len = bipfifo_prev_chunk__char(hf, &hist->cur_chunk);
    hist->chunk_pos = hist->chunk_len;
  }

  if(hist->chunk_len == 0) { // No more strings
    // Restore chunk state
    hist->cur_chunk = old_chunk;
    hist->chunk_len = old_chunk_len;
    hist->chunk_pos = old_chunk_pos;

    return NULL;
  }

  // Search backward for start of prev string in chunk
  if(hist->chunk_pos > 2)
    hist->chunk_pos -= 2;

  while(hist->chunk_pos > 0 && hist->cur_chunk[hist->chunk_pos] != '\0') {
    hist->chunk_pos--;
  }

  if(hist->chunk_pos > 0) // Found prev NUL
    hist->chunk_pos++;

  return &hist->cur_chunk[hist->chunk_pos];
}



void shell_show_history(ConsoleShell *shell) {
  char *cmd;
  ConsoleHistory *hist = (ConsoleHistory *)(&shell->con_hist);

  history_reset_iter(hist);

  cmd = history_next_command(hist); //con_hist->next_command();
  uint8_t i = 1;

  while(cmd) {
    printf("  %2d: %s\n", i, cmd);

    cmd = history_next_command(hist); //con_hist->next_command();
    i++;
  }

  history_reset_iter(hist);
}

