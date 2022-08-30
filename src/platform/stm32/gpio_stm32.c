#include <stdint.h>
#include <stdbool.h>

#include "build_config.h" // Get build-specific platform settings

#include "cstone/io/gpio.h"
#if defined PLATFORM_STM32F1
#  include "stm32f1xx_ll_gpio.h"
#else
#  include "stm32f4xx_ll_gpio.h"
#endif

#ifndef COUNT_OF
#  define COUNT_OF(a) (sizeof(a) / sizeof(*(a)))
#endif


void gpio_sys_init(void) {
}


void gpio_enable_port(uint8_t port) {
  switch(port) {
#ifdef GPIOA
  case GPIO_PORT_A: __HAL_RCC_GPIOA_CLK_ENABLE(); break;
#endif
#ifdef GPIOB
  case GPIO_PORT_B: __HAL_RCC_GPIOB_CLK_ENABLE(); break;
#endif
#ifdef GPIOC
  case GPIO_PORT_C: __HAL_RCC_GPIOC_CLK_ENABLE(); break;
#endif
#ifdef GPIOD
  case GPIO_PORT_D: __HAL_RCC_GPIOD_CLK_ENABLE(); break;
#endif
#ifdef GPIOE
  case GPIO_PORT_E: __HAL_RCC_GPIOE_CLK_ENABLE(); break;
#endif
#ifdef GPIOF
  case GPIO_PORT_F: __HAL_RCC_GPIOF_CLK_ENABLE(); break;
#endif
#ifdef GPIOG
  case GPIO_PORT_G: __HAL_RCC_GPIOG_CLK_ENABLE(); break;
#endif
#ifdef GPIOH
  case GPIO_PORT_H: __HAL_RCC_GPIOH_CLK_ENABLE(); break;
#endif
#ifdef GPIOI
  case GPIO_PORT_I: __HAL_RCC_GPIOI_CLK_ENABLE(); break;
#endif
#ifdef GPIOJ
  case GPIO_PORT_J: __HAL_RCC_GPIOJ_CLK_ENABLE(); break;
#endif
#ifdef GPIOK
  case GPIO_PORT_K: __HAL_RCC_GPIOK_CLK_ENABLE(); break;
#endif

  default:  break;
  }
}

#if defined PLATFORM_STM32F1
// STM32F1 doesn't use simple sequential pin numbers in its GPIO registers.
// We have to translate logical pins to their encoded form.
static inline GPIOPortData stm32__pin_encode(uint8_t pin) {
  static const GPIOPortData s_port_pins[] = {
    LL_GPIO_PIN_0,  LL_GPIO_PIN_1,  LL_GPIO_PIN_2,  LL_GPIO_PIN_3,
    LL_GPIO_PIN_4,  LL_GPIO_PIN_5,  LL_GPIO_PIN_6,  LL_GPIO_PIN_7,
    LL_GPIO_PIN_8,  LL_GPIO_PIN_9,  LL_GPIO_PIN_10, LL_GPIO_PIN_11,
    LL_GPIO_PIN_12, LL_GPIO_PIN_13, LL_GPIO_PIN_14, LL_GPIO_PIN_15
  };

  return s_port_pins[pin & 0x0F];
}
#else
#  define stm32__pin_encode(pin)  (1ul << (pin))
#endif

// Convert port index to STM32 base register for the selected port
static inline GPIO_TypeDef *stm32__get_port_addr(uint8_t port) {
  static const uintptr_t s_port_addresses[] = {
#ifdef GPIOA
    [GPIO_PORT_A] = GPIOA_BASE,
#endif
#ifdef GPIOB
    [GPIO_PORT_B] = GPIOB_BASE,
#endif
#ifdef GPIOC
    [GPIO_PORT_C] = GPIOC_BASE,
#endif
#ifdef GPIOD
    [GPIO_PORT_D] = GPIOD_BASE,
#endif
#ifdef GPIOE
    [GPIO_PORT_E] = GPIOE_BASE,
#endif
#ifdef GPIOF
    [GPIO_PORT_F] = GPIOF_BASE,
#endif
#ifdef GPIOG
    [GPIO_PORT_G] = GPIOG_BASE,
#endif
#ifdef GPIOH
    [GPIO_PORT_H] = GPIOH_BASE,
#endif
#ifdef GPIOI
    [GPIO_PORT_I] = GPIOI_BASE,
#endif
#ifdef GPIOJ
    [GPIO_PORT_J] = GPIOJ_BASE,
#endif
#ifdef GPIOK
    [GPIO_PORT_K] = GPIOK_BASE
#endif
  };

  if(port < COUNT_OF(s_port_addresses))
    return (GPIO_TypeDef *)s_port_addresses[port];
  else
    return 0;
}


