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
#include "qog_ovs_gateway_internal_types.h"

#define MQTT_PUBLISHER_TASK_HEAP 0x200

static qog_Task MQTTPublisherTaskImpl(Gateway * gwInst);

qog_gateway_task MQTTPublisherTaskDef =
{ &MQTTPublisherTaskImpl, MQTT_PUBLISHER_TASK_HEAP, NULL };
//-------- Gateway Interface - END

MQTTClient client =
{ 0 };
Network network =
{ 0 };

uint8_t rxBuf[MQTT_BUFFER_SIZE], txBuf[MQTT_BUFFER_SIZE];

static enum
{
	MQTT_CLIENT_UNNINITIALIZED = 0,
	MQTT_CLIENT_CONNECTED,
	MQTT_CLIENT_DISCONNECTED
} MQTTClientState;

void MQTTHandler_Info(MessageData * data)
{
	uint32_t asd = 123;
	asd++;
}

static qog_Task MQTTPublisherTaskImpl(Gateway * gwInst)
{
	pb_ostream_t ostream;
	MQTTMessage msg;
	uint8_t topic[32];

	Gateway* gw = (Gateway*) gwInst;
	Timer timer;
	network.SockRxQueue = gw->SocketRxQueue;
	network.SockTxQueue = gw->SocketTxQueue;
	NetworkInit(&network);
	MQTTClientInit(&client, &network, MQTT_TIMEOUT_MS, txBuf, MQTT_BUFFER_SIZE,
			rxBuf, MQTT_BUFFER_SIZE);

	MQTTPacket_connectData conn = MQTTPacket_connectData_initializer;
	conn.username.cstring = (char*) gw->BrokerParams.Username;
	conn.password.cstring = (char*) gw->BrokerParams.Password;
	conn.clientID.cstring = "GWProto"; //TODO gw->id ?
	conn.cleansession = false;
	conn.keepAliveInterval = 30;

	for (;;)
	{
		vTaskDelay(1000);
		switch (MQTTClientState)
		{
		case MQTT_CLIENT_UNNINITIALIZED:
		{
			MQTTClientInit(&client, &network, MQTT_TIMEOUT_MS, txBuf,
			MQTT_BUFFER_SIZE, rxBuf, MQTT_BUFFER_SIZE);
			MQTTClientState = MQTT_CLIENT_DISCONNECTED;
		}
			break;
		case MQTT_CLIENT_DISCONNECTED:
		{
			if (gw->Status == GW_BROKER_SOCKET_OPEN)
			{
				if (!MQTTConnect(&client, &conn))
				{
					MQTTClientState = MQTT_CLIENT_CONNECTED;

					sprintf((char*) topic, "/%lu/Info", (uint32_t) gw->Id);
					MQTTSubscribe(&client, (char*) topic, QOS0,
							MQTTHandler_Info);
				} //TODO tratar erro de conexão MQTT
			}
		}
			break;
		case MQTT_CLIENT_CONNECTED:
		{
			TimerInit(&timer);

#if defined(MQTT_TASK)
			MutexLock(&gw->MQTTMutex);
#endif
			TimerCountdownMS(&timer, 500); /* Don't wait too long if no traffic is incoming */
			MQTTYield(&client, 50);

			while (uxQueueSpacesAvailable(gw->DataSourceQs.DataUsedQueue)
					< OVS_NUMBER_DATA_BUFFER_SIZE)
			{
				uint8_t idx = 0;
				uint8_t msgBuf[ChannelNumberData_size];

				xQueueReceive(gw->DataSourceQs.DataUsedQueue, &idx, 0);

				ChannelNumberData * sample = &gw->DataSampleBuffer[idx];

				ostream = pb_ostream_from_buffer(msgBuf, sizeof(msgBuf));
				pb_encode(&ostream, ChannelNumberData_fields, sample);
				msg.qos = QOS0;
				msg.payload = msgBuf;
				msg.payloadlen = ostream.bytes_written;
				sprintf((char*) &topic, "/channel/%lu/protobuf/data",
						sample->channelId);
				MQTTPublish(&client, (char*) topic, &msg); //TODO Tratar erro de envio de mensagem MQTT

				xQueueSend(gw->DataSourceQs.DataAvailableQueue, &idx, 0);
			}

#if defined(MQTT_TASK)
			MutexUnlock(&gw->MQTTMutex);
#endif
		}
			break;
		default:
			break;
		}

	}
	return 0;
}

#endif
