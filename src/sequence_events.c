#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

#include "util/list_ops.h"
#include "util/locking.h"

#include "cstone/prop_id.h"
#include "cstone/umsg.h"
#include "cstone/sequence_events.h"


// List of active sequences
static Sequence *s_SequenceList = NULL;

// Protect linked list access
#define LOCK()    LOCK_TAKE(0)
#define UNLOCK()  LOCK_RELEASE(0)



void sequence_init(Sequence *seq, SequenceEvent *events, uint16_t event_count, uint8_t repeats,
                    SequenceCompletion complete) {

  sequence_configure(seq, events, event_count, repeats, complete);
  seq->next = NULL;
}


bool sequence_init_pairs(Sequence *seq, SequenceEventPair *event_pairs, uint16_t pair_count, uint8_t repeats,
                    SequenceCompletion complete) {

  uint16_t event_count;
  SequenceEvent *events = sequence_compile(event_pairs, pair_count, &event_count);
  if(!events)
    return false;

  sequence_configure(seq, events, event_count, repeats, complete);
  seq->next = NULL;
  
  return true;
}


void sequence_configure(Sequence *seq, SequenceEvent *events, uint16_t event_count, uint8_t repeats,
                        SequenceCompletion complete) {

  seq->events = events;
  seq->event_count = event_count;
  seq->repeats = repeats ? repeats + 1 : repeats;

  seq->cur_event = 0;
  seq->timestamp = 0;
  seq->complete = complete;
}


void sequence_start(Sequence *seq, uint8_t repeats) {
  seq->timestamp = sequence_timestamp();
}


void sequence_restart(Sequence *seq, uint8_t repeats) {
  seq->timestamp = sequence_timestamp();
  seq->repeats   = repeats ? repeats + 1 : repeats;
  seq->cur_event = 0;
}


bool sequence_is_active(Sequence *seq) { return seq->repeats != 1; }


void sequence_add(Sequence *seq) {
LOCK();
  if(seq && seq->next == NULL)
    ll_slist_push(&s_SequenceList, seq);
UNLOCK();
}


void sequence_remove(Sequence *seq) {
LOCK();
  if(seq)
    ll_slist_remove(&s_SequenceList, seq);
UNLOCK();
}


static SequenceTime sequence_update(Sequence *seq, SequenceTime now) {
  SequenceTime delta = now - seq->timestamp; // Elapsed since last update
  bool keep_seq = true;

  if(delta >= seq->events[seq->cur_event].delay_ms) {
    // Advance through sequence to current time position
    do {
      UMsg msg = {
        .id       = seq->events[seq->cur_event].event,
        .source   = (P1_RSRC | P2_SYS | P3_SEQUENCER | P4_TASK),
        .payload  = seq->events[seq->cur_event].payload
      };

      umsg_hub_send(umsg_sys_hub(), &msg, NO_TIMEOUT);

      delta -= seq->events[seq->cur_event++].delay_ms;


      if(seq->cur_event >= seq->event_count) { // End of sequence
        seq->cur_event = 0;

        if(seq->repeats > 1)
          seq->repeats--;

        if(seq->repeats == 1) { // Sequence ended
          delta = 0;

          if(seq->complete) // Notify sequence is complete
            seq->complete(seq);

          keep_seq = seq->repeats != 1;
          break;
        }
        // Run indefinitely if repeats == 0
      }

    } while(delta >= seq->events[seq->cur_event].delay_ms);

    seq->timestamp = now - delta; // Correct for any system delay
  }

  return keep_seq ? seq->events[seq->cur_event].delay_ms - delta : 0;
}


SequenceTime sequence_update_all(void) {
  SequenceTime now = sequence_timestamp();
  SequenceTime next_delay = ~0;
  Sequence *seq;
  
LOCK();
  seq = s_SequenceList;
UNLOCK();

  while(seq) {
    SequenceTime next_seq_delay = sequence_update(seq, now);

    Sequence *next_seq;

LOCK();
    next_seq = seq->next;
UNLOCK();

    if(next_seq_delay == 0) {
      sequence_remove(seq);
    } else {
      if(next_seq_delay < next_delay)
        next_delay = next_seq_delay;
    }

    seq = next_seq;
  }

  if(next_delay == (SequenceTime)~0) // No active sequences
    next_delay = 0;

  return next_delay;
}


static int seq_events_cmp(const void *a, const void *b) {
  uint16_t aa = ((SequenceEvent *)a)->delay_ms;
  uint16_t bb = ((SequenceEvent *)b)->delay_ms;

  return aa < bb ? -1 : (aa > bb ? 1 : 0);
}

SequenceEvent *sequence_compile(SequenceEventPair *pair_seq, uint16_t pair_count, uint16_t *event_count) {
  // Count number of event items
  uint16_t new_count = 0;
  for(uint16_t i = 0; i < pair_count; i++) {
    new_count++;
    if(pair_seq[i].event_end.event != 0)
      new_count++;
  }

  SequenceEvent *new_seq = malloc(new_count * sizeof(SequenceEvent));
  if(!new_seq)
    return NULL;

  // Populate sequence with absolute delay from start
  uint16_t abs_time = 0;
  uint16_t j = 0;
  for(uint16_t i = 0; i < pair_count; i++) {
    new_seq[j] = pair_seq[i].event_begin;
    abs_time += new_seq[j].delay_ms;
    new_seq[j++].delay_ms = abs_time;

    if(pair_seq[i].event_end.event != 0) {
      new_seq[j] = pair_seq[i].event_end;
      new_seq[j++].delay_ms += abs_time;
    }
  }

  // Sort sequence by absolute delay
  qsort(new_seq, new_count, sizeof *new_seq, seq_events_cmp);

  // Convert to relative delay
  abs_time = 0;
  for(uint16_t i = 0; i < new_count; i++) {
    uint16_t next_abs_time = new_seq[i].delay_ms;
    new_seq[i].delay_ms -= abs_time;
    abs_time = next_abs_time;
  }

  *event_count = new_count;
  return new_seq;
}


void sequence_dump(SequenceEvent *seq, uint16_t event_count) {
  char buf[40];
  for(uint16_t i = 0; i < event_count; i++) {
    printf("%4d ms  " PROP_ID "  %s\n", seq[i].delay_ms, seq[i].event,
            prop_get_name(seq[i].event, buf, sizeof buf));
  }
}

