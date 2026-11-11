/*
 * mcu.h
 *
 *  Created on: Nov 4, 2026
 *      Author: rynos
 */

#ifndef MCU_H_
#define MCU_H_

#include <stdint.h>
#include <stdbool.h>

//#define BIT(x) (1UL << (x))



// Overrides the system default weak handlers
//void SysTick_Handler(void);

uint32_t millis(void);

void delay_ms(uint32_t ms);

// t: expiration time, prd: period, now: current time. Return true if expired
static inline bool timer_expired(uint32_t *t, uint32_t prd, uint32_t now) {
  if (now + prd < *t) *t = 0;                    // Time wrapped? Reset timer
  if (*t == 0) *t = now + prd;                   // First poll? Set expiration
  if (*t > now) return false;                    // Not expired yet, return
  *t = (now - *t) > prd ? now + prd : *t + prd;  // Next expiration time
  return true;                                   // Expired, return true
}


#endif /* MCU_H_ */
