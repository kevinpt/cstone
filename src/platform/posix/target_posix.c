#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#include "cstone/platform.h"

#include "cstone/target.h"

void software_reset(void) {
  puts("Simulator can't reset");
}
