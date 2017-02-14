/*
 * qog_ovs_gateway_internal_types.h
 *
 *  Created on: Feb 13, 2017
 *      Author: Marcel
 */

#ifndef QOG_OVS_GATEWAY_INTERNAL_TYPES_H_
#define QOG_OVS_GATEWAY_INTERNAL_TYPES_H_

#include "qog_gateway_config.h"
#include "OVS_Channel.pb.h"
#include "OVS_ChannelNumberData.pb.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

#include <stdint.h>
/*
 * qog_ovs_gateway.h
 *
 *  Created on: Dec 1, 2016
 *      Author: Marcel
 */

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

typedef struct Gateway Gateway;

typedef struct
{
	qog_Task (*Task)(Gateway *gwInst);
	uint32_t requestedHeap;
	TaskHandle_t Handle;
} qog_gateway_task;

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
	uint32_t HostIp;
	uint8_t Username[32];
	uint8_t Password[32];
} BrokerParams;

typedef Channel DataChannel;

typedef enum
{
	GW_ERROR = 0,
	GW_STARTING,
	GW_AP_CONFIG_MODE,
	GW_WLAN_DISCONNECTED,
	GW_WLAN_CONNECTED,
	GW_BROKER_DNS_RESOLVED,
	GW_BROKER_SOCKET_OPEN,
	GW_BROKER_SOCKET_CLOSED,
	GW_MQTT_CLIENT_CONNECTED,
	GW_MQTT_CLIENT_DISCONNECTED,

} GatewayStatus;

typedef struct Gateway Gateway;

typedef struct
{
	qog_gateway_task * WifiTask;
	qog_gateway_task * MQTTClientTask;
	qog_gateway_task * MQTTPublisherTask;
	qog_gateway_task * DataSourceTask;
	qog_gateway_task * LocalStorageTask;
} GatewayTasks;

struct Gateway
{
	WLANConnectionParams WLANConnection;
	BrokerParams BrokerParams;
	DataChannel DataChannels[MAX_DATA_CHANNELS];
	GatewayStatus Status;
	GatewayTasks Tasks;
	qog_DataSource_queues DataSourceQs;
	qog_Queue SocketRxQueue;
	qog_Queue SocketTxQueue;
	ChannelNumberData DataSampleBuffer[MAX_SAMPLE_BUFFER_SIZE];
	qog_Mutex MQTTMutex;
	qog_Mutex LocalStorageMutex;
};

#endif /* QOG_OVS_GATEWAY_INTERNAL_TYPES_H_ */
