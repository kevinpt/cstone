#ifndef GPIO_H
#define GPIO_H

/* Generic GPIO API

Note that the DEF_PIN() macro uses a GCC extension to mark an initializer function
as a constructor in C. If the target compiler does not support this you have to
either manually call the constructor function <pin name>_ctor() for each pin or
skip the macro and call gpio_init() directly.

*/

#define GPIO_PIN_NO_INIT    0x00
#define GPIO_PIN_INPUT      0x01 // Input
#define GPIO_PIN_INPUT_PD   0x02 // Input with pulldown
#define GPIO_PIN_INPUT_PU   0x03 // Input with pullup
#define GPIO_PIN_OUTPUT     0x11 // Output
#define GPIO_PIN_OUTPUT_L   0x12 // Output, preinitialized low before enabling
#define GPIO_PIN_OUTPUT_H   0x13 // Output, preinitialized high before enabling
#define GPIO_PIN_OUTPUT_OD  0x21  // Output, open drain with pullup

#define IS_OUTPUT_MODE(m) ((m) & 0x30)


#define GPIO_EDGE_SLOW        0x000
#define GPIO_EDGE_MEDIUM      0x100
#define GPIO_EDGE_FAST        0x200
#define GPIO_EDGE_VERY_FAST   0x300

#define GPIO_PORT_MODE(m)   ((m) & 0xFF)
#define GPIO_PORT_SPEED(m)  ((m) >> 8)

#define GPIO_PORT_A   0
#define GPIO_PORT_B   1
#define GPIO_PORT_C   2
#define GPIO_PORT_D   3
#define GPIO_PORT_E   4
#define GPIO_PORT_F   5
#define GPIO_PORT_G   6
#define GPIO_PORT_H   7
#define GPIO_PORT_I   8
#define GPIO_PORT_J   9
#define GPIO_PORT_K   10

typedef struct {
  unsigned short  mode; // Pin config and speed
  uint8_t   port;
  uint8_t   pin;
} GPIOPin;


typedef unsigned int GPIOPortData;

typedef struct {
  GPIOPortData mask;        // Derived mask for bits in bus
  unsigned short mode;  // Pin config and speed
  uint8_t port;
  uint8_t size;         // Bits in bus
  uint8_t shift;        // Offset from bit 0
} GPIOBus;


#define DEF_PIN(name, port, pin, mode) \
  GPIOPin name = {0}; \
  __attribute__((constructor)) \
  void name ## __ctor(void); \
  void name ## __ctor(void) { \
    gpio_init(&name, port, pin, mode); \
  }


#ifdef __cplusplus
extern "C" {
#endif

void gpio_sys_init(void);
void gpio_enable_port(uint8_t port);

void gpio_init(GPIOPin *gpio, uint8_t port, uint8_t pin, unsigned short mode);
bool gpio_value(GPIOPin *gpio);
void gpio_set_high(GPIOPin *gpio);
void gpio_set_low(GPIOPin *gpio);
void gpio_set(GPIOPin *gpio, bool new_value);
void gpio_toggle(GPIOPin *gpio);
void gpio_highz_on(GPIOPin *gpio);
void gpio_highz_off(GPIOPin *gpio, bool new_value);


void gpio_bus_init(GPIOBus *bus, uint8_t port, uint8_t size, uint8_t shift, unsigned short mode);
void gpio_bus_mode(GPIOBus *bus, unsigned short mode);
void gpio_bus_set(GPIOBus *bus, GPIOPortData new_value);
GPIOPortData gpio_bus_value(GPIOBus *bus);

#ifdef __cplusplus
}
#endif

#endif // GPIO_H

