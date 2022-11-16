#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <ctype.h>

#include "build_config.h" // Get build-specific platform settings
#include "cstone/platform.h"

#if defined PLATFORM_STM32F1
#  include "stm32f1xx_ll_gpio.h"
#else
#  include "stm32f4xx_ll_gpio.h"
#endif

#include "FreeRTOS.h"
#include "semphr.h"
#include "cstone/console.h"
#include "cstone/term_color.h"
#include "cstone/blocking_io.h"



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


#define VERT_BAR        A_GRN "|" A_NONE
#define CH_OPEN_DRAIN   u8"\u2390" /*open drain ⎐ */
#define CH_PUSH_PULL    u8"\u2195" /*push-pull ↕*/
#define CH_PULL_UP      u8"\u2191" /* Up arrow '↑' */
#define CH_PULL_DOWN    u8"\u21B3" /* Down right arrow '↳' */

// STM32F1 has a different register layout for GPIO than STM32F4
#if defined PLATFORM_STM32F1
  uint32_t m;
  int i;
  int mode, cnf;

  printf("  CRH     - %08lX " VERT_BAR, port->CRH);
  // Pin modes
  for(i = 28; i >= 0; i -= 4) {  // Upper 8 bits
    mode = (port->CRH >> i) & GPIO_CRL_MODE0_Msk;
    cnf = (port->CRH >> i) & GPIO_CRL_CNF0_Msk;

    if(mode == 0x00)  // Input mode
      putc(cnf == 0 ? '~' : 'i', stdout);
    else  // Output
      putc(cnf & GPIO_CRL_CNF0_1 ? 'A' : 'o', stdout);

  }
  for(i = 28; i >= 0; i -= 4) {  // Lower 8 bits
    mode = (port->CRL >> i) & GPIO_CRL_MODE0_Msk;
    cnf = (port->CRL >> i) & GPIO_CRL_CNF0_Msk;

    if(mode == 0x00)  // Input mode
      putc(cnf == 0 ? '~' : 'i', stdout);
    else  // Output
      putc(cnf & GPIO_CRL_CNF0_1 ? 'A' : 'o', stdout);
  }
  puts(VERT_BAR);


  printf("  CRL     - %08lX " VERT_BAR, port->CRL);
  // Pin output drivers
  for(i = 28; i >= 0; i -= 4) {  // Upper 8 bits
    mode = (port->CRH >> i) & GPIO_CRL_MODE0_Msk;
    cnf = (port->CRH >> i) & GPIO_CRL_CNF0_Msk;

    if(mode == 0x00)  // Input mode
      putc(' ', stdout);
    else  // Output
      fputs(cnf & GPIO_CRL_CNF0_0 ? CH_OPEN_DRAIN : CH_PUSH_PULL, stdout);
  }
  for(i = 28; i >= 0; i -= 4) {  // Lower 8 bits
    mode = (port->CRL >> i) & GPIO_CRL_MODE0_Msk;
    cnf = (port->CRL >> i) & GPIO_CRL_CNF0_Msk;

    if(mode == 0x00)  // Input mode
      putc(' ', stdout);
    else  // Output
      fputs(cnf & GPIO_CRL_CNF0_0 ? CH_OPEN_DRAIN : CH_PUSH_PULL, stdout);
  }
  puts(VERT_BAR);


  fputs("  (Edge rate)        " VERT_BAR, stdout);
  // Pin edge rate
  for(i = 28; i >= 0; i -= 4) {  // Upper 8 bits
    mode = (port->CRH >> i) & GPIO_CRL_MODE0_Msk;
    cnf = (port->CRH >> i) & GPIO_CRL_CNF0_Msk;

    if(mode == 0x00)  // Input mode
      putc(' ', stdout);
    else  // Output
      switch(mode) {
      case LL_GPIO_SPEED_FREQ_LOW:        fputs(A_CYN "l", stdout); break;
      case LL_GPIO_SPEED_FREQ_MEDIUM:     fputs(A_YLW "m", stdout); break;
      case LL_GPIO_SPEED_FREQ_HIGH:       fputs(A_MAG "h", stdout); break;
      default:  break;
      }
  }
  for(i = 28; i >= 0; i -= 4) {  // Lower 8 bits
    mode = (port->CRL >> i) & GPIO_CRL_MODE0_Msk;
    cnf = (port->CRL >> i) & GPIO_CRL_CNF0_Msk;

    if(mode == 0x00)  // Input mode
      putc(' ', stdout);
    else  // Output
      switch(mode) {
      case LL_GPIO_SPEED_FREQ_LOW:        fputs(A_CYN "l", stdout); break;
      case LL_GPIO_SPEED_FREQ_MEDIUM:     fputs(A_YLW "m", stdout); break;
      case LL_GPIO_SPEED_FREQ_HIGH:       fputs(A_MAG "h", stdout); break;
      default:  break;
      }
  }
  puts(VERT_BAR);

  fputs("  (Pulls)            " VERT_BAR, stdout);
  // Pin pulls
  for(i = 28, m = 0x8000; i >= 0; i -= 4, m >>= 1) {  // Upper 8 bits
    mode = (port->CRH >> i) & GPIO_CRL_MODE0_Msk;
    cnf = (port->CRH >> i) & GPIO_CRL_CNF0_Msk;

    if(mode == 0x00) {  // Input mode
      if(cnf == 0x04) // Floating input
        putc(' ', stdout);
      else  // Pulls active
        fputs((port->ODR & m) ? CH_PULL_UP : CH_PULL_DOWN, stdout);

    } else {  // Output
      putc(' ', stdout);
    }
  }
  for(i = 28, m = 0x0080; i >= 0; i -= 4, m >>= 1) {  // Lower 8 bits
    mode = (port->CRL >> i) & GPIO_CRL_MODE0_Msk;
    cnf = (port->CRL >> i) & GPIO_CRL_CNF0_Msk;

    if(mode == 0x00) {  // Input mode
      if(cnf == 0x04) // Floating input
        putc(' ', stdout);
      else  // Pulls active
        fputs((port->ODR & m) ? CH_PULL_UP : CH_PULL_DOWN, stdout);

    } else {  // Output
      putc(' ', stdout);
    }
  }
  puts(VERT_BAR);


  printf("  IDR     - %08lx " VERT_BAR, port->IDR);
  for(m = 0x8000; m > 0; m >>= 1) {
    putc((port->IDR & m) ? '1' : '0', stdout);
  }
  puts(VERT_BAR);

  printf("  ODR     - %08lx " VERT_BAR, port->ODR);
  for(i = 28, m = 0x8000; i >= 0; i -= 4, m >>= 1) {  // Upper 8 bits
    mode = (port->CRH >> i) & GPIO_CRL_MODE0_Msk;
    cnf = (port->CRH >> i) & GPIO_CRL_CNF0_Msk;

    if(mode == 0x00) {  // Input mode
      putc(' ', stdout);
    } else {  // Output
      if(port->ODR & m)
        putc((cnf & GPIO_CRL_CNF0_0) ? 'z' : '1', stdout);
      else
        putc('0', stdout);
    }
  }
  for(i = 28, m = 0x0080; i >= 0; i -= 4, m >>= 1) {  // Lower 8 bits
    mode = (port->CRL >> i) & GPIO_CRL_MODE0_Msk;
    cnf = (port->CRL >> i) & GPIO_CRL_CNF0_Msk;

    if(mode == 0x00) {  // Input mode
      putc(' ', stdout);
    } else {  // Output
      if(port->ODR & m)
        putc((cnf & GPIO_CRL_CNF0_0) ? 'z' : '1', stdout);
      else
        putc('0', stdout);
    }
  }
  puts(VERT_BAR);

