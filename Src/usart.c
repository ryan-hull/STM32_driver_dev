/*
 * usart.c
 *
 *  Created on: Nov 6, 2026
 *      Author: rynos
 */
#include "stdio.h"
#include "usart.h"
#include "mcu.h"

drv_status_t uart_init(uart_t *console, uart_config_t *console_cfg, uint32_t timeout) {
	//sanity check inputs:
	if (!console || !console_cfg || console_cfg->baud == 0 ) return DRV_BAD_PARAM;


	// enable the clock:
	if (console_cfg->instance == USART1 || console_cfg->instance == USART6) {
		uint32_t bit = (console_cfg->instance == USART1? RCC_APB2ENR_USART1EN : RCC_APB2ENR_USART6EN);
		RCC->APB2ENR |= bit;
		(void) RCC->APB2ENR;
	} else if (console_cfg->instance == USART2) {
		RCC->APB1ENR |= RCC_APB1ENR_USART2EN;
		(void) RCC->APB1ENR;
	} else {
		return DRV_BAD_PARAM;
	}

	// wait for transmit to end, then clear all old settings to disable
	// reminder: this will only work if the clock has been enabled above!
	uint32_t start = millis();
	while (!(console_cfg->instance->SR & USART_SR_TC)) {
		if (timeout > 0 && (millis()-start) > timeout) return DRV_TIMEOUT;
	}

	console_cfg->instance->CR1 = 0;
	console_cfg->instance->CR2 = 0;


	// these pin values are hardcoded. if we ever need to adjust them we should reconsider
	// how they're implemented. Maybe move the pin logic out of usart_init() and let the board
	// own them?

	// enable tx pin:
    gpio_config_t tx_config = {
		  .pin = console_cfg->tx_pin,
		  .mode = DRV_GPIO_MODE_AF,
		  .otype = DRV_GPIO_OTYPE_PUSH_PULL,
		  .pull = DRV_GPIO_NOPULL,
		  .speed = DRV_GPIO_OSPEED_LOW,
		  .af = console_cfg->af,
    };

    gpio_init(&tx_config);

    // enable rx pin
    gpio_config_t rx_config = {
    		.pin = console_cfg->rx_pin,
			.mode = DRV_GPIO_MODE_AF,
			.otype = DRV_GPIO_OTYPE_PUSH_PULL,
			.pull = DRV_GPIO_PULLUP,
			.speed = DRV_GPIO_OSPEED_LOW,
			.af = console_cfg->af,
    };

    gpio_init(&rx_config);

    // Set BRR. Assumes over8 == 0. If we need to change later, add support here.
    // BRR is round(pclk_hz/baud)
    console_cfg->instance->BRR = (console_cfg->pclk_hz + console_cfg->baud / 2) / console_cfg->baud;


    // set parity and stop bits (M, PCE, and PS)
    if (console_cfg->parity != UART_PARITY_NONE) {
    	console_cfg->instance->CR1 |= USART_CR1_M | USART_CR1_PCE;
      if (console_cfg->parity == UART_PARITY_ODD) console_cfg->instance->CR1 |= USART_CR1_PS;
    }

    // transmitter on:
    console_cfg->instance->CR1 |= USART_CR1_TE;

    // receiver on:
    console_cfg->instance->CR1 |= USART_CR1_RE;

    // enable:
    console_cfg->instance->CR1 |= USART_CR1_UE;

    // assign to console
    console->instance = console_cfg->instance;

	return DRV_OK;
}

drv_status_t uart_write(uart_t *console, uint8_t *buf, size_t length, uint32_t timeout) {

	uint32_t start = millis();

	for (int i = 0; i < length; i++) {
		// wait until DR is empty:
		while (!(console->instance->SR & USART_SR_TXE)) {
			// if timeout is set, check that we don't exceed it.
			if (timeout > 0 && (millis() - start) > timeout) return DRV_TIMEOUT;
		}

		console->instance->DR = buf[i];
	}

	return DRV_OK;
}

drv_status_t uart_read(uart_t *console, uint8_t *buf, size_t length, uint32_t timeout) {

	uint32_t start = millis();

	for (int i = 0; i < length; ++i) {
		// wait until DR is
		while (!(console->instance->SR & USART_SR_RXNE)) {
			if (timeout > 0 && (millis() - start) > timeout) return DRV_TIMEOUT;
		}

		uint8_t status = console->instance->SR;
		uint8_t data = (uint8_t) (console->instance->DR & 0xFF); //cut off the parity bit if 9 bit words

		if ((status & USART_SR_PE) | (status & USART_SR_FE) | (status & USART_SR_NE) | (status & USART_SR_ORE)) {
			// parity error, framing error, noise error:
			console->error_flags += 1;
		} else {

			buf[i] = data;
		}
		printf("byte: %u | errors: %lu\r\n", data, console->error_flags);
	}

	return DRV_OK;
}
