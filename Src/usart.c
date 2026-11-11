/*
 * usart.c
 *
 *  Created on: Nov 6, 2026
 *      Author: rynos
 */
#include <stddef.h>
#include <stdlib.h>
#include "stdio.h"
#include "usart.h"
#include "mcu.h"

// add other usart pointers if needed
static uart_t *s_usart1 = NULL;
static uart_t *s_usart2 = NULL;
static uart_t *s_usart6 = NULL;

drv_status_t uart_init(uart_t *console, uart_config_t *console_cfg, uint32_t timeout) {
	//sanity check inputs:
	if (!console || !console_cfg || console_cfg->baud == 0 ) return DRV_BAD_PARAM;

	// map the console to the file-level static.
	if (console_cfg->instance == USART1) s_usart1 = console;
	if (console_cfg->instance == USART2) s_usart2 = console;
	if (console_cfg->instance == USART6) s_usart6 = console;

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
    uint32_t brr = (console_cfg->pclk_hz + console_cfg->baud / 2) / console_cfg->baud;
    console_cfg->instance->BRR = brr;

    // verify within 2% error
    uint64_t left = (abs(console_cfg->pclk_hz - brr * console_cfg->baud) * 50);
    uint64_t right = (brr * console_cfg->baud);
    if ( (abs(console_cfg->pclk_hz - brr * console_cfg->baud) * 50) > (brr * console_cfg->baud)) {
    	return DRV_BAUD_ERROR;
    }


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
    //flip the interrupt enable bit:
    console_cfg->instance->CR1 |= USART_CR1_RXNEIE;
    // enable the usart2 read interrupt
    if (console_cfg->instance == USART1) NVIC_EnableIRQ(USART1_IRQn);
	if (console_cfg->instance == USART2) NVIC_EnableIRQ(USART2_IRQn);
	if (console_cfg->instance == USART6) NVIC_EnableIRQ(USART6_IRQn);


    // assign to console
    console->instance = console_cfg->instance;
    console->rx_buf = console_cfg->rx_buf;
    console->rx_size = console_cfg->rx_size;

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

drv_status_t uart_read(uart_t *console, uint8_t *buf, size_t length, size_t *received, uint32_t timeout) {

	uint32_t start = millis();

	for (int i = 0; i < length; ++i) {
		// wait until data is in the buffer
		while (console->rx_head == console->rx_tail) {
			if (timeout == 0 || (timeout > 0 && (millis() - start) > timeout)) return DRV_TIMEOUT;
		}

		uint8_t data = console->rx_buf[console->rx_tail & (console->rx_size-1)]; //cut off the parity bit if 9 bit words
		console->rx_tail++;
		buf[i] = data;
		*received++;

		printf("byte read from ring buffer: %u\r\n", data);
	}

	return DRV_OK;
}


void USART1_IRQHandler(void) {
	// interrupt received, so bit already received and don't need to wait-block
	uint8_t status = s_usart1->instance->SR;
	uint8_t data = (uint8_t) (s_usart1->instance->DR & 0xFF); //cut off the parity bit if 9 bit words

	if ((status & USART_SR_PE) | (status & USART_SR_FE) | (status & USART_SR_NE) | (status & USART_SR_ORE)) {
//		if (status & USART_SR_PE) printf("parity error\r\n");
//		if (status & USART_SR_FE) printf("framing error\r\n");
//		if (status & USART_SR_NE) printf("noise error\r\n");
//		if (status & USART_SR_ORE) printf("overrun error\r\n");
		// parity error, framing error, noise error, overrun error:
		s_usart1->error_flags += 1;
	} else {
		// move the byte to the ring buffer. index is tail
		if (s_usart1->rx_head - s_usart1->rx_tail == s_usart1->rx_size) {
			s_usart1->overflow_count++;
//			printf("buffer overflow | count: %lu\r\n", s_usart1->overflow_count);
		} else {
			uint32_t index = s_usart1->rx_head & (s_usart1->rx_size -1);
			s_usart1->rx_buf[s_usart1->rx_head & (s_usart1->rx_size -1)] = data;
			s_usart1->rx_head++;
		}
	}
}

// handle usart2 read interrupts
void USART2_IRQHandler(void) {
	// interrupt received, so bit already received and don't need to wait-block
	uint8_t status = s_usart2->instance->SR;
	uint8_t data = (uint8_t) (s_usart2->instance->DR & 0xFF); //cut off the parity bit if 9 bit words

	if ((status & USART_SR_PE) | (status & USART_SR_FE) | (status & USART_SR_NE) | (status & USART_SR_ORE)) {
		if (status & USART_SR_PE) printf("parity error\r\n");
		if (status & USART_SR_FE) printf("framing error\r\n");
		if (status & USART_SR_NE) printf("noise error\r\n");
		if (status & USART_SR_ORE) printf("overrun error\r\n");
		// parity error, framing error, noise error, overrun error:
		s_usart2->error_flags += 1;
	} else {
		// move the byte to the ring buffer. index is tail
		if (s_usart2->rx_head - s_usart2->rx_tail == s_usart2->rx_size) {
			s_usart2->overflow_count++;
			printf("buffer overflow | count: %lu\r\n", s_usart2->overflow_count);
		} else {
			uint32_t index = s_usart2->rx_head & (s_usart2->rx_size -1);
			s_usart2->rx_buf[s_usart2->rx_head & (s_usart2->rx_size -1)] = data;
			s_usart2->rx_head++;
		}
	}
}


// override __io_putchar to redirect printf to my uart write.
extern int __io_putchar(int ch) {
	uint8_t c = (uint8_t) ch;
	uart_write(s_usart1, &c, 1, UART_TIMEOUT_DEFAULT_MS);
	return ch;
}