static void stm32__configure_port(GPIO_TypeDef *port_addr, GPIOPortData pin_bit, unsigned short mode) {
  uint32_t stm32_speed;
  switch(GPIO_PORT_SPEED(mode)) {
  default:
  case GPIO_EDGE_SLOW:      stm32_speed = LL_GPIO_SPEED_FREQ_LOW; break;
  case GPIO_EDGE_MEDIUM:    stm32_speed = LL_GPIO_SPEED_FREQ_MEDIUM; break;
  case GPIO_EDGE_FAST:      stm32_speed = LL_GPIO_SPEED_FREQ_HIGH; break;
#ifdef PLATFORM_STM32F4
  case GPIO_EDGE_VERY_FAST: stm32_speed = LL_GPIO_SPEED_FREQ_VERY_HIGH; break;
#endif
  }

  LL_GPIO_InitTypeDef pin_cfg = {
    .Pin        = pin_bit,
    .Mode       = LL_GPIO_MODE_OUTPUT,
    .Speed      = stm32_speed,
    .OutputType = LL_GPIO_OUTPUT_PUSHPULL,
#ifdef PLATFORM_STM32F1
    // STM32F1 GPIO lacks PUPDR to disable pulls. Pulls not used in output modes.
    .Pull       = LL_GPIO_PULL_UP
#else
    .Pull       = LL_GPIO_PULL_NO
#endif
  };

  switch(GPIO_PORT_MODE(mode)) {
  default:
  case GPIO_PIN_INPUT:
    pin_cfg.Mode = LL_GPIO_MODE_INPUT;
    break;

  case GPIO_PIN_INPUT_PD:
    pin_cfg.Mode = GPIO_PIN_INPUT;
    pin_cfg.Pull = LL_GPIO_PULL_DOWN;
    break;

  case GPIO_PIN_INPUT_PU:
    pin_cfg.Mode = LL_GPIO_MODE_INPUT;
    pin_cfg.Pull = LL_GPIO_PULL_UP;
    break;

  case GPIO_PIN_OUTPUT:
    break;

  case GPIO_PIN_OUTPUT_L:
    LL_GPIO_ResetOutputPin(port_addr, pin_bit); // Set initial state
    break;

  case GPIO_PIN_OUTPUT_H:
    LL_GPIO_SetOutputPin(port_addr, pin_bit); // Set initial state
    break;

  case GPIO_PIN_OUTPUT_OD:
    pin_cfg.OutputType = LL_GPIO_OUTPUT_OPENDRAIN;
    pin_cfg.Pull = LL_GPIO_PULL_UP;
    LL_GPIO_SetOutputPin(port_addr, pin_bit); // Set initial PU state
    break;
  }

  LL_GPIO_Init(port_addr, &pin_cfg);
}


void gpio_init(GPIOPin *gpio, uint8_t port, uint8_t pin, unsigned short mode) {
  gpio->port  = port;
  gpio->pin   = pin;
  gpio->mode  = mode;

  GPIO_TypeDef *port_addr = stm32__get_port_addr(port);
  GPIOPortData pin_bit = stm32__pin_encode(pin);
  gpio_enable_port(port);
  stm32__configure_port(port_addr, pin_bit, mode);
}

bool gpio_value(GPIOPin *gpio) {
  GPIO_TypeDef *port_addr = stm32__get_port_addr(gpio->port);
  GPIOPortData pin_bit = stm32__pin_encode(gpio->pin);
  return LL_GPIO_IsInputPinSet(port_addr, pin_bit);
}

