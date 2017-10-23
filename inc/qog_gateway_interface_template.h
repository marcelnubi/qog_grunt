/*
 * qog_gateway_power_interface.h
 *
 *  Created on: 23 Oct 2017
 *      Author: marcel
 */
#ifndef INC_QOG_GATEWAY_INTERFACE_H_
#define INC_QOG_GATEWAY_INTERFACE_H_

/*
 * Power Management Pin Mapping
 */
#define _OVS_GW_PWR_WIFI_PORT PWR_EN_HP_GPIO_Port
#define _OVS_GW_PWR_WIFI_PIN PWR_EN_HP_Pin
#define _OVS_GW_PWR_WIFI_LOGIC_ACTIVE GPIO_PIN_SET

#define _OVS_GW_PWR_IOB_PORT PWR_EN_IOB_GPIO_Port
#define _OVS_GW_PWR_IOB_PIN PWR_EN_IOB_Pin
#define _OVS_GW_PWR_IOB_LOGIC_ACTIVE GPIO_PIN_SET

#define _OVS_GW_PWR_SYS_PORT PWR_EN_BAT_REG_GPIO_Port
#define _OVS_GW_PWR_SYS_PIN PWR_EN_BAT_REG_Pin
#define _OVS_GW_PWR_SYS_LOGIC_ACTIVE GPIO_PIN_RESET

#endif /* INC_QOG_GATEWAY_INTERFACE_H_ */
