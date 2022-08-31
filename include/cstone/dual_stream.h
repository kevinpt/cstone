#ifndef DUAL_STREAM_H
#define DUAL_STREAM_H

typedef struct DualStream DualStream;

typedef void (*DualStreamIOSend)(DualStream *stream);

typedef struct {
  IsrQueue          *tx_queue;
  IsrQueue          *rx_queue;
  DualStreamIOSend    io_send;   // Notify interface that TX data is available
  void               *io_ctx;
} DualStreamConfig;

struct DualStream {
  IsrQueue           *tx_queue;
  IsrQueue           *rx_queue;
  SemaphoreHandle_t   tx_lock;
  SemaphoreHandle_t   rx_lock;

  SemaphoreHandle_t   tx_empty; // Signal blocking print routines when it is safe to proceed

  DualStreamIOSend    io_send;   // Notify interface that TX data is available
  void               *io_ctx;
};


#ifdef __cplusplus
extern "C" {
#endif

void dualstream_init(DualStream *stream, DualStreamConfig *cfg);

#ifdef __cplusplus
}
#endif


#endif // DUAL_STREAM_H
