/*
 * i2c.c
 *
 *  Created on: Nov 11, 2026
 *      Author: rynos
 */
#include <stdbool.h>
#include <stdint.h>
#include "i2c.h"
#include "mcu.h"

// add other i2c pointers if needed
static i2c_t *s_i2c1 = NULL;
static i2c_t *s_i2c2 = NULL;
static i2c_t *s_i2c3 = NULL;

// Wait until the bus is free. It is not free if the busy flag is set or if the stop bit is set.
static drv_status_t i2c_wait_for_bus(i2c_t *bus, uint32_t timeout){
	uint32_t start = millis();
	while ((bus->instance->SR1 & I2C_SR2_BUSY) | (bus->instance->CR1 & I2C_CR1_STOP)) {
		if (timeout > 0 && (millis()-start) > timeout) return DRV_TIMEOUT;
	}
	return DRV_OK;
}

// wait for a specific flag in SR1 to be set while watching for error conditions
static drv_status_t wait_sr1 (i2c_t *bus, uint32_t flag, uint32_t timeout) {
	uint32_t start = millis();
	for (;;) {
		uint32_t sr1 = bus->instance->SR1;
		if (sr1 & I2C_SR1_AF) return DRV_NACK;
		if (sr1 & I2C_SR1_BERR) return DRV_BERR;
		if (sr1 & I2C_SR1_ARLO) return DRV_ARLO;
		if (sr1 & flag) return DRV_OK;
		if (timeout > 0 && (millis()-start) > timeout) return DRV_TIMEOUT;
	}
}

// set and read the start bit, verify bus controller state established
static drv_status_t i2c_start(i2c_t *bus, uint32_t timeout) {
	uint32_t start = millis();

	//set start bit
	bus->instance->CR1 |= I2C_CR1_START;

	//read sr1 to activate
	(void) bus->instance->SR1;

	//verify start bit and bus controller established
	return wait_sr1(bus, I2C_SR1_SB, timeout - (millis() - start));
}

// set the stop bit
static void i2c_stop(i2c_t *bus) {
	//set stop
	bus->instance->CR1 |= I2C_CR1_STOP;
}

// set the address of the device to be read/write
static drv_status_t i2c_set_addr(i2c_t *bus, uint8_t addr, bool rx_mode) {
	//set DR = addr, LSB set to 0 for transmitter mode
	bus->instance->DR = ((uint8_t)addr << 1) | (rx_mode ? 1u : 0u);
	return DRV_OK;
}

// clear addr to send it
static drv_status_t i2c_clear_addr(i2c_t *bus) {
	// read SR1 and SR2 to clear addr
	(void) bus->instance->SR1;
	(void) bus->instance->SR2;

	return DRV_OK;
}

// send bytes to the
static drv_status_t i2c_send_bytes(i2c_t *bus, const uint8_t *data, size_t length, uint32_t timeout) {
	uint32_t start = millis();

	drv_status_t st = DRV_OK;

	for (size_t i=0; i < length; i++) {
		// wait for tx to be empty
		DRV_TRY(wait_sr1(bus, I2C_SR1_TXE, timeout - (millis() - start)));

		bus->instance->DR = data[i];
	}

	return st;
}

