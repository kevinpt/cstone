#ifndef SEQUENCE_EVENTS_H
#define SEQUENCE_EVENTS_H

typedef unsigned  SequenceTime;

typedef struct {
  uint32_t event;
  uint16_t delay_ms;
  uint16_t payload;
} SequenceEvent;

typedef struct {
  SequenceEvent event_begin;  // delay_ms field is relative to prev event_begin
  SequenceEvent event_end;    // delay_ms field is relative to this event_begin
} SequenceEventPair;

struct Sequence;

typedef void (*SequenceCompletion)(struct Sequence *seq);

typedef struct Sequence {
  struct Sequence *next;
  SequenceCompletion  complete;
  SequenceEvent *events;
  SequenceTime  timestamp; // Time for next event
  uint16_t event_count; // Items in events
  uint16_t cur_event;   // Index into events
  uint8_t  repeats;

} Sequence;


#ifdef __cplusplus
extern "C" {
#endif

// You must provide implementation of this function
SequenceTime sequence_timestamp(void);


void sequence_init(Sequence *seq, SequenceEvent *events, uint16_t event_count, uint8_t repeats,
                        SequenceCompletion complete);
bool sequence_init_pairs(Sequence *seq, SequenceEventPair *event_pairs, uint16_t pair_count, uint8_t repeats,
                    SequenceCompletion complete);

void sequence_configure(Sequence *seq, SequenceEvent *events, uint16_t event_count, uint8_t repeats,
                        SequenceCompletion complete);

void sequence_start(Sequence *seq, uint8_t repeats);
void sequence_restart(Sequence *seq, uint8_t repeats);
bool sequence_is_active(Sequence *seq);
void sequence_add(Sequence *seq);
void sequence_remove(Sequence *seq);
SequenceTime sequence_update_all(void);

SequenceEvent *sequence_compile(SequenceEventPair *pair_seq, uint16_t pair_count, uint16_t *event_count);
void sequence_dump(SequenceEvent *seq, uint16_t event_count);

#ifdef __cplusplus
}
#endif

#endif // SEQUENCE_EVENTS_H
