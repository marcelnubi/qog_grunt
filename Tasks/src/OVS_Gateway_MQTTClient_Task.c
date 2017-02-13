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

void NetworkInit(Network* n)
{
	n->my_socket = 0;
	n->mqttread = FreeRTOS_read;
	n->mqttwrite = FreeRTOS_write;
	n->disconnect = FreeRTOS_disconnect;
}

void TimerInit(Timer* timer)
{
	timer->xTicksToWait = 0;
	memset(&timer->xTimeOut, '\0', sizeof(timer->xTimeOut));
}

void MutexInit(Mutex* mutex)
{
	mutex->sem = xSemaphoreCreateMutex();
}

int FreeRTOS_read(Network* n, unsigned char* buffer, int len, int timeout_ms)
{
	TickType_t xTicksToWait = timeout_ms / portTICK_PERIOD_MS; /* convert milliseconds to ticks */
	TimeOut_t xTimeOut;
	int recvLen = 0;

	vTaskSetTimeOutState(&xTimeOut); /* Record the time at which this function was entered. */
	do
	{
		if (xQueueReceive(*n->SockRxQueue, buffer, xTicksToWait/10))
		{
			recvLen++;
			buffer++;
		}
	} while (recvLen < len
			&& xTaskCheckForTimeOut(&xTimeOut, &xTicksToWait) == pdFALSE);

	return recvLen;
}

int FreeRTOS_write(Network* n, unsigned char* buffer, int len, int timeout_ms)
{
	TickType_t xTicksToWait = timeout_ms / portTICK_PERIOD_MS; /* convert milliseconds to ticks */
	TimeOut_t xTimeOut;
	int sentLen = 0;

	vTaskSetTimeOutState(&xTimeOut); /* Record the time at which this function was entered. */
	do
	{
		int rc = 0;

//		FreeRTOS_setsockopt(n->my_socket, 0, FREERTOS_SO_RCVTIMEO, &xTicksToWait, sizeof(xTicksToWait));
//		rc = FreeRTOS_send(n->my_socket, buffer + sentLen, len - sentLen, 0);
		rc = xQueueSend(*n->SockTxQueue, buffer++, xTicksToWait/10);
		if (rc > 0)
			sentLen++;
		else if (rc == 0)
		{
			break;
		}
	} while (sentLen < len
			&& xTaskCheckForTimeOut(&xTimeOut, &xTicksToWait) == pdFALSE);

	return sentLen;
}

void FreeRTOS_disconnect(Network* n)
{
//	FreeRTOS_closesocket(n->my_socket);
}

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
