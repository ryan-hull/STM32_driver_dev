/*
 * i2c.h
 *
 *  Created on: Nov 11, 2026
 *      Author: rynos
 */

#ifndef I2C_H_
#define I2C_H_

#include <stddef.h>
#include <stdint.h>
#include "drivers.h"
#include "stm32f401xe.h"
#include "gpio.h"

typedef struct {
  I2C_TypeDef *instance;  // I2C instance (I2C1, I2C2, I2C3)
  uint8_t addr;
  const gpio_pin_t scl, sda; // (for I2C1: D15/PB8, D14/PB9)
  uint8_t af;                // (for I2C: 4)
  uint32_t cr2_freq;         // clock speed in MHz (must match the configured clock speed, 16 @ reset)
  uint32_t ccr;              // scl frequency @100kHz (pclk/(2SCL)) @400kHz(pclk/3SCL)
  uint32_t trise;            // max rise time (cycles) + 1.  (FLOOR(tr * pclk) + 1). @100khz: tr=1000ns @400khz: tr=300ns. @16MHz/100kHz: 17 @16MHz/400kHz: 5
} i2c_config_t;

typedef struct {
  I2C_TypeDef *instance;
  uint8_t addr;
  volatile uint32_t error_count;   // (DRV-I2C-008)
} i2c_t;

typedef enum {
	WRITE_MODE,
	READ_MODE,
} i2c_mode_t;

// Read/Write/Write_Read (DRV-I2C-004)
/**
 * @brief init an i2c bus
 *
 * initializes an i2c bus using the settings in bus_cfg.
 *
 * @param[out]  bus     the i2c bus instance
 * @param[in]  bus_cfg  the config definitions for the bus.
 *
 * @retval  DRV_OK          init succeeded.
 * @retval  DRV_BAD_PARAM   a parameter value in the config is invalid.
 */
drv_status_t i2c_init(i2c_t *bus, i2c_config_t *bus_cfg);


/**
 * @brief write data to a peripheral on an i2c bus
 *
 * writes data to the specified address on the bus
 *
 * @param[in]  bus       the i2c bus instance
 * @param[in]  addr      the address of the target device
 * @param[in]  data      the data to write
 * @param[in]  length    the number of data bytes to send
 * @param[in]  timeout   the max wait time for write to succeed
 *
 * @retval  DRV_OK            write succeeded.
 * @retval  DRV_TIMEOUT       time limit exceeded.
 * @retval  DRV_BAD_CONTEXT   called from an interrupt. not permitted!
 * @retval  DRV_NACK          NACK error
 * @retval  DRV_ARLO          arbitration error, lost control of bus
 * @retval  DRV_BERR          bus error
 */
drv_status_t i2c_write(i2c_t *bus, uint8_t addr, const uint8_t *data, size_t length, uint32_t timeout);

/**
 * @brief read data from a peripheral on an i2c bus
 *
 * @param[in]  bus       the i2c bus instance
 * @param[in]  addr      the address of the target device
 * @param[out] data      the data rx buffer
 * @param[in]  length    the number of data bytes to send
 * @param[in]  timeout   the max wait time for write to succeed
 *
 * @retval  DRV_OK            read succeeded.
 * @retval  DRV_TIMEOUT       time limit exceeded.
 * @retval  DRV_BAD_CONTEXT   called from an interrupt. not permitted!
 * @retval  DRV_NACK          NACK error
 * @retval  DRV_ARLO          arbitration error, lost control of bus
 * @retval  DRV_BERR          bus error
 */
drv_status_t i2c_read(i2c_t *bus, uint8_t addr, uint8_t *data, size_t length, uint32_t timeout);

/**
 * @brief writes then reads data from a peripheral on an i2c bus without releasing control between read and write
 *
 * @param[in]  bus         the i2c bus instance
 * @param[in]  addr        the address of the target device
 * @param[in]  write_data  the data to send
 * @param[in]  write_len   the number of bytes to send
 * @param[out] read_data   the data rx buffer
 * @param[in]  read_len    the number of data bytes to receive
 * @param[in]  timeout   the max wait time for write to succeed
 *
 * @retval  DRV_OK            write_read succeeded.
 * @retval  DRV_TIMEOUT       time limit exceeded.
 * @retval  DRV_BAD_CONTEXT   called from an interrupt. not permitted!
 * @retval  DRV_NACK          NACK error
 * @retval  DRV_ARLO          arbitration error, lost control of bus
 * @retval  DRV_BERR          bus error
 */
drv_status_t i2c_write_read(i2c_t *bus, uint8_t addr, const uint8_t *write_data, size_t write_len, uint8_t *read_data, size_t read_len, uint32_t timeout );


// Register Write/Read for N bytes (DRV-I2C-005)

/**
 * @brief reads data from a specific register at an address on an i2c bus
 *
 * @param[in]  bus         the i2c bus instance
 * @param[in]  addr        the address of the target device
 * @param[in]  reg         the memory register to read
 * @param[out] data        the data rx buffer
 * @param[in]  length      the number of data bytes to receive
 * @param[in]  timeout     the max wait time for write to succeed
 *
 * @retval  DRV_OK            reg_read succeeded.
 * @retval  DRV_TIMEOUT       time limit exceeded.
 * @retval  DRV_BAD_CONTEXT   called from an interrupt. not permitted!
 * @retval  DRV_NACK          NACK error
 * @retval  DRV_ARLO          arbitration error, lost control of bus
 * @retval  DRV_BERR          bus error
 */
drv_status_t i2c_reg_read(i2c_t *bus, uint8_t addr, uint8_t reg, uint8_t *data, size_t length, uint32_t timeout );

/**
 * @brief writes data to a specific register at an address on an i2c bus
 *
 * @param[in]  bus         the i2c bus instance
 * @param[in]  addr        the address of the target device
 * @param[in]  reg         the memory register to read
 * @param[out] data        the data rx buffer
 * @param[in]  length      the number of data bytes to receive
 * @param[in]  timeout     the max wait time for write to succeed
 *
 * @retval  DRV_OK            reg_write read succeeded.
 * @retval  DRV_TIMEOUT       time limit exceeded.
 * @retval  DRV_BAD_CONTEXT   called from an interrupt. not permitted!
 * @retval  DRV_NACK          NACK error
 * @retval  DRV_ARLO          arbitration error, lost control of bus
 * @retval  DRV_BERR          bus error
 */
drv_status_t i2c_reg_write(i2c_t *bus, uint8_t addr, uint8_t reg, const uint8_t *data, size_t length, uint32_t timeout );


/**
 * @brief test an address to see if we get an ACK
 *
 * @param[in]  bus         the i2c bus instance
 * @param[in]  addr        the address of the target device
 * @param[in]  timeout     the max wait time for write to succeed
 *
 * @retval  DRV_OK            peripheral at addr responded with ACK.
 * @retval  DRV_TIMEOUT       time limit exceeded.
 * @retval  DRV_BAD_CONTEXT   called from an interrupt. not permitted!
 * @retval  DRV_NACK          NACK error
 * @retval  DRV_ARLO          arbitration error, lost control of bus
 * @retval  DRV_BERR          bus error
 */
drv_status_t i2c_probe(i2c_t *bus, uint8_t addr, uint32_t timeout);



#endif /* I2C_H_ */
