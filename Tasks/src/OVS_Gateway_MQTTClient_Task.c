/*
 * OVS_Gateway_MQTTClient_Task_FreeRTOS.c
 *
 *  Created on: Dec 5, 2016
 *      Author: Marcel
 */

#if defined(GW_MQTT_CLIENT_TASK)

#include "MQTTClient.h"
#include "MQTTPacket.h"

#define MQTT_TIMEOUT_MS 1000
#define MQTT_BUFFER_SIZE 512

//-------- Gateway Interface - START
#include "qog_ovs_gateway.h"

#define WIFI_TASK_HEAP 0x200

static qog_Task MQTTClientTaskImpl(Gateway * gwInst);

qog_gateway_task MQTTClientTaskDef =
{ &MQTTClientTaskImpl, WIFI_TASK_HEAP, NULL };

static qog_Task MQTTPublisherTaskImpl(Gateway * gwInst);

qog_gateway_task MQTTPublisherTaskDef =
{ &MQTTPublisherTaskImpl, WIFI_TASK_HEAP, NULL };
//-------- Gateway Interface - END

MQTTClient client =
{ 0 };
Network network =
{ 0 };

uint8_t rxBuf[MQTT_BUFFER_SIZE],txBuf[MQTT_BUFFER_SIZE];

static qog_Task MQTTClientTaskImpl(Gateway * gwInst)
{
	network.SockRxQueue = &gwInst->SocketRxQueue;
	network.SockTxQueue = &gwInst->SocketTxQueue;
	NetworkInit(&network);
	MQTTClientInit(&client,&network,MQTT_TIMEOUT_MS,txBuf,MQTT_BUFFER_SIZE,rxBuf,MQTT_BUFFER_SIZE);
}

static qog_Task MQTTPublisherTaskImpl(Gateway * gwInst)
{
	network.SockRxQueue = &gwInst->SocketRxQueue;
	network.SockTxQueue = &gwInst->SocketTxQueue;
	NetworkInit(&network);
	MQTTClientInit(&client,&network,MQTT_TIMEOUT_MS,txBuf,MQTT_BUFFER_SIZE,rxBuf,MQTT_BUFFER_SIZE);
}

#endif
