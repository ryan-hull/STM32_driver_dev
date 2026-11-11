/*
 * system.c
 *
 *  Created on: Nov 11, 2026
 *      Author: rynos
 */


#include "stm32f4xx.h"

void SystemInit(void) {
  SCB->CPACR |= (0xFUL << 20);   // enable FPU
  __DSB();
  __ISB();
}