static drv_status_t i2c_receive_bytes(i2c_t *bus, uint8_t addr, uint8_t *data, size_t length, uint32_t timeout) {

	if (__get_IPSR() != 0) {
		return DRV_BAD_CONTEXT;   // blocking calls won't advance millis() -> freeze if called from interrupt
	}

	uint32_t start = millis();
	drv_status_t st = DRV_OK;

	// if length == 1 clear ack, set stop
	if (length == 1) {

		//disable interrupts! save current state:
		uint32_t primask = __get_PRIMASK();
		__disable_irq();
		bus->instance->CR1 &= ~I2C_CR1_ACK;
		bus->instance->CR1 |= I2C_CR1_STOP;

		//reenable interrupts
		__set_PRIMASK(primask);

		//send the address
		i2c_set_addr(bus, addr, true);
		i2c_clear_addr(bus);

		i2c_stop(bus);

		//wait for byte to arrive
		DRV_TRY(wait_sr1(bus, I2C_SR1_RXNE, timeout - (millis() - start)));

		// byte has arrived, read it
		*data = (uint8_t) (bus->instance->DR & 0xFF);



	} else if (length == 2) {
		// set ACK
		bus->instance->CR1 |= I2C_CR1_ACK;

		//set address
		i2c_set_addr(bus, addr, true);

		// wait until addr set to pull SCL low
		DRV_TRY(wait_sr1(bus, I2C_SR1_ADDR, timeout - (millis() - start)));

		//set ack low and pos high
		bus->instance->CR1 &= ~I2C_CR1_ACK;
		bus->instance->CR1 |= ~I2C_CR1_POS;

		//clear addr flag:
		i2c_clear_addr(bus);

		//wait for BTF high
		DRV_TRY(wait_sr1(bus, I2C_SR1_BTF, timeout - (millis() - start)));

		//set stop
		i2c_stop(bus);

		//read bytes 1 and 2
		data[0] = (uint8_t) (bus->instance->DR & 0xFF);
		data[1] = (uint8_t) (bus->instance->DR & 0xFF);

	} else {

		//set address
		i2c_set_addr(bus, addr, true);

		// set ACK
		bus->instance->CR1 |= I2C_CR1_ACK;

		//clear addr flag:
		i2c_clear_addr(bus);

		//read through N-2 bytes
		for (size_t i; i < length-3; i++){

			// wait until rxne is true
			DRV_TRY(wait_sr1(bus, I2C_SR1_RXNE, timeout - (millis() - start)));

			// data has arrived, read it
			data[i] = (uint8_t) (bus->instance->DR & 0xFF);
		}

		//wait until BTF:
		DRV_TRY(wait_sr1(bus, I2C_SR1_BTF, timeout - (millis() - start)));

		//set ack low
		bus->instance->CR1 &= ~I2C_CR1_ACK;

		// read n-2 byte
		data[length-3] = (uint8_t) (bus->instance->DR & 0xFF);

		//wait for btf
		DRV_TRY(wait_sr1(bus, I2C_SR1_BTF, timeout - (millis() - start)));

		//set stop
		i2c_stop(bus);

		//read final two bytes
		data[length-2] = (uint8_t) (bus->instance->DR & 0xFF);
		data[length-1] = (uint8_t) (bus->instance->DR & 0xFF);
	}

	return DRV_OK;
}

static void cleanup(i2c_t *bus){
	//free bus if we control it
	if (!(bus->instance->SR2 & I2C_SR2_MSL)) {
		i2c_stop(bus);
	}

	//clear error flags
	bus->instance->SR1 &= ~(I2C_SR1_AF | I2C_SR1_BERR | I2C_SR1_ARLO);
}



