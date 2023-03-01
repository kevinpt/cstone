divert(-1)
changecom(`@@')
dnl Template parameters:
dnl   T:  Type for expanded template
define(`TN', translit(T, ` ', `_'))
define(`GUARD', `BIPBUF_'TN`_H')
define(`BIP_FIFO_TYPE', `BipFifo_'TN)
define(`BIP_ELEM_TYPE', TN)
divert(0)dnl
#ifndef GUARD
#define GUARD


// Generated from bipbuf.h.m4 template
//   `T' = T

typedef uint16_t BipBufIndex;

typedef struct {
  BIP_ELEM_TYPE  *buf;
  BipBufIndex     buf_elems;
  
  // Element indices into m_buf
  BipBufIndex reg_a_start;  // Region A start
  BipBufIndex reg_a_end;    // Region A end
  BipBufIndex reg_b_end;    // No need for start since region B always starts at beginning of buffer
  BipBufIndex res_start;    // Reserve start
  BipBufIndex res_end;      // Reserve end

} BIP_FIFO_TYPE;


#ifdef __cplusplus
extern "C" {
#endif

void `bipfifo_init__'TN (BIP_FIFO_TYPE *fifo, BIP_ELEM_TYPE *buf, BipBufIndex buf_elems);
BipBufIndex `bipfifo_used_elems__'TN (BIP_FIFO_TYPE *fifo);
BipBufIndex `bipfifo_free_elems__'TN (BIP_FIFO_TYPE *fifo);
BipBufIndex `bipfifo_popable_elems__'TN (BIP_FIFO_TYPE *fifo);
BipBufIndex `bipfifo_pushable_elems__'TN (BIP_FIFO_TYPE *fifo);
bool `bipfifo_is_empty__'TN (BIP_FIFO_TYPE *fifo);
bool `bipfifo_is_full__'TN (BIP_FIFO_TYPE *fifo);
void `bipfifo_flush__'TN (BIP_FIFO_TYPE *fifo);
BipBufIndex `bipfifo_reserved_elems__'TN (BIP_FIFO_TYPE *fifo);
BIP_ELEM_TYPE *`bipfifo_reserve__'TN (BIP_FIFO_TYPE *fifo, BipBufIndex data_elems);
void `bipfifo_commit__'TN (BIP_FIFO_TYPE *fifo, BipBufIndex data_elems);
bool `bipfifo_push__'TN (BIP_FIFO_TYPE *fifo, const BIP_ELEM_TYPE *data, BipBufIndex data_elems);
BipBufIndex `bipfifo_pop__'TN (BIP_FIFO_TYPE *fifo, BIP_ELEM_TYPE **data, BipBufIndex data_elems);
BipBufIndex `bipfifo_peek__'TN (BIP_FIFO_TYPE *fifo, BIP_ELEM_TYPE **data, BipBufIndex data_elems);
BipBufIndex `bipfifo_next_chunk__'TN (BIP_FIFO_TYPE *fifo, BIP_ELEM_TYPE **cur_chunk);
BipBufIndex `bipfifo_prev_chunk__'TN (BIP_FIFO_TYPE *fifo, BIP_ELEM_TYPE **cur_chunk);

#ifdef __cplusplus
}
#endif


#endif // GUARD
