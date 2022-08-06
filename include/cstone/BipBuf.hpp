#ifndef BIPBUF_HPP
#define BIPBUF_HPP

#include <stdint.h>
#include <string.h>

#include "MinUInt.hpp"
#include "cstone/debug.h"

/*
This provides two implementations of a bipartite circular buffer (bipbuf) as created
by Simon Cooke. A data block inserted into the buffer is always preserved as a contiguous
chunk. That allows it to be referred by a pointer/length pair and passed to other
functions without concern for wrap around at the end.

The BipDeque acts as a deque with push and pop operations from both ends.

The BipFifo removes the double-ended operations and adds a facility to reserve space in
the buffer so that data can be inserted directly without copying from an intermediate
buffer as with push.


Neither structure is reentrant and can't be safely used from a muti-threaded context
on its own.

https://www.codeproject.com/Articles/3479/The-Bip-Buffer-The-Circular-Buffer-with-a-Twist
*/



template <class T, uintmax_t MAX_ELEM>
class BipDeque {
public:
  typedef T elem_t;
  typedef typename MinUInt<MAX_ELEM>::type index_t;  // Get smallest uint type for K

private:
  T     *m_buf;
  index_t m_buf_elems;

  // Element indices into m_buf
  index_t m_reg_a_start;  // Region A start
  index_t m_reg_a_end;    // Region A end
  index_t m_reg_b_end;    // No need for start since region B always starts at beginning of buffer


  inline index_t space_after_a(void) {
    return m_buf_elems - m_reg_a_end;
  }

  inline index_t space_after_b(void) {
    return m_reg_a_start - m_reg_b_end;
  }


public:

  BipDeque() : m_buf(0), m_buf_elems(0), m_reg_a_start(0), m_reg_a_end(0), m_reg_b_end(0) {};

  ~BipDeque() {};

  void init(T *buf, index_t buf_elems) {
    m_reg_a_start = 0;
    m_reg_a_end   = 0;
    m_reg_b_end   = 0;
    m_buf         = buf;
    m_buf_elems   = buf_elems;
  };

  index_t total_used_elems(void) {
    return m_reg_a_end - m_reg_a_start + m_reg_b_end;
  };

  index_t total_free_elems(void) {
    return m_buf_elems - total_used_elems();
  };

  // Max elems supported by a pop_front()
  index_t used_front_elems(void) {
    return m_reg_a_end - m_reg_a_start;
  };

  // Max elems supported by a pop_back()
  index_t used_back_elems(void) {
    if(m_reg_b_end > 0)
      return m_reg_b_end;
    else
      return m_reg_a_end - m_reg_a_start;
  };


  // Max space available to push_front() and push_back()
  index_t free_elems(void) {
    if(m_reg_b_end > 0) {
      return m_reg_a_start - m_reg_b_end; // Gap between B and A
    } else { // No region B
      // Choose largest of gap after or before region A
      index_t before_gap = m_reg_a_start;
      index_t after_gap  = m_buf_elems - m_reg_a_end;
      return before_gap > after_gap ? before_gap : after_gap;
    }
  };



  bool is_empty(void) {
    return m_reg_a_start == m_reg_a_end;
  };

  bool is_full(void) {
    return total_used_elems() == m_buf_elems;
  };

  void flush(void) {
    m_reg_a_start = 0;
    m_reg_a_end   = 0;
    m_reg_b_end   = 0;
  };

  bool push_back(const T *data, index_t data_elems);
  bool push_front(const T *data, index_t data_elems);

  index_t pop_front(T **data, index_t data_elems);
  index_t peek_front(T **data, index_t data_elems);

  bool pop_back(T **data, index_t data_elems);
  bool peek_back(T **data, index_t data_elems);

  void purge_front(index_t data_elems);
  void purge_back(index_t data_elems);
};



