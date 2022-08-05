#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#ifdef AVR
#  include <util/atomic.h>
#else
#  include "FreeRTOS.h"
#  include "task.h"
#endif

#include "util/list_ops.h"
#include "cstone/led_blink.h"


// Protect linked list access
#ifdef AVR
#  define LOCK()    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
#  define UNLOCK()  }
#else
#  define LOCK()    taskENTER_CRITICAL();
#  define UNLOCK()  taskEXIT_CRITICAL();
#endif


// List of active blinkers
static LedBlinker *s_LedBlinkerList = NULL;



void blink_init(LedBlinker *blink, uint8_t led_id, const BlinkTime *pattern, uint8_t repeats,
                PatternCompletion complete) {
  blink_configure(blink, led_id, pattern, repeats, complete);
  blink->next = NULL;
}

void blink_configure(LedBlinker *blink, uint8_t led_id, const BlinkTime *pattern,
                     uint8_t repeats, PatternCompletion complete) {
  blink->led_id      = led_id & 0x7F;
  blink->complete    = complete;

  blink_set_pattern(blink, pattern, repeats);
}

void blink_set_pattern(LedBlinker *blink, const BlinkTime *pattern, uint8_t repeats) {
  blink->timestamp = blink_timestamp();
  blink->pattern   = pattern;
  blink->repeats   = repeats ? repeats+1 : repeats;
  blink->pat_ix    = 0;

  set_led(blink->led_id, 1);
}

void blink_set_index(LedBlinker *blink, uint8_t ix) {
  blink->pat_ix = ix;

  // LED is on during even indices in the pattern
  short set_on = !(blink->pat_ix & 0x01) ? 1 : 0;
  set_led(blink->led_id, set_on);
}

void blink_restart(LedBlinker *blink, uint8_t repeats) {
  blink->timestamp = blink_timestamp();
  blink->repeats   = repeats ? repeats+1 : repeats;
  blink->pat_ix    = 0;

  set_led(blink->led_id, 1);
}

bool blink_is_active(LedBlinker *blink) { return blink->pattern && blink->repeats != 1; }


uint8_t blink_led_id(LedBlinker *blink) { return blink->led_id; }


bool blink_update(LedBlinker *blink, BlinkTime now) {
  BlinkTime delta = now - blink->timestamp; // Elapsed since last update
  bool keep_blinker = true;


  if(delta >= blink->pattern[blink->pat_ix]) {
    // Advance through pattern to current time position
    do {
      delta -= blink->pattern[blink->pat_ix++];
      if(blink->pattern[blink->pat_ix] == 0) { // End of pattern
        blink->pat_ix = 0;

        if(blink->repeats > 1)
          blink->repeats--;

        if(blink->repeats == 1) { // Done blinking
          delta = 0;

          if(blink->complete) // Notify pattern is complete
            blink->complete(blink);

          keep_blinker = blink->repeats != 1;
          if(!keep_blinker)
            set_led(blink->led_id, 0); // Disable LED
          break;

        }
        // Run indefinitely if repeats == 0
      }

    } while(delta >= blink->pattern[blink->pat_ix]);

    if(keep_blinker) {
      blink->timestamp = now - delta; // Correct for any system delay

      // LED is on during even indices in the pattern
      short set_on = !(blink->pat_ix & 0x01) ? 1 : 0;
      set_led(blink->led_id, set_on);
    }
  }

  return keep_blinker;
}





void blinkers_add(LedBlinker *blink) {
LOCK();
  if(blink && blink->next == NULL)
    ll_slist_push(&s_LedBlinkerList, blink);
UNLOCK();
}

void blinkers_remove(LedBlinker *blink) {
LOCK();
  if(blink)
    ll_slist_remove(&s_LedBlinkerList, blink);
UNLOCK();
}


LedBlinker *blinkers_find(uint8_t led_id) {
  LedBlinker *blink = s_LedBlinkerList;

  while(blink) {
    if(blink_led_id(blink) == led_id)
      break;

  LOCK();
    blink = blink->next;
  UNLOCK();
  }

  return blink;
}


bool blinkers_cancel(uint8_t led_id) {
  LedBlinker *blink = blinkers_find(led_id);

  if(blink)
    blinkers_remove(blink);

  return blink;
}


void blinkers_update_all(void) {
  BlinkTime now = blink_timestamp();
  LedBlinker *blink;

LOCK();
  blink = s_LedBlinkerList;
UNLOCK();

  while(blink) {
    bool keep_blinker = blink_update(blink, now);

    LedBlinker *next_blink;

  LOCK();
    next_blink = blink->next;
  UNLOCK();

    if(!keep_blinker)
      blinkers_remove(blink);

    blink = next_blink;
  }
}


