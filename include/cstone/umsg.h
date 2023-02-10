#ifndef UMSG_H
#define UMSG_H

#include "FreeRTOS.h"
#include "queue.h"

typedef struct {
  uint32_t  id;       // Prop id for type of message

  union {
    uint32_t source;  // Sender of message
    uint32_t param32;
    uint16_t param16[2];
  };

  uintptr_t payload;  // Optional value associated with the message
  size_t    payload_size; // payload is a pointer when size > 0
} UMsg;

#define NO_TIMEOUT        0
#define INFINITE_TIMEOUT  UINT32_MAX

#define UMSG_FILTERS_IN_CHUNK  4

#define P_RSRC_SYS_LOCAL_TASK       (P1_RSRC | P2_SYS | P3_LOCAL | P4_TASK)
#define P_RSRC_HW_LOCAL_TASK        (P1_RSRC | P2_HW  | P3_LOCAL | P4_TASK)

#define P_ERROR_SYS_MESSAGE_TIMEOUT (P1_ERROR | P2_SYS | P3_MESSAGE | P4_TIMEOUT)

typedef struct UMsgFilterChunk {
  struct UMsgFilterChunk *next;
  uint32_t filters[UMSG_FILTERS_IN_CHUNK];
} UMsgFilterChunk;


typedef struct UMsgTarget UMsgTarget;

typedef void(*UMsgTargetCallback)(UMsgTarget *tgt, UMsg *msg);

// A target receives messages either into a queue for deferred processing or
// directly handled via callback.
struct UMsgTarget {
  struct UMsgTarget  *next;
  UMsgFilterChunk    *filter_chunks;  // List of filter masks for this target
  uintptr_t           user_data;
  QueueHandle_t       q;
  UMsgTargetCallback  msg_handler_cb;
  unsigned            dropped_messages;
};


// A hub is a target with a subscriber list of other targets
typedef struct {
  UMsgTarget inbox;

  UMsgTarget *subscribers;
} UMsgHub;


#ifdef __cplusplus
extern "C" {
#endif

void umsg_tgt_queued_init(UMsgTarget *tgt, size_t max_msg);
void umsg_tgt_callback_init(UMsgTarget *tgt, UMsgTargetCallback msg_handler_cb);

void umsg_tgt_free(UMsgTarget *tgt);
bool umsg_tgt_add_filter(UMsgTarget *tgt, uint32_t filter_mask);
bool umsg_tgt_remove_filter(UMsgTarget *tgt, uint32_t filter_mask);

bool umsg_tgt_send(UMsgTarget *tgt, UMsg *msg, uint32_t timeout);
bool umsg_tgt_recv(UMsgTarget *tgt, UMsg *msg, uint32_t timeout);

void umsg_discard(UMsg *msg);

void umsg_hub_init(UMsgHub *hub, size_t max_msg);
void umsg_hub_free(UMsgHub *hub);
void umsg_set_sys_hub(UMsgHub *hub);
UMsgHub *umsg_sys_hub(void);

void umsg_hub_subscribe(UMsgHub *hub, UMsgTarget *subscriber);
bool umsg_hub_unsubscribe(UMsgHub *hub, UMsgTarget *subscriber);
void umsg_hub_process_inbox(UMsgHub *hub, uint32_t send_timeout);
#define umsg_hub_send(hub, msg, to) umsg_tgt_send((UMsgTarget *)hub, msg, to)
bool umsg_hub_query(UMsgHub *hub, uint32_t query_id, uintptr_t *response, uint32_t timeout);

bool report_event(uint32_t id, uintptr_t data);
#define report_error(id, data)  report_event((id), (data))

#ifdef __cplusplus
}
#endif

#endif // UMSG_H
