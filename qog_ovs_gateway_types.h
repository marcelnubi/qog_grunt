/*
 * qog_ovs_gateway_types.h
 *
 *  Created on: Dec 1, 2016
 *      Author: Marcel
 */

#ifndef INC_QOG_OVS_GATEWAY_TYPES_H_
#define INC_QOG_OVS_GATEWAY_TYPES_H_

#include "qog_gateway_config.h"
#include "OVS_Channel.pb.h"
#include "OVS_ChannelNumberData.pb.h"

#include "FreeRTOS.h"

#include "task.h"
#include "queue.h"
#include "semphr.h"

typedef SemaphoreHandle_t qog_Mutex;
typedef TaskFunction_t qog_Task;
typedef QueueHandle_t qog_Queue;

typedef struct
{
	uint8_t WLAN_SSID[32];
	uint8_t WLAN_PSK[63];
	uint8_t WLAN_AUTH;
} WLANConnectionParams;

typedef struct
{
	uint8_t HostName[32];
	uint16_t HostPort;
	uint8_t Username[32];
	uint8_t Password[32];
} BrokerParams;

typedef Channel DataChannel;

typedef enum
{
	GW_STARTING = 0, GW_AP_CONFIG_MODE, GW_WLAN_CONNECTED, GW_BROKER_CONNECTED, GW_ERROR
} GatewayStatus;

typedef struct Gateway Gateway;

typedef struct
{
	qog_Task (*Task)(Gateway *gwInst);
	uint32_t requestedHeap;
	TaskHandle_t Handle;
} qog_gateway_task;

typedef struct
{
	qog_gateway_task * WifiTask;
	qog_gateway_task * MQTTClientTask;
	qog_gateway_task * MQTTPublisherTask;
	qog_gateway_task * DataSourceTaskGroup[MAX_DATA_SOURCE_TASKS];
} GatewayTasks;

struct Gateway
{
	WLANConnectionParams WLANConnection;
	BrokerParams BrokerParams;
	DataChannel DataChannels[MAX_DATA_CHANNELS];
	GatewayStatus Status;
	GatewayTasks Tasks;
	qog_Queue  DataUsedQueue;
	qog_Queue  DataAvailableQueue;
	qog_Queue  SocketRxQueue;
	qog_Queue  SocketTxQueue;
	ChannelNumberData  DataSampleBuffer[MAX_SAMPLE_BUFFER_SIZE];
	qog_Mutex  MQTTMutex;
};

#endif /* INC_QOG_OVS_GATEWAY_TYPES_H_ */
