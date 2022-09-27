#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

#include "FreeRTOS.h"
#include "queue.h"

#include "cstone/prop_id.h"
#include "cstone/umsg.h"
#include "util/mempool.h"
#include "util/list_ops.h"


extern unsigned long millis(void);


/*
Initialize a message target with queued processing

Args:
  tgt:      Target to init
  max_msg:  Number of messages in the queue
*/
void umsg_tgt_queued_init(UMsgTarget *tgt, size_t max_msg) {
  memset(tgt, 0, sizeof(*tgt));
  if(max_msg > 0)
    tgt->q = xQueueCreate(max_msg, sizeof(UMsg));
}


/*
Initialize a message target with callback processing

Args:
  tgt:      Target to init
  callback: Callback to run for each message received
*/
void umsg_tgt_callback_init(UMsgTarget *tgt, UMsgTargetCallback msg_handler_cb) {
  memset(tgt, 0, sizeof(*tgt));
  tgt->msg_handler_cb = msg_handler_cb;
}


/*
Free a target's resources

Args:
  tgt:      Target to free
*/
void umsg_tgt_free(UMsgTarget *tgt) {
  if(tgt->q) {
    vQueueDelete(tgt->q);
    tgt->q = 0;
  }

  while(tgt->filter_chunks) {
    UMsgFilterChunk *cur_chunk = LL_NODE(ll_slist_pop(&tgt->filter_chunks), UMsgFilterChunk, next);
    mp_free(mp_sys_pools(), cur_chunk);
  }
}



static uint32_t *umsg__find_filter_slot(UMsgTarget *tgt, uint32_t filter_mask) {
  UMsgFilterChunk *cur;
  for(cur = tgt->filter_chunks; cur; cur = cur->next) {
    for(int i = 0; i < UMSG_FILTERS_IN_CHUNK; i++) {
      if(cur->filters[i] == filter_mask)
        return &cur->filters[i];
    }
  }

  return NULL;
}


static inline uint32_t *umsg__find_empty_filter_slot(UMsgTarget *tgt) {
  return umsg__find_filter_slot(tgt, 0);
}


/*
Add a new mesage filter to a target

Args:
  tgt:          Target for the message filter
  filter_mask:  Prop mask for messages to accept

Returns:
  true on success
*/
bool umsg_tgt_add_filter(UMsgTarget *tgt, uint32_t filter_mask) {
  // Check if filter already exists
  uint32_t *filter_slot = umsg__find_filter_slot(tgt, filter_mask);
  if(filter_slot)
    return true;

  // Find an unused filter slot
  filter_slot = umsg__find_empty_filter_slot(tgt);

  if(!filter_slot) {  // Add a new chunk
    UMsgFilterChunk *new_chunk = mp_alloc(mp_sys_pools(), sizeof(UMsgFilterChunk), NULL);
    if(!new_chunk)
      return false;

    memset(new_chunk, 0, sizeof(*new_chunk));
    ll_slist_push(&tgt->filter_chunks, new_chunk);
    filter_slot = &new_chunk->filters[0];
  }

  if(filter_slot) {
    *filter_slot = filter_mask;
    return true;
  }

  return false;
}


/*
Remove a mesage filter from a target

Args:
  tgt:          Target for the message filter
  filter_mask:  Prop mask to remove

Returns:
  true on success
*/
bool umsg_tgt_remove_filter(UMsgTarget *tgt, uint32_t filter_mask) {
  uint32_t *filter_slot = umsg__find_filter_slot(tgt, filter_mask);

  if(filter_slot) {
    *filter_slot = 0;
    return true;
  }

  return false;
}


// Scan all filters in a targer for a match against prop_id
static bool umsg__tgt_match_filter(UMsgTarget *tgt, uint32_t prop_id) {
  UMsgFilterChunk *cur;
  for(cur = tgt->filter_chunks; cur; cur = cur->next) {
    for(int i = 0; i < UMSG_FILTERS_IN_CHUNK; i++) {
      if(cur->filters[i] == 0)
        continue;

      if(prop_match(prop_id, cur->filters[i]))
        return true;
    }
  }

  return false;
}


