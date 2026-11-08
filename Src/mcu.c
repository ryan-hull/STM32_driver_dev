/*
 * mcu.c
 *
 *  Created on: Nov 6, 2026
 *      Author: rynos
 */

#include "mcu.h"
#include <stdint.h>

static volatile uint32_t s_ticks;   // static: file scope. volatile: modified elsewhere (interrupt)

uint32_t millis(void) { return s_ticks; }

void SysTick_Handler(void) {
  s_ticks++;
}

// a blocking wait
void delay_ms(uint32_t ms) {
  uint32_t start = s_ticks;
  while ((s_ticks - start) < ms) {}   // wraparound safe for unsigned ints
}
