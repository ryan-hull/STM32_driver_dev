/*
 * bme280.h
 *
 *  Created on: Nov 13, 2026
 *      Author: rynos
 */

#ifndef BME280_H_
#define BME280_H_

typedef struct {
	i2c_t *bus;
	uint8_t addr;
} bme280_t;


#endif /* BME280_H_ */
