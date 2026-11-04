#ifndef SDL_SYS_H
#define SDL_SYS_H

#include <stdint.h>

/* main hooks IRQ 0 (vector 8) after programming the PIT (ports 0x43, 0x40).
   The handler in the binary is 0x102A0. */

void timer_init(void);
uint32_t timer_ticks(void);
void timer_delay(uint32_t ms);

#endif
