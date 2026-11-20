/*
 * drivers.h
 *
 *  Created on: Nov 19, 2026
 *      Author: rynos
 */

#ifndef DRIVERS_H_
#define DRIVERS_H_

// Universal driver return states
typedef enum {
DRV_OK,
DRV_BAD_PARAM,
DRV_TIMEOUT,
DRV_BAUD_ERROR,
DRV_BAD_CONTEXT,
DRV_NACK,
DRV_ARLO,
DRV_BERR
} drv_status_t;


//Both of the following macros assume drv_status_t st is defined in the functions calling them.

//DRV_TRY just returns when status is not DRV_OK.
//Only use this when there's no state that needs to be unwound
#define DRV_TRY(expr)                           \
	do {                                        \
		drv_status_t st = (expr);                            \
		if (st != DRV_OK) return st;            \
	} while (0)

//DRV_CHECK uses goto to handle state that needs to
//be cleaned up in case of failure. Requires
// drv_status_t st to be defined before the call
//and a 'fail' section to go to.
#define DRV_CHECK(expr)                         \
	do {                                        \
		st = (expr);                            \
		if (st != DRV_OK) goto fail;            \
	} while (0)



#endif /* DRIVERS_H_ */
