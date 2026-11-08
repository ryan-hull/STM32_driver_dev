/*
 * gpio.c
 *
 *  Created on: Nov 5, 2026
 *      Author: rynos
 */

#include "gpio.h"

drv_status_t gpio_init(const gpio_config_t *cfg) {
  // Sanity check for out of range inputs.
  if (!cfg || !cfg->pin.port || cfg->pin.num > 15 ||
      cfg->mode  > DRV_GPIO_MODE_ANALOG ||
      cfg->otype > DRV_GPIO_OTYPE_OPEN_DRAIN ||
	  cfg->pull  > DRV_GPIO_PULLDOWN ||
	  cfg->speed > DRV_GPIO_OSPEED_FAST ||
	  cfg->af > 15) return DRV_BAD_PARAM;

  // MODER, OSPEEDR and PUPDR are 2 bits per pin, so shift2 allows us to easily track offsets
  uint32_t n = cfg->pin.num, shift2 = n * 2;
  GPIO_TypeDef *g = cfg->pin.port;

  uint32_t moder = g->MODER;               // read mode register

  //       clear both bits              then set to new mode
  moder = (moder & ~(3U << shift2)) | ((uint32_t) cfg->mode << shift2);

  // OTYPER is 1 bit per pin, clear and set
  g->OTYPER  = (g->OTYPER  & ~(1U << n))      | ((uint32_t) cfg->otype << n);

  g->OSPEEDR = (g->OSPEEDR & ~(3U << shift2)) | ((uint32_t) cfg->speed << shift2);
  g->PUPDR   = (g->PUPDR   & ~(3U << shift2)) | ((uint32_t) cfg->pull  << shift2);



  if (cfg->mode == DRV_GPIO_MODE_AF) {
    // (n & 7) selects the slot
    // (n & 7) * 4 selects the slot position (because each slot is 4 bits).
    uint32_t s4 = (n & 7) * 4;

    // AFR has two registers per pin.
    // n >> 3 is the same as integer dividing by 8, and selects the register

    // & ~(15U << s4) creates a 4 bit mask, inverts it and clears all 4 bits in AFR
    // ((uint32_t) cfg->af << s4) sets the new 4 bit value
    g->AFR[n >> 3] = (g->AFR[n >> 3] & ~(15U << s4)) | ((uint32_t) cfg->af << s4);
  }

  g->MODER = moder; // switch mode last
  return DRV_OK;
}


drv_status_t gpio_port_enable(GPIO_TypeDef *port) {
  uint32_t bit;

  // map each of the different ports to their enable registers:
  if      (port == GPIOA) bit = RCC_AHB1ENR_GPIOAEN;
  else if (port == GPIOB) bit = RCC_AHB1ENR_GPIOBEN;
  else if (port == GPIOC) bit = RCC_AHB1ENR_GPIOCEN;
  else if (port == GPIOD) bit = RCC_AHB1ENR_GPIODEN;
  else if (port == GPIOH) bit = RCC_AHB1ENR_GPIOHEN;
  else return DRV_BAD_PARAM;

  // set the enable register bit
  RCC->AHB1ENR |= bit;

  // it takes two clock cycles to enable a port register.
  // Also, a read cannot complete until the write is complete,
  // so reading the same value back blocks until enable is done
  // guaranteeing everything is ready to continue.
  (void) RCC->AHB1ENR;
  return DRV_OK;
}


