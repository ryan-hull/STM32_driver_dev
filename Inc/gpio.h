/*
 * gpio.h
 *
 *  Created on: Nov 4, 2026
 *      Author: rynos
 */

#ifndef GPIO_H_
#define GPIO_H_

#include <stdbool.h>
#include "board.h"
#include "drivers.h"


typedef enum {
  DRV_GPIO_MODE_INPUT  = 0,
  DRV_GPIO_MODE_OUTPUT = 1,
  DRV_GPIO_MODE_AF     = 2,
  DRV_GPIO_MODE_ANALOG = 3,
} drv_gpio_mode_t;

typedef enum {
  DRV_GPIO_OTYPE_PUSH_PULL = 0,
  DRV_GPIO_OTYPE_OPEN_DRAIN = 1,
} drv_gpio_otype_t;

typedef enum {
  DRV_GPIO_NOPULL = 0,
  DRV_GPIO_PULLUP = 1,
  DRV_GPIO_PULLDOWN = 2,
} drv_gpio_pull_t;

typedef enum {
  DRV_GPIO_OSPEED_LOW = 0,
  DRV_GPIO_OSPEED_MEDIUM = 1,
  DRV_GPIO_OSPEED_HIGH = 2,
  DRV_GPIO_OSPEED_FAST = 3,
} drv_gpio_ospeed_t;

typedef struct {
  gpio_pin_t pin;
  drv_gpio_mode_t mode;
  drv_gpio_otype_t otype;
  drv_gpio_pull_t pull;
  drv_gpio_ospeed_t speed;        // 0-3
  uint8_t af;           // 0-15, used when mode == GPIO_AF
} gpio_config_t;

drv_status_t gpio_init(const gpio_config_t *cfg);
drv_status_t gpio_port_enable(GPIO_TypeDef *port);


//Driver note: BSRR is the safe way to set or reset bits in an atomic way.
//We could do also it via ODR directly, but it would be a read-modify-write and not atomic

/**
 * @brief Sets a GPIO pin high or low.
 *
 * Sets GPIO pin p to true (high) or false (low).
 *
 * @param[in]  p  the GPIO pin to write.
 * @param[in]  v  the value to set.
 */
static inline void gpio_write(gpio_pin_t p, bool v) {
  p.port->BSRR = (1UL << p.num) << (v ? 0 : 16);
}


/**
 * @brief Toggles a GPIO pin high to low or low to high.
 *
 * Flips a pin to its opposite value (high to low) or (low to high).
 *
 * @param[in]  p  the GPIO pin to toggle.
 */
static inline void gpio_toggle(gpio_pin_t p) {
  uint32_t mask = 1UL << p.num;
  // if set, set the reset bit (num shifted 16 bits over).
  // if not set, set the set bit.
  p.port->BSRR = (p.port->ODR & mask) ? (mask << 16) : mask;
}

/**
 * @brief Gets a GPIO pin current statue.
 *
 * Reads the current state of a pin.
 *
 * @param[in]  p   the GPIO pin to write.

 * @retval  true   pin is set high.
 * @retval  false  pin is set low.
 */
static inline bool gpio_read(gpio_pin_t p) {
  return (p.port->IDR >> p.num) & 1UL;
}


#endif /* GPIO_H_ */
