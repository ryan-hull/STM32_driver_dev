/*
 * board.h
 *
 *  Created on: Nov 4, 2026
 *      Author: rynos
 */

#ifndef BOARD_H_
#define BOARD_H_

#include <stm32f401xe.h>

typedef struct { GPIO_TypeDef *port; uint8_t num; } gpio_pin_t;

#define PIN_LED ((gpio_pin_t){ GPIOA, 5 })
#define PIN_UART2_Tx  ((gpio_pin_t){ GPIOA, 2 })
#define PIN_UART2_Rx  ((gpio_pin_t){ GPIOA, 3 })
#define UART_TIMEOUT_DEFAULT_MS 100

#endif /* BOARD_H_ */
