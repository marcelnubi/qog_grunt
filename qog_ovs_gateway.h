/*
 * qog_ovs_gateway.h
 *
 *  Created on: Dec 1, 2016
 *      Author: Marcel
 */

#ifndef INC_QOG_OVS_GATEWAY_H_
#define INC_QOG_OVS_GATEWAY_H_

#include "qog_gateway_config.h"
#include "qog_ovs_gateway_types.h"

void qog_ovs_gw_register_data_source(qog_Task (*dataSourceFn)(Gateway * gwInst), uint32_t heapSize);
void qog_ovs_run();
#endif /* INC_QOG_OVS_GATEWAY_H_ */