drv_status_t i2c_init(i2c_t *bus, i2c_config_t *bus_cfg) {
	//sanity check inputs:
	//todo finish param checks
	if (!bus || !bus_cfg ) return DRV_BAD_PARAM;

	// map the console to the file-level static.
	if (bus_cfg->instance == I2C1) s_i2c1 = bus;
	if (bus_cfg->instance == I2C2) s_i2c2 = bus;
	if (bus_cfg->instance == I2C3) s_i2c3 = bus;

	// reset then enable the clock:
	if (bus_cfg->instance == I2C1) {
		RCC->APB1ENR |= RCC_APB1ENR_I2C1EN; // enable the clock
		RCC->APB1RSTR |= RCC_APB1RSTR_I2C1RST; // set reset bit
		RCC->APB1RSTR &= ~RCC_APB1RSTR_I2C1RST; // release the bit
	} else if (bus_cfg->instance == I2C2) {
		RCC->APB1ENR |= RCC_APB1ENR_I2C2EN;
		RCC->APB1RSTR |= RCC_APB1RSTR_I2C2RST;
		RCC->APB1RSTR &= ~RCC_APB1RSTR_I2C2RST;

	} else if (bus_cfg->instance == I2C3) {
		RCC->APB1ENR |= RCC_APB1ENR_I2C3EN;
		RCC->APB1RSTR |= RCC_APB1RSTR_I2C3RST;
		RCC->APB1RSTR &= ~RCC_APB1RSTR_I2C3RST;
	} else {
		return DRV_BAD_PARAM;
	}

	(void) RCC->APB1ENR;

	// enable scl pin:
	gpio_config_t scl_config = {
		  .pin = bus_cfg->scl,
		  .mode = DRV_GPIO_MODE_AF,
		  .otype = DRV_GPIO_OTYPE_OPEN_DRAIN,
		  .pull = DRV_GPIO_NOPULL,
		  .speed = DRV_GPIO_OSPEED_LOW,
		  .af = bus_cfg->af,
	};

	gpio_init(&scl_config);

	// enable sda pin
	gpio_config_t sda_config = {
			.pin = bus_cfg->sda,
			.mode = DRV_GPIO_MODE_AF,
			.otype = DRV_GPIO_OTYPE_OPEN_DRAIN,
			.pull = DRV_GPIO_NOPULL,
			.speed = DRV_GPIO_OSPEED_LOW,
			.af = bus_cfg->af,
	};

	gpio_init(&sda_config);

	// set FREQ
	bus_cfg->instance->CR2 |= (I2C_CR2_FREQ | bus_cfg->cr2_freq);

	// set CCR
	bus_cfg->instance->CCR |= bus_cfg->ccr;

	// set TRISE
	bus_cfg->instance->TRISE |= bus_cfg->trise;

	// enable:
	bus_cfg->instance->CR1 |= I2C_CR1_PE;

	// assign to console
	bus->instance = bus_cfg->instance;

	return DRV_OK;
}


drv_status_t i2c_write(i2c_t *bus, uint8_t addr, const uint8_t *data, size_t length, uint32_t timeout){
	// blocking calls won't advance millis() -> freeze if called from interrupt
	if (__get_IPSR() != 0) {
		return DRV_BAD_CONTEXT;
	}

	uint32_t start = millis();

	drv_status_t st = DRV_OK;

	//wait until bus is free
	DRV_TRY(i2c_wait_for_bus(bus, timeout-(millis()-start)));

	//bus is free, begin
	DRV_CHECK(i2c_start(bus, timeout-(millis()-start)));

	//send the address
	DRV_CHECK(i2c_set_addr(bus, addr << 1, false));
	DRV_CHECK(i2c_clear_addr(bus));

	//send the data
	DRV_CHECK(i2c_send_bytes(bus, data, length, timeout-(millis()-start)));

	//complete, send stop
	i2c_stop(bus);

	return DRV_OK;

fail:
	cleanup(bus);
	return st;

}


drv_status_t i2c_read(i2c_t *bus, uint8_t addr, uint8_t *data, size_t length, uint32_t timeout) {
	// blocking calls won't advance millis() -> freeze if called from interrupt
	if (__get_IPSR() != 0) {
		return DRV_BAD_CONTEXT;
	}

	uint32_t start = millis();

	drv_status_t st = DRV_OK;

	//wait until bus is free
	DRV_TRY(i2c_wait_for_bus(bus, timeout-(millis()-start)));

	//send start
	DRV_CHECK(i2c_start(bus, timeout-(millis()-start)));

	//get data
	DRV_CHECK(i2c_receive_bytes(bus, addr, data, length, timeout-(millis()-start)));

	return st;

fail:
	cleanup(bus);
	return st;
}