template<class T, uintmax_t MAX_ELEM>
bool BipDeque<T, MAX_ELEM>::push_back(const T *data, index_t data_elems) {
  if(m_buf == NULL)
    return false;

  if(m_reg_b_end > 0 || space_after_a() < space_after_b()) { // Using region B
    if(space_after_b() < data_elems)
      return false;

    memcpy(&m_buf[m_reg_b_end], data, data_elems*sizeof(T));
    m_reg_b_end += data_elems;

  } else { // Using region A
    if(space_after_a() < data_elems)
      return false;

    memcpy(&m_buf[m_reg_a_end], data, data_elems*sizeof(T));
    m_reg_a_end += data_elems;
  }

  return true;
}


template<class T, uintmax_t MAX_ELEM>
bool BipDeque<T, MAX_ELEM>::push_front(const T *data, index_t data_elems) {
  if(m_buf == NULL)
    return false;

  // Check if region A is at the start of the buffer
  if(m_reg_a_start == 0) { // Implies no region B
    if(space_after_a() < data_elems)
      return false;

    // Convert A region to B region and make new A at end of buffer
    m_reg_b_end   = m_reg_a_end;
    m_reg_a_start = m_buf_elems - data_elems;
    m_reg_a_end   = m_buf_elems;

  } else { // Region A is not at start of buffer and region B may exist
    if(space_after_b() < data_elems) // Check if there is space between region B and A
      return false;

    // Append data to front of region A
    m_reg_a_start -= data_elems;
  }

  memcpy(&m_buf[m_reg_a_start], data, data_elems*sizeof(T));

  return true;
}


template<class T, uintmax_t MAX_ELEM>
typename BipDeque<T, MAX_ELEM>::index_t BipDeque<T, MAX_ELEM>::pop_front(T **data, index_t data_elems) {
  if(data_elems == 0)
    return 0;

  if(is_empty() || m_reg_a_start + data_elems > m_buf_elems) { // Region A must have data and we can't go beyond end of buffer
    *data = NULL;
    return 0;
  }

  *data = &m_buf[m_reg_a_start];

  m_reg_a_start += data_elems;

  if(is_empty()) { // Region A is now empty
    if(m_reg_b_end > 0) { // Convert region B into region A
      m_reg_a_start = 0;
      m_reg_a_end   = m_reg_b_end;
      m_reg_b_end   = 0;
    } else { // No region B, just reset region A to start of buffer
      m_reg_a_start = 0;
      m_reg_a_end   = 0;
    }
  }

  return data_elems;
}


template<class T, uintmax_t MAX_ELEM>
typename BipDeque<T, MAX_ELEM>::index_t BipDeque<T, MAX_ELEM>::peek_front(T **data, index_t data_elems) {
  if(data_elems == 0)
    return 0;

  if(is_empty() || m_reg_a_start + data_elems > m_buf_elems) { // Region A must have data and we can't go beyond end of buffer
    *data = NULL;
    return 0;
  }

  *data = &m_buf[m_reg_a_start];
  return data_elems;
}


template<class T, uintmax_t MAX_ELEM>
bool BipDeque<T, MAX_ELEM>::pop_back(T **data, index_t data_elems) {
  if(m_reg_b_end > 0) { // If region B exists
    if(data_elems > m_reg_b_end) {
      *data = NULL;
      return false;
    }

    m_reg_b_end -= data_elems;
    *data = &m_buf[m_reg_b_end];
    return true;

  } else { // Check region A
    if(data_elems > (m_reg_a_end - m_reg_a_start)) {
      *data = NULL;
      return false;
    }

    m_reg_a_end -= data_elems;
    *data = &m_buf[m_reg_a_end];

    if(m_reg_a_end == m_reg_a_start) { // Reset region A to start of buffer
      m_reg_a_start = 0;
      m_reg_a_end   = 0;
    }

    return true;
  }
}