/*
Send a message to a target

The message object will be copied and does not need to be preserved.
Set timeout param to NO_TIMEOUT to fail immediately when message can't be sent.
Set it to INFINITE_TIMEOUT to block indefinitely until a message can be sent.

Args:
  tgt:      Target for the message
  msg:      Message to send
  timeout:  Timeout in milliseconds

Returns:
  true on success
*/
bool umsg_tgt_send(UMsgTarget *tgt, UMsg *msg, uint32_t timeout) {
  TickType_t timeout_ticks;

  timeout_ticks = (timeout == INFINITE_TIMEOUT) ? portMAX_DELAY : pdMS_TO_TICKS(timeout);
  bool status = xQueueSend(tgt->q, msg, timeout_ticks) == pdTRUE;

  if(!status) { // Timeout
taskENTER_CRITICAL();
    tgt->dropped_messages++;
taskEXIT_CRITICAL();
  }

  return status;
}


/*
Receive the next message from a target

This only applies to targets configured for queued message reception.

Set timeout param to NO_TIMEOUT to fail immediately when no message is available.
Set it to INFINITE_TIMEOUT to block indefinitely until a message is ready.

Args:
  tgt:      Target to read from
  msg:      Received message
  timeout:  Timeout in milliseconds

Returns:
  true on success
*/
bool umsg_tgt_recv(UMsgTarget *tgt, UMsg *msg, uint32_t timeout) {
  TickType_t timeout_ticks;

  timeout_ticks = (timeout == INFINITE_TIMEOUT) ? portMAX_DELAY : pdMS_TO_TICKS(timeout);
  return xQueueReceive(tgt->q, msg, timeout_ticks) == pdTRUE;
}


/*
Discard data allocated for a message

If a message has a non-zero length payload it will be freed using the system
memory pools.

Args:
  msg:  Message to discard
*/
void umsg_discard(UMsg *msg) {
  if(msg->payload_size > 0 && msg->payload)
    mp_free(mp_sys_pools(), (void *)msg->payload);
}



static UMsgHub *s_main_sys_hub = NULL;

/*
Initialize a message hub

A hub is always configured as a target with queued message input.
The first hub initialized will be set as a system wide "main" hub that
can be retrieved with :c:func:`umsg_sys_hub`.

Args:
  hub:      Message hub to init
  max_msg:  Number of messages in the queue
*/
void umsg_hub_init(UMsgHub *hub, size_t max_msg) {
  umsg_tgt_queued_init(&hub->inbox, max_msg);
  hub->subscribers = NULL;

  if(!s_main_sys_hub) // First hub becomes main by default
    s_main_sys_hub = hub;
}


/*
Free a hub's resources

Args:
  hub:  Message hub to free
*/
void umsg_hub_free(UMsgHub *hub) {
  umsg_tgt_free(&hub->inbox);
}


/*
Set the main system message hub

This hub will be returned by :c:func:`umsg_sys_hub`.

Args:
  hub:  Message hub to use as the system hub
*/
void umsg_set_sys_hub(UMsgHub *hub) {
  s_main_sys_hub = hub;
}


/*
Get the current main system message hub
Returns:
  The current main hub or NULL
*/
UMsgHub *umsg_sys_hub(void) {
  return s_main_sys_hub;
}


/*
Add a subscriber target to a message hub

Args:
  hub:        Message hub to add subscriber to
  subscriber: New subscriber target for the hub
*/
void umsg_hub_subscribe(UMsgHub *hub, UMsgTarget *subscriber) {
  taskENTER_CRITICAL();
    ll_slist_push(&hub->subscribers, subscriber);
  taskEXIT_CRITICAL();
}


/*
Remove a subscriber target from a message hub

Args:
  hub:        Message hub to remove subscriber from
  subscriber: Subscriber target to remove

Returns:
  true on success
*/
bool umsg_hub_unsubscribe(UMsgHub *hub, UMsgTarget *subscriber) {
  taskENTER_CRITICAL();
    bool status = ll_slist_remove(&hub->subscribers, subscriber);
  taskEXIT_CRITICAL();

  return status;
}


