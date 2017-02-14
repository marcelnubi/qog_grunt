/*
 * OVS_Gateway_DataSource.c
 *
 *  Created on: Feb 14, 2017
 *      Author: Marcel
 */

#include "qog_ovs_gateway.h"
#include "qog_ovs_gateway_internal_types.h"

static qog_Task DataSourceTaskImpl(void * gwInst);

qog_gateway_task DataSourceTaskDef =
{ &DataSourceTaskImpl, 0x200, NULL };

static qog_Task DataSourceTaskImpl(void * gwInst)
{
	for (;;)
	{
		osDelay(1);
	}
	return 0;
}
