/*
 * qog_gateway_system.c
 *
 *  Created on: Oct 31, 2017
 *      Author: Marcel
 */

#include "qog_gateway_system.h"

#if defined (STM32F091xC)
#include "stm32f0xx.h"
#endif

qog_gw_error_t qog_gw_sys_getUri(uint8_t * id) {
	uint8_t bf[12];

	memcpy(bf, (uint8_t *) UID_BASE, 12);
	sprintf((char*) id, "%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X",
			bf[0], bf[1], bf[2], bf[3], bf[4], bf[5], bf[6], bf[7], bf[8],
			bf[9], bf[10], bf[11]);
	id[25] = '\0';

	return GW_e_OK;
}
