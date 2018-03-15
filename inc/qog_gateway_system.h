/*
 * qog_gateway_system.h
 *
 *  Created on: Oct 31, 2017
 *      Author: Marcel
 */

#ifndef INC_QOG_GATEWAY_SYSTEM_H_
#define INC_QOG_GATEWAY_SYSTEM_H_

#include "qog_ovs_gateway_internal_types.h"
#include "qog_gateway_error_types.h"

qog_gw_error_t qog_gw_sys_getUri(GatewayId *);
uint32_t qog_gw_sys_getTimestamp();
qog_gw_error_t qog_gw_sys_setTime(qog_DateTime *);
void qog_gw_sys_swReset();

extern void HAL_Delay(volatile uint32_t Delay);

#endif /* INC_QOG_GATEWAY_SYSTEM_H_ */