drv_status_t i2c_write_read(i2c_t *bus, uint8_t addr, const uint8_t *write_data, size_t write_len, uint8_t *read_data, size_t read_len, uint32_t timeout ) {
	// blocking calls won't advance millis() -> freeze if called from interrupt
	if (__get_IPSR() != 0) {
		return DRV_BAD_CONTEXT;
	}

	uint32_t start = millis();

	drv_status_t st = DRV_OK;

	//wait until bus is free
	DRV_TRY(i2c_wait_for_bus(bus, timeout-(millis()-start)));

	//set start
	DRV_CHECK(i2c_start(bus, timeout));

	//send address
	DRV_CHECK(i2c_set_addr(bus, addr, false));
	DRV_CHECK(i2c_clear_addr(bus));

	//write data to address
	DRV_CHECK(i2c_send_bytes(bus, write_data, write_len, timeout - (millis()-start)));

	// wait for btf
	while (!(bus->instance->SR1 & I2C_SR1_BTF)) {
		if (timeout > 0 && (millis() - start) > timeout) return DRV_TIMEOUT;
	}

	//receive expected bytes
	DRV_CHECK(i2c_receive_bytes(bus, addr, read_data, read_len, timeout-(millis()-start)));
	return st;

fail:
	cleanup(bus);
	return st;
}

//Writes to a data to a specific peripheral register. For length > 1 assumes
//that the target device autoincrements the register address. For multi-byte writes
//on devices that do not auto-increment, write each byte separately.
drv_status_t i2c_reg_write(i2c_t *bus, uint8_t addr, uint8_t reg, const uint8_t *data, size_t length, uint32_t timeout) {
	// blocking calls won't advance millis() -> freeze if called from interrupt
	if (__get_IPSR() != 0) {
		return DRV_BAD_CONTEXT;
	}

	uint32_t start = millis();


	drv_status_t st = DRV_OK;

	//wait until bus is free
	DRV_TRY(i2c_wait_for_bus(bus, timeout-(millis()-start)));

	//start
	DRV_CHECK(i2c_start(bus, timeout));

	//send address
	DRV_CHECK(i2c_set_addr(bus, addr, false));
	DRV_CHECK(i2c_clear_addr(bus));

	//send register, then data
	DRV_CHECK(i2c_send_bytes(bus, &reg, 1, timeout - (millis()-start)));
	DRV_CHECK(i2c_send_bytes(bus, data, length, timeout-(millis()-start)));

	return st;
fail:
	cleanup(bus);
	return st;
}

//just a thin wrapper around write_read.
drv_status_t i2c_reg_read(i2c_t *bus, uint8_t addr, uint8_t reg, uint8_t *data, size_t length, uint32_t timeout) {
	// blocking calls won't advance millis() -> freeze if called from interrupt
	if (__get_IPSR() != 0) {
		return DRV_BAD_CONTEXT;
	}

	return i2c_write_read(bus, addr, &reg, 1, data, length, timeout);
}


drv_status_t i2c_probe(i2c_t *bus, uint8_t addr, uint32_t timeout) {
	// blocking calls won't advance millis() -> freeze if called from interrupt
	if (__get_IPSR() != 0) {
		return DRV_BAD_CONTEXT;
	}

	//return DRV if address ACKs else return NACK
	//start -> send address -> stop
	uint32_t start = millis();

	drv_status_t st = DRV_OK;

	//wait until bus is free
	DRV_TRY(i2c_wait_for_bus(bus, timeout-(millis()-start)));

	//bus is free, begin
	DRV_CHECK(i2c_start(bus, timeout-(millis()-start)));

	//send the address
	DRV_CHECK(i2c_set_addr(bus, addr, false));

	// wait until ADDR or AF is set. If AF, DRV_CHECK returns DRV_AF, otherwise continues to clear and DRV_OK
	DRV_CHECK(wait_sr1(bus, (I2C_SR1_ADDR | I2C_SR1_AF), timeout - (millis() - start)));

	DRV_CHECK(i2c_clear_addr(bus));

	//complete, send stop
	i2c_stop(bus);
	return st;

fail:
	cleanup(bus);
	return st;

}
