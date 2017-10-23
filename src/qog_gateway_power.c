/*
 * qog_gateway_power.c
 *
 *  Created on: 23 Oct 2017
 *      Author: marcel
 */

#include <qog_gateway_interface.h>
#include "qog_gateway_power.h"
#include "gpio.h"

qog_gw_error_t qog_gw_pwr_wifi_enable() {
	HAL_GPIO_WritePin(_OVS_GW_PWR_WIFI_PORT, _OVS_GW_PWR_WIFI_PIN,
	_OVS_GW_PWR_WIFI_LOGIC_ACTIVE);
	return GW_e_OK;
}
qog_gw_error_t qog_gw_pwr_wifi_disable() {
	HAL_GPIO_WritePin(_OVS_GW_PWR_WIFI_PORT, _OVS_GW_PWR_WIFI_PIN,
			!_OVS_GW_PWR_WIFI_LOGIC_ACTIVE);
	return GW_e_OK;
}

qog_gw_error_t qog_gw_pwr_iob_enable() {
	HAL_GPIO_WritePin(_OVS_GW_PWR_IOB_PORT, _OVS_GW_PWR_IOB_PIN,
	_OVS_GW_PWR_IOB_LOGIC_ACTIVE);
	return GW_e_OK;
}
qog_gw_error_t qog_gw_pwr_iob_disable() {
	HAL_GPIO_WritePin(_OVS_GW_PWR_IOB_PORT, _OVS_GW_PWR_IOB_PIN,
			!_OVS_GW_PWR_IOB_LOGIC_ACTIVE);
	return GW_e_OK;
}

qog_gw_error_t qog_gw_pwr_system_enable() {
	HAL_GPIO_WritePin(_OVS_GW_PWR_SYS_PORT, _OVS_GW_PWR_SYS_PIN,
			_OVS_GW_PWR_SYS_LOGIC_ACTIVE);
	return GW_e_OK;
}
qog_gw_error_t qog_gw_pwr_system_disable() {
	HAL_GPIO_WritePin(_OVS_GW_PWR_SYS_PORT, _OVS_GW_PWR_SYS_PIN,
			!_OVS_GW_PWR_SYS_LOGIC_ACTIVE);
	return GW_e_OK;
}
