/*
 * qog_gateway_util.h
 *
 *  Created on: 16 Nov 2017
 *      Author: marcel
 */

#ifndef INC_QOG_GATEWAY_UTIL_H_
#define INC_QOG_GATEWAY_UTIL_H_

#include "qog_ovs_gateway_internal_types.h"

bool qog_gw_util_isEdgeEqual(Edge * e1, Edge *e2);
void qog_gw_sys_debug_msg(char * str, ...);

#endif /* INC_QOG_GATEWAY_UTIL_H_ */