template<class T, uintmax_t MAX_ELEM>
bool BipDeque<T, MAX_ELEM>::peek_back(T **data, index_t data_elems) {

  if(m_reg_b_end > 0) { // If region B exists
    if(data_elems > m_reg_b_end) {
      *data = NULL;
      return false;
    }

    *data = &m_buf[m_reg_b_end - data_elems];
    return true;

  } else { // Check region A
      if(data_elems > (m_reg_a_end - m_reg_a_start)) {
        *data = NULL;
        return false;
      }

      *data = &m_buf[m_reg_a_end - data_elems];
      return true;
  }
}


template<class T, uintmax_t MAX_ELEM>
void BipDeque<T, MAX_ELEM>::purge_front(index_t data_elems) {
  T *discard;

  index_t available = free_elems();

  while(!is_empty() && available < data_elems) {
    pop_front(&discard, data_elems - available);
    available = free_elems();
  }
}


template<class T, uintmax_t MAX_ELEM>
void BipDeque<T, MAX_ELEM>::purge_back(index_t data_elems) {
  T *discard;

  index_t available = free_elems();

  while(!is_empty() && available < data_elems) {
    pop_back(&discard, data_elems - available);
    available = free_elems();
  }
}







/*
BipFifo is a restricted form of BipQueue that only permits push and pop from
back and front respectively. It adds the ability to reserve a block of memory
so that a copy isn't necessary as with push_back(). When a reservation is active
no new data can be added until it is committed or discarded.

Empty     [....................]
Push 8    [AAAAAAAA............]
Pop 4     [....AAAA............]
Push 10   [....AAAAAAAAAAAAAA..]
Push 3    [BBB.AAAAAAAAAAAAAA..] Gap after A not big enough; Wrap to front and start B
Pop 6     [BBB.......AAAAAAAA..]
Pop 8     [AAA.................] Region B converted to A

*/
template <class T, uintmax_t MAX_ELEM>
class BipFifo {

public:
  typedef T elem_t;

#if 1
  typedef typename MinUInt<MAX_ELEM>::type index_t;  // Get smallest uint type for K

#else
  // Wrapped integer implementation to help tracking down bugs with boundary conditions
  template <int MAX_VAL>
  class ranged_uint {
  public:
    int32_t val;

    ranged_uint(int32_t v = 0) : val(v) {};


    ranged_uint operator+(const ranged_uint &rhs) {
      int32_t sum = val + rhs.val;
      if(sum > MAX_VAL) {
        DPRINT("ranged_uint overflow: %d + %d = %d", val, rhs.val, sum);
        sum = sum % (MAX_VAL+1);
      }

      return ranged_uint(sum);
    };

    ranged_uint operator-(const ranged_uint &rhs) {
      int32_t sub = val - rhs.val;
      if(sub < 0) {
        DPRINT("ranged_uint underflow: %d - %d = %d", val, rhs.val, sub);
        sub = sub % (MAX_VAL+1);
      }

      return ranged_uint(sub);
    };

    ranged_uint &operator+=(const ranged_uint &rhs) {
      *this = *this + rhs;
      return *this;
    };

    ranged_uint &operator-=(const ranged_uint &rhs) {
      *this = *this - rhs;
      return *this;
    };


    ranged_uint &operator++() { // Prefix ++
      val += 1;
      return *this;
    };

    ranged_uint operator++(int) { // Postfix ++
       ranged_uint result(*this);
       ++(*this);
       return result;
    }

    ranged_uint &operator--() { // Prefix --
      val -= 1;
      return *this;
    };

    ranged_uint operator--(int) { // Postfix --
       ranged_uint result(*this);
       --(*this);
       return result;
    }

    operator int32_t() const { return val; };

  };

  typedef ranged_uint<(1ul << (sizeof(typename MinUInt<MAX_ELEM>::type) * 8)) - 1> index_t;
#endif

private:
  T     *m_buf;
  index_t m_buf_elems;

