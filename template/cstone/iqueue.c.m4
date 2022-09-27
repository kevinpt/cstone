divert(-1)
changecom(`@@')
dnl Template parameters:
dnl   T:  Type for expanded template
define(`TN', translit(T, ` ', `_'))
define(`QTYPE', `IQueue_'TN)
divert(0)
// Generated from iqueue.c.m4 template
//   `T' = T

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

#include "cstone/platform.h"
#include `"iqueue_'TN`.h"'


// Modular increment without division
// NOTE: This can be done with bit twiddling for branchless code but experimentation
// shows that GCC and Clang better optimize a ternary and pick branchless opcodes
// where applicable.
#define INC_MODULO(val, mod) ++(val), (val) = ((val) == (mod)) ? 0 : (val)


void `iqueue_init__'TN (QTYPE *q, T *buf, size_t buf_size, bool overwrite) {
  q->queue = buf;
  q->end_ix = buf_size;
  atomic_init(&q->head_ix, 0);
  atomic_init(&q->tail_ix, 0);
  q->overwrite = overwrite;

  memset(buf, 0, sizeof(T) * buf_size);
}


// Allocate a queue with attached array
QTYPE *`iqueue_alloc__'TN (size_t buf_size, bool overwrite) {
  QTYPE *q;
  T *buf;

  // Create a queue followed by its buffer data.
  // Though not needed for bytes, we will pad the queue to get proper alignment
  // for the T array following it.
#define ROUND_UP_ALIGN(n, T)  ((n) + _Alignof(T)-1 - ((n) + _Alignof(T)-1) % _Alignof(T))
  const size_t padded_q_size = ROUND_UP_ALIGN(sizeof *q, T);
  q = cs_malloc(padded_q_size + (buf_size * sizeof *buf));

  if(q) {
    buf = (T *)((uint8_t *)q + padded_q_size);
    `iqueue_init__'TN (q, buf, buf_size, overwrite);
  }

  return q;
}


size_t `iqueue_push_one__'TN (QTYPE *q, const T *element) {
#if 1
  size_t head_ix = atomic_load(&q->head_ix);
  size_t next = head_ix;
  INC_MODULO(next, q->end_ix);

  size_t tail_ix = atomic_load(&q->tail_ix);

  if(next != tail_ix) { // Not full
    memcpy(&q->queue[head_ix], element, sizeof *element);
    atomic_store(&q->head_ix, next);
    return 1;

  } else if (q->overwrite) {
    memcpy(&q->queue[head_ix], element, sizeof *element);
    atomic_store(&q->head_ix, next);

    next = tail_ix;
    INC_MODULO(next, q->end_ix);

    // A reader could have modified q->tail_ix since we loaded it above.
    // If it hasn't incremented then we need to modify it.
    // If it has, then we need to do nothing.
    atomic_compare_exchange_strong(&q->tail_ix, &tail_ix, next);

    return 1;
  }

  return 0;

#else
  return `iqueue_push__'TN (q, element, 1);
#endif
}


size_t `iqueue_pop_one__'TN (QTYPE *q, T *element) {
  size_t tail_ix = atomic_load(&q->tail_ix);
  if(tail_ix != atomic_load(&q->head_ix)) { // Not empty
    *element = q->queue[tail_ix];
    size_t next = tail_ix;
    INC_MODULO(next, q->end_ix);
    atomic_store(&q->tail_ix, next);
    return 1;
  }

  return 0; // Empty queue
}


size_t `iqueue_push__'TN (QTYPE *q, const T *elements, size_t len) {
  size_t copy_size;
  size_t remaining = len;
  while(remaining > 0) {
    size_t head_ix = atomic_load(&q->head_ix);
    size_t tail_ix = atomic_load(&q->tail_ix);
    copy_size = (head_ix >= tail_ix) ? (q->end_ix - head_ix) : (tail_ix-1 - head_ix);
    if(remaining < copy_size)
      copy_size = remaining;

    if(copy_size == 0) // Full
      break;

    memcpy(&q->queue[head_ix], elements, copy_size * sizeof *elements);

    size_t next = head_ix + copy_size;
    if(next >= q->end_ix) { // Wraparound
      if(tail_ix != 0) {
        next = 0;
      } else  { // Full
        // Adjust so that head stays one behind tail
        next = q->end_ix-1;
        copy_size--;
      }
    }

    atomic_store(&q->head_ix, next);

    elements += copy_size;
    remaining -= copy_size;
  }

  if(q->overwrite && remaining > 0) {
    while(remaining > 0) {
      if(`iqueue_push_one__'TN (q, elements) == 0)
        break;
      elements++;
      remaining--;
    }
  }

  return len - remaining;
}


