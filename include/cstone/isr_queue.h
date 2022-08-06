#ifndef ISR_QUEUE_H
#define ISR_QUEUE_H

#ifdef __cplusplus
#  include <atomic>
// NOTE: C++23 has the C11 stdatomic types so this isn't necessary in the future
typedef std::atomic<size_t> a_size_t;
#else
#  include <stdatomic.h>
typedef atomic_size_t a_size_t;
#endif

typedef  uint8_t QueueDatum;

typedef struct {
  QueueDatum *queue;
  size_t      end_ix;     // One past end of queue

  volatile a_size_t head_ix;  // Always an unused sentinel
  volatile a_size_t tail_ix;

  bool        overwrite;  // Overwrite old data when full
} IsrQueue;


#ifdef __cplusplus
extern "C" {
#endif


void isr_queue_init(IsrQueue *q, QueueDatum *buf, size_t buf_size, bool overwrite);
IsrQueue *isr_queue_alloc(size_t buf_size, bool overwrite);

size_t isr_queue_push_one(IsrQueue *q, const QueueDatum *element);
size_t isr_queue_pop_one(IsrQueue *q, QueueDatum *element);
size_t isr_queue_push(IsrQueue *q, const QueueDatum *elements, size_t len);
size_t isr_queue_pop(IsrQueue *q, QueueDatum *elements, size_t elements_size);
size_t isr_queue_discard(IsrQueue *q, size_t discard_num);
bool isr_queue_peek_one(IsrQueue *q, QueueDatum *element);
size_t isr_queue_peek(IsrQueue *q, QueueDatum **elements);

size_t isr_queue_count(IsrQueue *q);
void isr_queue_flush(IsrQueue *q);
bool isr_queue_is_full(IsrQueue *q);
bool isr_queue_is_empty(IsrQueue *q);


#ifdef __cplusplus
}
#endif

#endif // ISR_QUEUE_H