  // Element indices into m_buf
  index_t m_reg_a_start;  // Region A start
  index_t m_reg_a_end;    // Region A end
  index_t m_reg_b_end;    // No need for start since region B always starts at beginning of buffer
  index_t m_res_start;    // Reserve start
  index_t m_res_end;      // Reserve end

  inline index_t space_after_a(void) {
    return m_buf_elems - m_reg_a_end;
  }

  inline index_t space_after_b(void) {
    return m_reg_a_start - m_reg_b_end;
  }


public:

  BipFifo() : m_buf(0), m_buf_elems(0), m_reg_a_start(0), m_reg_a_end(0), m_reg_b_end(0),
              m_res_start(0), m_res_end(0) {};

  ~BipFifo() {};

  void init(T *buf, index_t buf_elems) {
    m_reg_a_start = 0;
    m_reg_a_end   = 0;
    m_reg_b_end   = 0;
    m_res_start   = 0;
    m_res_end     = 0;
    m_buf         = buf;
    m_buf_elems   = buf_elems;
  };

#if 0
  void debug(void) {
    pprintf("## A[%d,%d]  B[0,%d] %d\n", m_reg_a_start, m_reg_a_end, m_reg_b_end, sizeof(index_t));
  }
#endif

  index_t total_used_elems(void) {
    return m_reg_a_end - m_reg_a_start + m_reg_b_end;
  };

  index_t total_free_elems(void) {
    return m_buf_elems - total_used_elems();
  };

  // Max elems supported by a pop()
  index_t num_block_elems(void) {
    return m_reg_a_end - m_reg_a_start;
  };


  // Max space available to push()
  index_t free_elems(void) {
    if(m_reg_b_end > 0) {
      return m_reg_a_start - m_reg_b_end; // Gap between B and A
    } else { // No region B
      // Choose largest of gap after or before region A
      index_t before_gap = m_reg_a_start;
      index_t after_gap  = m_buf_elems - m_reg_a_end;
      return before_gap > after_gap ? before_gap : after_gap;
    }
  };


  bool is_empty(void) {
    return m_reg_a_start == m_reg_a_end;
  };

  bool is_full(void) {
    return total_used_elems() == m_buf_elems;
  };

  void flush(void) {
    m_reg_a_start = 0;
    m_reg_a_end   = 0;
    m_reg_b_end   = 0;
    m_res_start   = 0;
    m_res_end     = 0;
  };


  index_t reserved_elems(void) {
    return m_res_end - m_res_start;
  }


  T *reserve(index_t data_elems) {
    if(!m_buf || reserved_elems() > 0)
      return NULL;

    // Move region pointers as if push has been performed

    if(m_reg_b_end > 0 || space_after_a() < space_after_b()) { // Using region B
      if(space_after_b() < data_elems)
        return NULL;

      m_res_start = m_reg_b_end;
      m_res_end   = m_res_start + data_elems;
      m_reg_b_end = m_res_end;

    } else { // Using region A
      if(space_after_a() < data_elems)
        return NULL;

      m_res_start = m_reg_a_end;
      m_res_end   = m_res_start + data_elems;
      m_reg_a_end = m_res_end;
    }

    return &m_buf[m_res_start];
  };



  void commit(index_t data_elems) {
    if(reserved_elems() == 0)
      return;

    index_t res_elems = reserved_elems();
    if(data_elems > res_elems)
      data_elems = res_elems;

    if(m_res_end == m_reg_b_end) { // Using region B
      m_reg_b_end = m_res_start + data_elems;
    } else {  // Using region A
      m_reg_a_end = m_res_start + data_elems;
    }

    m_res_start = m_res_end = 0;

//    DPRINT("A[%d,%d]  B[0,%d] %d", m_reg_a_start, m_reg_a_end, m_reg_b_end, sizeof(index_t));
  };


