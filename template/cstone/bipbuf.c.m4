divert(-1)
changecom(`@@')
dnl Template parameters:
dnl   T:  Type for expanded template
define(`TN', translit(T, ` ', `_'))
define(`BIP_FIFO_TYPE', `BipFifo_'TN)
define(`BIP_ELEM_TYPE', TN)

divert(0)
// Generated from bipbuf.c.m4 template
//   `T' = T

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

#include `"bipbuf_'TN`.h"'



void `bipfifo_init__'TN (BIP_FIFO_TYPE *fifo, BIP_ELEM_TYPE *buf, BipBufIndex buf_elems) {
  memset(fifo, 0, sizeof *fifo);
  fifo->buf = buf;
  fifo->buf_elems = buf_elems;
}


BipBufIndex `bipfifo_used_elems__'TN (BIP_FIFO_TYPE *fifo) {
  return fifo->reg_a_end - fifo->reg_a_start + fifo->reg_b_end;
};


BipBufIndex `bipfifo_free_elems__'TN (BIP_FIFO_TYPE *fifo) {
  return fifo->buf_elems - `bipfifo_used_elems__'TN (fifo);
};

// Max elems supported by a pop()
BipBufIndex `bipfifo_popable_elems__'TN (BIP_FIFO_TYPE *fifo) {
  return fifo->reg_a_end - fifo->reg_a_start;
};


// Max space available to push()
BipBufIndex `bipfifo_pushable_elems__'TN (BIP_FIFO_TYPE *fifo) {
  if(fifo->reg_b_end > 0) {
    return fifo->reg_a_start - fifo->reg_b_end; // Gap between B and A
  } else { // No region B
    // Choose largest of gap after or before region A
    BipBufIndex before_gap = fifo->reg_a_start;
    BipBufIndex after_gap  = fifo->buf_elems - fifo->reg_a_end;
    return before_gap > after_gap ? before_gap : after_gap;
  }
};


bool `bipfifo_is_empty__'TN (BIP_FIFO_TYPE *fifo) {
  return fifo->reg_a_start == fifo->reg_a_end;
};

bool `bipfifo_is_full__'TN (BIP_FIFO_TYPE *fifo) {
  return `bipfifo_used_elems__'TN (fifo) == fifo->buf_elems;
};

void `bipfifo_flush__'TN (BIP_FIFO_TYPE *fifo) {
  fifo->reg_a_start = 0;
  fifo->reg_a_end   = 0;
  fifo->reg_b_end   = 0;
  fifo->res_start   = 0;
  fifo->res_end     = 0;
};


BipBufIndex `bipfifo_reserved_elems__'TN (BIP_FIFO_TYPE *fifo) {
  return fifo->res_end - fifo->res_start;
}


static inline BipBufIndex space_after_a(BIP_FIFO_TYPE *fifo) {
  return fifo->buf_elems - fifo->reg_a_end;
}

static inline BipBufIndex space_after_b(BIP_FIFO_TYPE *fifo) {
  return fifo->reg_a_start - fifo->reg_b_end;
}


BIP_ELEM_TYPE *`bipfifo_reserve__'TN (BIP_FIFO_TYPE *fifo, BipBufIndex data_elems) {
  if(!fifo->buf || `bipfifo_reserved_elems__'TN (fifo) > 0)
    return NULL;

  // Move region pointers as if push has been performed

  if(fifo->reg_b_end > 0 || space_after_a(fifo) < space_after_b(fifo)) { // Using region B
    if(space_after_b(fifo) < data_elems)
      return NULL;

    fifo->res_start = fifo->reg_b_end;
    fifo->res_end   = fifo->res_start + data_elems;
    fifo->reg_b_end = fifo->res_end;

  } else { // Using region A
    if(space_after_a(fifo) < data_elems)
      return NULL;

    fifo->res_start = fifo->reg_a_end;
    fifo->res_end   = fifo->res_start + data_elems;
    fifo->reg_a_end = fifo->res_end;
  }

  return &fifo->buf[fifo->res_start];
};



void `bipfifo_commit__'TN (BIP_FIFO_TYPE *fifo, BipBufIndex data_elems) {
  BipBufIndex res_elems = `bipfifo_reserved_elems__'TN (fifo);
  if(res_elems == 0)
    return;

  if(data_elems > res_elems)
    data_elems = res_elems;

  if(fifo->res_end == fifo->reg_b_end) { // Using region B
    fifo->reg_b_end = fifo->res_start + data_elems;
  } else {  // Using region A
    fifo->reg_a_end = fifo->res_start + data_elems;
  }

  fifo->res_start = fifo->res_end = 0;

//    DPRINT("A[%d,%d]  B[0,%d] %d", fifo->reg_a_start, fifo->reg_a_end, fifo->reg_b_end, sizeof(BipBufIndex));
  // FIXME: Return res_elems
};


bool `bipfifo_push__'TN (BIP_FIFO_TYPE *fifo, const BIP_ELEM_TYPE *data, BipBufIndex data_elems) {
  void *res_data = `bipfifo_reserve__'TN (fifo, data_elems);

  if(!res_data)
    return false;

//    DPRINT("copy @%d '%s' %d", fifo->res_start, data, data_elems);
  memcpy(res_data, data, data_elems*sizeof(BIP_ELEM_TYPE));
  `bipfifo_commit__'TN (fifo, data_elems);
  return true;
}



