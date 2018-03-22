/*
 * OVS_Gateway_LocalStorage.c
 *
 *  Created on: Feb 14, 2017
 *      Author: Marcel
 */

#include "qog_ovs_gateway_internal_types.h"
#include "qog_gateway_system.h"

static qog_Task LocalStorageTaskImpl(Gateway * gwInst);

qog_gateway_task LocalStorageTaskDef =
{ &LocalStorageTaskImpl, 128, NULL };

static qog_Task LocalStorageTaskImpl(Gateway * gwInst)
{
	for (;;)
	{
		HAL_Delay(1000);
	}
	return 0;
}