  bool push(const T *data, index_t data_elems) {
    void *res_data = reserve(data_elems);

    if(!res_data)
      return false;

//    DPRINT("copy @%d '%s' %d", m_res_start, data, data_elems);
    memcpy(res_data, data, data_elems*sizeof(T));
    commit(data_elems);
    return true;
  }


  index_t pop(T **data, index_t data_elems);
  index_t peek(T **data, index_t data_elems);
  index_t next_chunk(T **cur_chunk);
  index_t prev_chunk(T **cur_chunk);
};


template<class T, uintmax_t MAX_ELEM>
typename BipFifo<T, MAX_ELEM>::index_t BipFifo<T, MAX_ELEM>::pop(T **data, index_t data_elems) {
  if(data_elems == 0)
    return 0;

  if(is_empty() || m_reg_a_start + data_elems > m_buf_elems) { // Region A must have data and we can't go beyond end of buffer
    *data = NULL;
    return 0;
  }

  // Pop range can't overlap with reservation
  if(reserved_elems() > 0 && m_reg_a_start + data_elems >= m_res_start) {
    *data = NULL;
    return 0;
  }


  *data = &m_buf[m_reg_a_start];

  m_reg_a_start += data_elems;

  if(is_empty()) { // Region A is now empty (reserve in A not possible)
    if(m_reg_b_end > 0) { // Convert region B into region A
      m_reg_a_start = 0;
      m_reg_a_end   = m_reg_b_end;
      m_reg_b_end   = 0;
//      DPUTS("A empty, switch to B");
    } else { // No region B, just reset region A to start of buffer
      m_reg_a_start = 0;
      m_reg_a_end   = 0;
//      DPUTS("Buffer empty");
    }
  }

  return data_elems;
}


template<class T, uintmax_t MAX_ELEM>
typename BipFifo<T, MAX_ELEM>::index_t BipFifo<T, MAX_ELEM>::peek(T **data, index_t data_elems) {
  if(data_elems == 0)
    return 0;

  if(is_empty() || m_reg_a_start + data_elems > m_buf_elems) { // Region A must have data and we can't go beyond end of buffer

    *data = NULL;
    return 0;
  }

  // Peek range can't overlap with reservation
  if(reserved_elems() > 0 && m_reg_a_start + data_elems >= m_res_start) {
    *data = NULL;
    return 0;
  }

  *data = &m_buf[m_reg_a_start];
  return data_elems;
}



template<class T, uintmax_t MAX_ELEM>
typename BipFifo<T, MAX_ELEM>::index_t BipFifo<T, MAX_ELEM>::next_chunk(T **cur_chunk) {
  if(!cur_chunk)
    return 0;

  if(!*cur_chunk) { // Select region A
    *cur_chunk = &m_buf[m_reg_a_start];
    return m_reg_a_end - m_reg_a_start;

  } else if(m_reg_b_end > 0 && *cur_chunk == &m_buf[m_reg_a_start]) { // Select region B
    *cur_chunk = &m_buf[0];
    return m_reg_b_end;
  }

  // No more chunks
  *cur_chunk = NULL;
  return 0;
}


template<class T, uintmax_t MAX_ELEM>
typename BipFifo<T, MAX_ELEM>::index_t BipFifo<T, MAX_ELEM>::prev_chunk(T **cur_chunk) {
  if(!cur_chunk)
    return 0;

  if(!*cur_chunk) { // Select last chunk
    if(m_reg_b_end > 0) { // Region B
      *cur_chunk = &m_buf[0];
      return m_reg_b_end;

    } else {  // Region A
      *cur_chunk = &m_buf[m_reg_a_start];
      return m_reg_a_end - m_reg_a_start;
    }

  } else if(m_reg_b_end > 0 && *cur_chunk == &m_buf[0]) { // Select region A
    *cur_chunk = &m_buf[m_reg_a_start];
    return m_reg_a_end - m_reg_a_start;
  }

  // No more chunks
  *cur_chunk = NULL;
  return 0;
}


#endif // BIPBUF_HPP