#elif defined PLATFORM_STM32F4
  uint32_t m, m2;
  int i;
  bool has_alt = false;

  printf("  MODER   - %08lx " VERT_BAR, port->MODER);
  // Pin modes
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
  puts(VERT_BAR);

  printf("  OTYPER  - %08lx " VERT_BAR, port->OTYPER);
  // Pin output drivers
  for(i = 30, m2 = 0xC0000000, m = 0x8000; m > 0; i-=2, m2 >>= 2, m >>= 1) {
    int mode = (port->MODER & m2) >> i;
    int otype = (port->OTYPER & m);

    if(mode == LL_GPIO_MODE_OUTPUT || mode == LL_GPIO_MODE_ALTERNATE)
      otype ? fputs(CH_OPEN_DRAIN, stdout) : fputs(CH_PUSH_PULL, stdout);
    else
      putc(' ', stdout);
  }
  puts(VERT_BAR);

  printf("  OSPEEDR - %08lx " VERT_BAR, port->OSPEEDR);
  // Pin edge rate
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
  puts(VERT_BAR);

  printf("  PUPDR   - %08lx " VERT_BAR, port->PUPDR);
  // Pin pulls
  for(i = 30, m2 = 0xC0000000; m2 > 0; i-=2, m2 >>= 2) {
    int pull = (port->PUPDR & m2) >> i;
    switch(pull) {
    case LL_GPIO_PULL_NO:   putc(' ', stdout); break;
    case LL_GPIO_PULL_UP:   fputs(CH_PULL_UP, stdout); break;
    case LL_GPIO_PULL_DOWN: fputs(CH_PULL_DOWN, stdout); break;
    default: break;
    }
  }
  puts(VERT_BAR);

  printf("  IDR     - %08lx " VERT_BAR, port->IDR);
  for(m = 0x8000; m > 0; m >>= 1) {
    int pin = (port->IDR & m);
    pin ? putc('1', stdout) : putc('0', stdout);
  }
  puts(VERT_BAR);

  printf("  ODR     - %08lx " VERT_BAR, port->ODR);
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
  puts(VERT_BAR);
#endif

#if defined PLATFORM_STM32F4
// STM32F4 has multiple alternate functions per pin so we list the mapping here
  if(has_alt) {
    bputs(A_BLU "Alt modes:" A_NONE);
    for(i = 30, m2 = 0xC0000000; m2 > 0; i-=2, m2 >>= 2) {
      int mode = (port->MODER & m2) >> i;
      if(mode == LL_GPIO_MODE_ALTERNATE) {
        int af_ix = i/2;
        int af = (port->AFR[af_ix/8] >> (af_ix%8 * 4)) & 0x0F;
        bprintf(A_YLW "  %2d" A_NONE ": " A_BLU "AF%d" A_NONE "\n", af_ix, af);
      }
    }
  }
#endif
  return 0;
}


const ConsoleCommandDef g_stm32_cmd_set[] = {
  CMD_DEF("port",     cmd_port,       "Dump GPIO"),
  CMD_END
};
