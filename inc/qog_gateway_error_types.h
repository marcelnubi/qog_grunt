/*
 * qog_gateway_error_types.h
 *
 *  Created on: Feb 15, 2017
 *      Author: Marcel
 */

#ifndef INC_QOG_GATEWAY_ERROR_TYPES_H_
#define INC_QOG_GATEWAY_ERROR_TYPES_H_

typedef enum
{
	GW_e_OK = 0,
	GW_e_WLAN_ERROR,
	GW_e_SOCKET_ERROR,
	GW_e_HOST_ERROR,
	GW_e_APPLICATION_ERROR
} qog_gw_error_t;

#endif /* INC_QOG_GATEWAY_ERROR_TYPES_H_ */