static void umsg__query_cb(UMsgTarget *tgt, UMsg *msg) {
  tgt->user_data = msg->payload;
taskENTER_CRITICAL();
  tgt->msg_handler_cb = NULL; // Notify busy wait loop
taskEXIT_CRITICAL();
}


/*
Send a query message to a target

Set timeout param to NO_TIMEOUT to fail immediately when query can't be sent.
Set it to INFINITE_TIMEOUT to block indefinitely until a response is received.

Args:
  hub:      Hub to send message on
  query_id: Id to request response from target
  response: Response from the target
  timeout:  Timeout in milliseconds

Returns:
  true on success
*/
bool umsg_hub_query(UMsgHub *hub, uint32_t query_id, uintptr_t *response, uint32_t timeout) {
  bool rval = true;
  // Create temporary target to receive response
  UMsgTarget *response_tgt = mp_alloc(mp_sys_pools(), sizeof(UMsgTarget), NULL);
  if(!response_tgt)
    return false;

  uint32_t response_id = prop_new_global_id();

  umsg_tgt_callback_init(response_tgt, umsg__query_cb);
  umsg_tgt_add_filter(response_tgt, PROP_AUX_24_MASK);
  umsg_hub_subscribe(hub, response_tgt);

  // Send query
  UMsg msg = {
    .id     = query_id,
    .source = response_id
  };

  uint32_t start = millis();
  if(!umsg_hub_send(hub, &msg, timeout)) {
    rval = false;
    goto cleanup;
  }

  // Wait for response
  while(response_tgt->msg_handler_cb) {  // Callback function will be cleared when response arrives
    if((timeout != INFINITE_TIMEOUT) && (millis() - start > timeout)) {
      rval = false;
      goto cleanup;
    }
    vTaskDelay(1);
  }

  *response = response_tgt->user_data;

cleanup:
  umsg_hub_unsubscribe(hub, response_tgt);
  umsg_tgt_free(response_tgt);
  mp_free(mp_sys_pools(), response_tgt);

  return rval;
}


/*
Dispatch messages waiting in a message hub queue

Set send_timeout to INFINITE_TIMEOUT to block indefinitely until a subscriber is ready.

Args:
  hub:          Message hub to operate on
  send_timeout: Timeout in milliseconds for sending messages to subscribers
*/
void umsg_hub_process_inbox(UMsgHub *hub, uint32_t send_timeout) {
  UMsgTarget *cur;
  UMsg msg;
  TickType_t send_timeout_ticks;

  send_timeout_ticks = (send_timeout == INFINITE_TIMEOUT) ? portMAX_DELAY : pdMS_TO_TICKS(send_timeout);

  while(xQueueReceive(hub->inbox.q, &msg, portMAX_DELAY) == pdTRUE) {
    // Relay message to subscribers with matching mask
    for(cur = hub->subscribers; cur; cur = cur->next) {
      if(umsg__tgt_match_filter(cur, msg.id)) {

        if(cur->msg_handler_cb) { // Handle message via callback
          cur->msg_handler_cb(cur, &msg);

        } else if(cur->q) { // Pass message along to subscriber queue
          if(msg.payload_size > 0 && msg.payload)  // Add reference for subscriber
            mp_inc_ref((void *)msg.payload);

          if(xQueueSend(cur->q, &msg, send_timeout_ticks) != pdTRUE) {
            report_error(P_ERROR_SYS_MESSAGE_TIMEOUT, 0);

            if(msg.payload_size > 0 && msg.payload)  // Remove unused reference
              mp_free(mp_sys_pools(), (void *)msg.payload);
          }
        }
      }

    }

    if(msg.payload_size > 0 && msg.payload)  // Remove our reference
      mp_free(mp_sys_pools(), (void *)msg.payload);
  }
}


/*
Send an event message to the system hub

Args:
  id:   Event prop ID
  data: Optional associated event data

Returns:
  true on success
*/
bool report_event(uint32_t id, uintptr_t data) {
  UMsg msg = {
    .id       = id,
    .source   = P_RSRC_SYS_LOCAL_TASK,
    .payload  = data
  };

  UMsgHub *hub = umsg_sys_hub();
  if(!hub)
    return false;

  return umsg_hub_send(hub, &msg, NO_TIMEOUT);
}