BipBufIndex `bipfifo_pop__'TN (BIP_FIFO_TYPE *fifo, BIP_ELEM_TYPE **data, BipBufIndex data_elems) {
  if(data_elems == 0)
    return 0;

  if(`bipfifo_is_empty__'TN (fifo) || fifo->reg_a_start + data_elems > fifo->buf_elems) { // Region A must have data and we can't go beyond end of buffer
    *data = NULL;
    return 0;
  }

  // Pop range can't overlap with reservation
  if(`bipfifo_reserved_elems__'TN (fifo) > 0 && fifo->reg_a_start + data_elems >= fifo->res_start) {
    *data = NULL;
    return 0;
  }


  *data = &fifo->buf[fifo->reg_a_start];

  fifo->reg_a_start += data_elems;

  if(`bipfifo_is_empty__'TN (fifo)) { // Region A is now empty (reserve in A not possible)
    if(fifo->reg_b_end > 0) { // Convert region B into region A
      fifo->reg_a_start = 0;
      fifo->reg_a_end   = fifo->reg_b_end;
      fifo->reg_b_end   = 0;
//      DPUTS("A empty, switch to B");
    } else { // No region B, just reset region A to start of buffer
      fifo->reg_a_start = 0;
      fifo->reg_a_end   = 0;
//      DPUTS("Buffer empty");
    }
  }

  return data_elems;
}


BipBufIndex `bipfifo_peek__'TN (BIP_FIFO_TYPE *fifo, BIP_ELEM_TYPE **data, BipBufIndex data_elems) {
  if(data_elems == 0)
    return 0;

  if(`bipfifo_is_empty__'TN (fifo) || fifo->reg_a_start + data_elems > fifo->buf_elems) { // Region A must have data and we can't go beyond end of buffer

    *data = NULL;
    return 0;
  }

  // Peek range can't overlap with reservation
  if(`bipfifo_reserved_elems__'TN (fifo) > 0 && fifo->reg_a_start + data_elems >= fifo->res_start) {
    *data = NULL;
    return 0;
  }

  *data = &fifo->buf[fifo->reg_a_start];
  return data_elems;
}



BipBufIndex `bipfifo_next_chunk__'TN (BIP_FIFO_TYPE *fifo, BIP_ELEM_TYPE **cur_chunk) {
  if(!cur_chunk)
    return 0;

  if(!*cur_chunk) { // Select region A
    *cur_chunk = &fifo->buf[fifo->reg_a_start];
    return fifo->reg_a_end - fifo->reg_a_start;

  } else if(fifo->reg_b_end > 0 && *cur_chunk == &fifo->buf[fifo->reg_a_start]) { // Select region B
    *cur_chunk = &fifo->buf[0];
    return fifo->reg_b_end;
  }

  // No more chunks
  *cur_chunk = NULL;
  return 0;
}


BipBufIndex `bipfifo_prev_chunk__'TN (BIP_FIFO_TYPE *fifo, BIP_ELEM_TYPE **cur_chunk) {
  if(!cur_chunk)
    return 0;

  if(!*cur_chunk) { // Select last chunk
    if(fifo->reg_b_end > 0) { // Region B
      *cur_chunk = &fifo->buf[0];
      return fifo->reg_b_end;

    } else {  // Region A
      *cur_chunk = &fifo->buf[fifo->reg_a_start];
      return fifo->reg_a_end - fifo->reg_a_start;
    }

  } else if(fifo->reg_b_end > 0 && *cur_chunk == &fifo->buf[0]) { // Select region A
    *cur_chunk = &fifo->buf[fifo->reg_a_start];
    return fifo->reg_a_end - fifo->reg_a_start;
  }

  // No more chunks
  *cur_chunk = NULL;
  return 0;
}


