/*
 * usart.h
 *
 *  Created on: Nov 5, 2026
 *      Author: rynos
 */

#ifndef USART_H_
#define USART_H_

#include <stddef.h>
#include <stdint.h>
#include "stm32f401xe.h"
#include "gpio.h"

typedef enum { UART_PARITY_NONE,
  UART_PARITY_EVEN,
  UART_PARITY_ODD
} uart_parity_t;

typedef enum {
  UART_STOP_1,
  UART_STOP_2
} uart_stop_t;

typedef struct {
  USART_TypeDef *instance;  // USART instance (USART1, USART2, USART6)
  uint32_t pclk_hz;         // clock for this USART (USART2 is APB1 @ 16 MHz at reset)
  uint32_t baud;            // options: 9600, 115200, 921600 (DRV-UART-001)
  uart_parity_t parity;     // parity + stop = frame settings. parity 0 + stop 0 = standard 8N1
  uart_stop_t stop;
  const gpio_pin_t tx_pin, rx_pin; // (for USART2: PA2, PA3)
  uint8_t af;                // (for USART2: 7)
  uint8_t *rx_buf;           // ring buffer storage (DRV-GEN-004)
  size_t rx_size;
} uart_config_t;

typedef struct {
  USART_TypeDef *instance;
  uint8_t *rx_buf;
  size_t rx_size;
  volatile size_t rx_head, rx_tail;   // volatile because written by ISR
  volatile uint32_t overflow_count;   // (DRV-UART-005)
  volatile uint32_t error_flags;      // (DRV-UART-006)
} uart_t;

drv_status_t uart_init(uart_t *console, uart_config_t *console_cfg, uint32_t timeout);
drv_status_t uart_write(uart_t *console, uint8_t *string, size_t length, uint32_t timeout);
drv_status_t uart_read(uart_t *console, uint8_t *buf, size_t length, uint32_t timeout);

#endif /* USART_H_ */
