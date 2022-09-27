divert(-1)
changecom(`@@')
dnl Template parameters:
dnl   T:  Type for expanded template
define(`TN', translit(T, ` ', `_'))
define(`GUARD', `IQ_'TN`_H')
define(`QTYPE', `IQueue_'TN)
divert(0)dnl
#ifndef GUARD
#define GUARD

// Generated from iqueue.h.m4 template
//   `T' = T

#ifdef __cplusplus
#  include <atomic>
// NOTE: C++23 has the C11 stdatomic types so this isn't necessary in the future
typedef std::atomic<size_t> a_size_t;
#else
#  include <stdatomic.h>
typedef atomic_size_t a_size_t;
#endif


typedef struct {
  T        *queue;
  size_t    end_ix;     // One past end of queue

  volatile a_size_t head_ix;  // Always an unused sentinel
  volatile a_size_t tail_ix;

  bool      overwrite;  // Overwrite old data when full
} QTYPE;


#ifdef __cplusplus
extern "C" {
#endif


void `iqueue_init__'TN (QTYPE *q, T *buf, size_t buf_size, bool overwrite);
QTYPE *`iqueue_alloc__'TN (size_t buf_size, bool overwrite);

size_t `iqueue_push_one__'TN (QTYPE *q, const T *element);
size_t `iqueue_pop_one__'TN (QTYPE *q, T *element);
size_t `iqueue_push__'TN (QTYPE *q, const T *elements, size_t len);
size_t `iqueue_pop__'TN (QTYPE *q, T *elements, size_t elements_size);
size_t `iqueue_discard__'TN (QTYPE *q, size_t discard_num);
bool   `iqueue_peek_one__'TN (QTYPE *q, T *element);
bool   `iqueue_peek_delta__'TN (QTYPE *q, T *element, size_t offset);
size_t `iqueue_peek__'TN (QTYPE *q, T **elements);

size_t `iqueue_count__'TN (QTYPE *q);
size_t `iqueue_capacity__'TN (QTYPE *q);
void   `iqueue_flush__'TN (QTYPE *q);
bool   `iqueue_is_full__'TN (QTYPE *q);
bool   `iqueue_is_empty__'TN (QTYPE *q);


#ifdef __cplusplus
}
#endif

#endif // GUARD
