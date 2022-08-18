#ifndef LED_BLINK_H
#define LED_BLINK_H

#define BLINK_ALWAYS    0   // Use for repeats arg on set_pattern() and restart()

#define BLINK_PAT(...) {__VA_ARGS__, 0}

typedef unsigned  BlinkTime;

struct LedBlinker;

typedef void (*PatternCompletion)(struct LedBlinker *blinker);

typedef struct LedBlinker {
  struct LedBlinker  *next;
  const BlinkTime    *pattern; // Array of blink periods
  PatternCompletion   complete;
  BlinkTime           timestamp;
  uint8_t             led_id;
  uint8_t             repeats     :4; // Number of repetitions for pattern; 0 == infinity
  uint8_t             pat_ix      :4;
} LedBlinker;


// Generic blank patterns
extern BlinkTime g_PatternFastBlink[];
extern BlinkTime g_PatternSlowBlink[];
extern BlinkTime g_PatternPulseOne[];
extern BlinkTime g_PatternPulseTwo[];
extern BlinkTime g_PatternPulseThree[];
extern BlinkTime g_PatternPulseFour[];
extern BlinkTime g_PatternFlash200ms[];
extern BlinkTime g_PatternDelay3s[];


#ifdef __cplusplus
extern "C" {
#endif

// You must provide implementations of these two functions
void set_led(uint8_t led_id, const short state);
unsigned blink_timestamp(void);


// ******************** LedBlinker operations ********************
void blink_init(LedBlinker *blink, uint8_t led_id, const BlinkTime *pattern, uint8_t repeats,
                PatternCompletion complete);
void blink_configure(LedBlinker *blink, uint8_t led_id, const BlinkTime *pattern,
                     uint8_t repeats, PatternCompletion complete);
void blink_set_pattern(LedBlinker *blink, const BlinkTime *pattern, uint8_t repeats);
void blink_set_index(LedBlinker *blink, uint8_t ix);
void blink_restart(LedBlinker *blink, uint8_t repeats);
bool blink_is_active(LedBlinker *blink);
uint8_t blink_led_id(LedBlinker *blink);
bool blink_update(LedBlinker *blink, BlinkTime now);

// ******************** Blinker management ********************
void blinkers_add(LedBlinker *blinker);
void blinkers_remove(LedBlinker *blinker);

LedBlinker *blinkers_find(uint8_t led_id);
bool blinkers_cancel(uint8_t led_id);

void blinkers_update_all(void);

#ifdef __cplusplus
}
#endif

#endif // LED_BLINK_H
