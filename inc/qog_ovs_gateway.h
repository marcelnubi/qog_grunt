/*
 * qog_ovs_gateway.h
 *
 *  Created on: Dec 1, 2016
 *      Author: Marcel
 */

#ifndef INC_QOG_OVS_GATEWAY_H_
#define INC_QOG_OVS_GATEWAY_H_

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

typedef SemaphoreHandle_t qog_Mutex;
typedef TaskFunction_t qog_Task;
typedef QueueHandle_t qog_Queue;

typedef struct
{
	uint32_t ChannelId;
	uint32_t Timestamp;
	double Value;
} qog_DataSample;

typedef struct
{
	qog_Queue DataAvailableQueue;
	qog_Queue DataUsedQueue;
} qog_DataSource_queues;

typedef struct
{
	uint32_t Period;
	uint8_t Parameters[32];
} qog_DataSource_config;

typedef struct
{
	qog_Task (*Task)(void *gwInst);
	uint32_t requestedHeap;
	TaskHandle_t Handle;
} qog_gateway_task;

void qog_ovs_gw_init();
void qog_ovs_gw_register_data_source(qog_gateway_task * task);
void qog_ovs_run();
#endif /* INC_QOG_OVS_GATEWAY_H_ */