size_t `iqueue_pop__'TN (QTYPE *q, T *elements, size_t elements_size) {
  size_t popped = 0;

  size_t available = `iqueue_count__'TN (q);
  if(elements_size > available)
    elements_size = available;

  while(elements_size > 0) {
    size_t head_ix = atomic_load(&q->head_ix);
    size_t tail_ix = atomic_load(&q->tail_ix);

    // If there are multiple readers we could be empty prematurely
    if(head_ix == tail_ix)  // Empty
      break;

    size_t chunk_size = (head_ix >= tail_ix) ? (head_ix - tail_ix) : (q->end_ix - tail_ix);
    if(chunk_size > elements_size)
      chunk_size = elements_size;

    memcpy(elements, &q->queue[tail_ix], chunk_size * sizeof *elements);
    size_t next = tail_ix + chunk_size;
    if(next >= q->end_ix) // Wraparound
      next = 0;

    atomic_store(&q->tail_ix, next);

    elements += chunk_size;
    popped += chunk_size;
    elements_size -= chunk_size;
  }

  return popped;
}


size_t `iqueue_discard__'TN (QTYPE *q, size_t discard_num) {
  size_t popped = 0;

  size_t available = `iqueue_count__'TN (q);
  if(discard_num > available)
    discard_num = available;

  while(discard_num > 0) {
    size_t head_ix = atomic_load(&q->head_ix);
    size_t tail_ix = atomic_load(&q->tail_ix);

    // If there are multiple readers we could be empty prematurely
    if(head_ix == tail_ix)  // Empty
      break;

    size_t chunk_size = (head_ix >= tail_ix) ? (head_ix - tail_ix) : (q->end_ix - tail_ix);
    if(chunk_size > discard_num)
      chunk_size = discard_num;

    size_t next = tail_ix + chunk_size;
    if(next >= q->end_ix) // Wraparound
      next = 0;

    atomic_store(&q->tail_ix, next);

    popped += chunk_size;
    discard_num -= chunk_size;
  }

  return popped;
}


bool `iqueue_peek_one__'TN (QTYPE *q, T *element) {
  size_t tail_ix = atomic_load(&q->tail_ix);
  if(tail_ix != atomic_load(&q->head_ix)) {
    *element = q->queue[tail_ix];
    return true;
  }

  return false; // Empty queue

}


bool `iqueue_peek_delta__'TN (QTYPE *q, T *element, size_t offset) {
  size_t head_ix = atomic_load(&q->head_ix);
  size_t tail_ix = atomic_load(&q->tail_ix);

  if(tail_ix < head_ix) { // One chunk from tail to head-1
    //   T        H              E
    //  [0][1][2][3][4][5][6][7]
    //   *  *  *

    size_t count = head_ix - tail_ix;
    if(offset >= count)
      return false;

    *element = q->queue[head_ix-1-offset];
    return true;

  } else if(tail_ix > head_ix) { // One or two chunks
    //         H     T           E
    //  [0][1][2][3][4][5][6][7]
    //   *  *        *  *  *  * 

    size_t count = q->end_ix - (tail_ix - head_ix);
    if(offset >= count)
      return false;

    if(offset < head_ix) { // First chunk
      *element = q->queue[head_ix-1-offset];
    } else {  // Second chunk
      offset -= head_ix;
      *element = q->queue[q->end_ix-1-offset];
    }
    return true;
  }

  return false; // Empty queue
}


size_t `iqueue_peek__'TN (QTYPE *q, T **elements) {
  size_t head_ix = atomic_load(&q->head_ix);
  size_t tail_ix = atomic_load(&q->tail_ix);
  if(tail_ix < head_ix) { // One chunk from tail to head-1
    //   T        H              E
    //  [0][1][2][3][4][5][6][7]
    //   *  *  *
    *elements = &q->queue[tail_ix];
    return head_ix - tail_ix;

  } else if(tail_ix > head_ix) { // One or two chunks
    //         H     T           E
    //  [0][1][2][3][4][5][6][7]
    //   *  *        *  *  *  * 
    *elements = &q->queue[tail_ix];
    return q->end_ix - tail_ix;
  }

  // Empty
  *elements = NULL;
  return 0;
}


size_t `iqueue_count__'TN (QTYPE *q) {
  size_t head_ix = atomic_load(&q->head_ix);
  size_t tail_ix = atomic_load(&q->tail_ix);
  return (head_ix >= tail_ix) ? head_ix - tail_ix : q->end_ix - (tail_ix - head_ix);
}


size_t `iqueue_capacity__'TN (QTYPE *q) {
  return q->end_ix;
}


void `iqueue_flush__'TN (QTYPE *q) {
  atomic_store(&q->tail_ix, 0);
  atomic_store(&q->head_ix, 0);
}


bool `iqueue_is_full__'TN (QTYPE *q) {
  size_t next = atomic_load(&q->head_ix);
  INC_MODULO(next, q->end_ix);

  return next == atomic_load(&q->tail_ix); // Full when about to overlap tail
}


bool `iqueue_is_empty__'TN (QTYPE *q) {
  return atomic_load(&q->head_ix) == atomic_load(&q->tail_ix);
}


