/*
 * OVS_Gateway_LocalStorage.c
 *
 *  Created on: Feb 14, 2017
 *      Author: Marcel
 */

#include "qog_ovs_gateway_internal_types.h"

static qog_Task LocalStorageTaskImpl(Gateway * gwInst);

qog_gateway_task LocalStorageTaskDef =
{ &LocalStorageTaskImpl, 0x200, NULL };

static qog_Task LocalStorageTaskImpl(Gateway * gwInst)
{
	for (;;)
	{
		osDelay(1);
	}
	return 0;
}