void gpio_set_high(GPIOPin *gpio) {
  GPIO_TypeDef *port_addr = stm32__get_port_addr(gpio->port);
  GPIOPortData pin_bit = stm32__pin_encode(gpio->pin);
  LL_GPIO_SetOutputPin(port_addr, pin_bit);
}

void gpio_set_low(GPIOPin *gpio) {
  GPIO_TypeDef *port_addr = stm32__get_port_addr(gpio->port);
  GPIOPortData pin_bit = stm32__pin_encode(gpio->pin);
  LL_GPIO_ResetOutputPin(port_addr, pin_bit);
}

void gpio_set(GPIOPin *gpio, bool new_value) {
  GPIO_TypeDef *port_addr = stm32__get_port_addr(gpio->port);
  GPIOPortData pin_bit = stm32__pin_encode(gpio->pin);
  if(new_value)
    LL_GPIO_SetOutputPin(port_addr, pin_bit);
  else
    LL_GPIO_ResetOutputPin(port_addr, pin_bit);
}

void gpio_toggle(GPIOPin *gpio) {
  GPIO_TypeDef *port_addr = stm32__get_port_addr(gpio->port);
  GPIOPortData pin_bit = stm32__pin_encode(gpio->pin);
  LL_GPIO_TogglePin(port_addr, pin_bit);
}

void gpio_highz_on(GPIOPin *gpio) {
  GPIO_TypeDef *port_addr = stm32__get_port_addr(gpio->port);
  GPIOPortData pin_bit = stm32__pin_encode(gpio->pin);

  // Switch to input
#ifdef PLATFORM_STM32F1
  // STM32F1 GPIO lacks PUPDR to disable pulls. Set with mode instead.
  LL_GPIO_SetPinMode(port_addr, pin_bit, LL_GPIO_MODE_FLOATING);
#else
  LL_GPIO_SetPinMode(port_addr, pin_bit, LL_GPIO_MODE_INPUT);
  LL_GPIO_SetPinPull(port_addr, pin_bit, LL_GPIO_PULL_NO);
#endif
}

void gpio_highz_off(GPIOPin *gpio, bool new_value) {
  GPIO_TypeDef *port_addr = stm32__get_port_addr(gpio->port);
  GPIOPortData pin_bit = stm32__pin_encode(gpio->pin);

  if(IS_OUTPUT_MODE(gpio->mode) || gpio->mode == GPIO_PIN_OUTPUT_OD) {
    gpio_set(gpio, new_value);
    LL_GPIO_SetPinMode(port_addr, pin_bit, LL_GPIO_MODE_OUTPUT);
  }
}






void gpio_bus_init(GPIOBus *bus, uint8_t port, uint8_t size, uint8_t shift, unsigned short mode) {
  bus->mode = mode;
  bus->port = port;
  bus->size = size;
  bus->shift = shift;
  bus->mask = ((1ul << size)-1) << shift;

  GPIO_TypeDef *port_addr = stm32__get_port_addr(port);
  gpio_enable_port(port);
  stm32__configure_port(port_addr, bus->mask, mode);
}


void gpio_bus_mode(GPIOBus *bus, unsigned short mode) {
  GPIO_TypeDef *port_addr = stm32__get_port_addr(bus->port);
  stm32__configure_port(port_addr, bus->mask, mode);
}


void gpio_bus_set(GPIOBus *bus, GPIOPortData new_value) {
  GPIOPortData one_bits  = (new_value << bus->shift) & bus->mask;
  GPIOPortData zero_bits = (~one_bits) & bus->mask;

  GPIO_TypeDef *port_addr = stm32__get_port_addr(bus->port);
  LL_GPIO_WriteReg(port_addr, BSRR, (zero_bits << 16) | one_bits);
}

GPIOPortData gpio_bus_value(GPIOBus *bus) {
  GPIO_TypeDef *port_addr = stm32__get_port_addr(bus->port);
  return (LL_GPIO_ReadInputPort(port_addr) & bus->mask) >> bus->shift;
}


