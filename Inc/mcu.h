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

/**
 * @brief get current clock time (ms)
 *
 * returns a count of the milliseconds that have passed since power up.
 *
 * @return milliseconds since boot
 */
uint32_t millis(void);

/**
 * @brief blocking wait (ms)
 *
 * a millisecond-based blocking wait.
 *
 * @param  ms  the number of milliseconds to block.
 */
void delay_ms(uint32_t ms);


/**
 * @brief a resettable timer for repeating events
 *
 * checks a timer to see if it has expired. If it has then reset and begin counting again.
 *
 * @param[in,out]  t   the end time of the waiting period.
 * @param[in]  prd     the duration of the wait period (ms).
 * @param[in]  now     the current time
 *
 * @retval  false      timer not expired
 * @retval  true       timer expired
 */
static inline bool timer_expired(uint32_t *t, uint32_t prd, uint32_t now) {
  if (now + prd < *t) *t = 0;                    // uint wraparound -> timer reset
  if (*t == 0) *t = now + prd;                   // first check -> set expire time
  if (*t > now) return false;                    // not expired -> return false
  *t = (now - *t) > prd ? now + prd : *t + prd;  // time expired -> reset timer
  return true;                                   // return true
}


#endif /* MCU_H_ */
