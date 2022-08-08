#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <ctype.h>

#include "cstone/platform.h"
#include "lib_cfg/build_config.h"

#include "stm32f4xx_ll_gpio.h"

#include "FreeRTOS.h"
#include "semphr.h"
#include "cstone/console.h"
#include "cstone/term_color.h"



static int32_t cmd_port(uint8_t argc, char *argv[], void *eval_ctx) {
  if(argc < 2)
    return -1;

  char port_id = toupper(argv[1][0]);

  GPIO_TypeDef *port = NULL;

  switch(port_id) {
#ifdef GPIOA
  case 'A': port = GPIOA; break;
#endif
#ifdef GPIOB
  case 'B': port = GPIOB; break;
#endif
#ifdef GPIOC
  case 'C': port = GPIOC; break;
#endif
#ifdef GPIOD
  case 'D': port = GPIOD; break;
#endif
#ifdef GPIOE
  case 'E': port = GPIOE; break;
#endif
#ifdef GPIOF
  case 'F': port = GPIOF; break;
#endif
#ifdef GPIOG
  case 'G': port = GPIOG; break;
#endif
#ifdef GPIOH
  case 'H': port = GPIOH; break;
#endif
#ifdef GPIOI
  case 'I': port = GPIOI; break;
#endif
#ifdef GPIOJ
  case 'J': port = GPIOJ; break;
#endif
#ifdef GPIOK
  case 'K': port = GPIOK; break;
#endif

  default:
    puts("Unknown port");
    break;
  }


  if(!port)
    return -1;

  printf("PORT " A_BMAG "%c" A_NONE ":               " A_YLW "FEDCBA9876543210" A_NONE "\n", port_id);

  uint32_t m, m2;
  int i;
  bool has_alt = false;

  printf("  MODER   - %08lx " A_GRN "|" A_NONE, port->MODER);
  for(i = 30, m2 = 0xC0000000; m2 > 0; i-=2, m2 >>= 2) {
    int mode = (port->MODER & m2) >> i;
    switch(mode) {
    case LL_GPIO_MODE_INPUT:    putc('i', stdout); break;
    case LL_GPIO_MODE_OUTPUT:   putc('o', stdout); break;
    case LL_GPIO_MODE_ALTERNATE: fputs(A_BLU "A" A_NONE, stdout); has_alt = true; break;
    case LL_GPIO_MODE_ANALOG:   fputs(A_CYN "~" A_NONE, stdout); break;
    default: break;
    }
  }
  puts(A_GRN "|" A_NONE);

  printf("  OTYPER  - %08lx " A_GRN "|" A_NONE, port->OTYPER);
  for(i = 30, m2 = 0xC0000000, m = 0x8000; m > 0; i-=2, m2 >>= 2, m >>= 1) {
    int mode = (port->MODER & m2) >> i;
    int otype = (port->OTYPER & m);

    if(mode == LL_GPIO_MODE_OUTPUT || mode == LL_GPIO_MODE_ALTERNATE)
      otype ? fputs(u8"\u2390" /*open drain ⎐ */, stdout) : fputs(u8"\u2195" /*push-pull ↕*/, stdout);
    else
      putc(' ', stdout);
  }
  puts(A_GRN "|" A_NONE);

  printf("  OSPEEDR - %08lx " A_GRN "|" A_NONE, port->OSPEEDR);
  for(i = 30, m2 = 0xC0000000; m2 > 0; i-=2, m2 >>= 2) {
    int mode = (port->MODER & m2) >> i;
    int speed = (port->OSPEEDR & m2) >> i;
    if(mode == LL_GPIO_MODE_OUTPUT || mode == LL_GPIO_MODE_ALTERNATE) {
      switch(speed) {
      case LL_GPIO_SPEED_FREQ_LOW:        fputs(A_CYN "l", stdout); break;
      case LL_GPIO_SPEED_FREQ_MEDIUM:     fputs(A_YLW "m", stdout); break;
      case LL_GPIO_SPEED_FREQ_HIGH:       fputs(A_MAG "h", stdout); break;
      case LL_GPIO_SPEED_FREQ_VERY_HIGH:  fputs(A_RED "H", stdout); break;
      default: break;
      }
    } else {
      putc(' ', stdout);
    }
  }
  puts(A_GRN "|" A_NONE);

  printf("  PUPDR   - %08lx " A_GRN "|" A_NONE, port->PUPDR);
  for(i = 30, m2 = 0xC0000000; m2 > 0; i-=2, m2 >>= 2) {
    int pull = (port->PUPDR & m2) >> i;
    switch(pull) {
    case LL_GPIO_PULL_NO:   putc(' ', stdout); break;
    case LL_GPIO_PULL_UP:   fputs(u8"\u2191", stdout); break; // Up arrow '↑'
    case LL_GPIO_PULL_DOWN: fputs(u8"\u21B3", stdout); break; // Down right arrow '↳'
    default: break;
    }
  }
  puts(A_GRN "|" A_NONE);

  printf("  IDR     - %08lx " A_GRN "|" A_NONE, port->IDR);
  for(m = 0x8000; m > 0; m >>= 1) {
    int pin = (port->IDR & m);
    pin ? putc('1', stdout) : putc('0', stdout);
  }
  puts(A_GRN "|" A_NONE);

  printf("  ODR     - %08lx " A_GRN "|" A_NONE, port->OTYPER);
  for(i = 30, m2 = 0xC0000000, m = 0x8000; m > 0; i-=2, m2 >>= 2, m >>= 1) {
    int mode  = (port->MODER & m2) >> i;
    int otype = (port->OTYPER & m);
    int pull  = (port->PUPDR & m2) >> i;
    int pin   = (port->ODR & m);

    if(mode == LL_GPIO_MODE_INPUT || mode == LL_GPIO_MODE_ANALOG) {
      putc(' ', stdout);

    } else { // Output or alt
      if(pin == 0) { // Always have active drive low
        putc('0', stdout);

      } else { // Determine "high" state
        if(otype == 1) { // Open drain output
          switch(pull) {
          case LL_GPIO_PULL_NO:   putc('z', stdout); break;
          case LL_GPIO_PULL_UP:   fputs(u8"\u2191", stdout); break; // Up arrow '↑'
          case LL_GPIO_PULL_DOWN: fputs(u8"\u21B3", stdout); break; // Down right arrow '↳'
          default: break;
          }

        } else {  // Push-pull output
          putc('1', stdout);
        }
      }
    }
  }
  puts(A_GRN "|" A_NONE);



  if(has_alt) {
    puts(A_BLU "Alt modes:" A_NONE);
    for(i = 30, m2 = 0xC0000000; m2 > 0; i-=2, m2 >>= 2) {
      int mode = (port->MODER & m2) >> i;
      if(mode == LL_GPIO_MODE_ALTERNATE) {
        int af_ix = i/2;
        int af = (port->AFR[af_ix/8] >> (af_ix%8 * 4)) & 0x0F;
        printf(A_YLW "  %2d" A_NONE ": " A_BLU "AF%d" A_NONE "\n", af_ix, af);
      }
    }
  }

  return 0;
}


const ConsoleCommandDef g_stm32_cmd_set[] = {
  CMD_DEF("port",     cmd_port,       "Dump GPIO"),
  CMD_END
};
