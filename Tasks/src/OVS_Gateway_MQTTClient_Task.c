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
#define MQTT_CONNECT_RETRY_DELAY_MS 5000
#define MQTT_CLIENT_PUBLISH_RETRY 5
#define MQTT_CLIET_PUBLISH_RETRY_DELAY_MS 100
#define MQTT_TASK_LOOP_MS 100

//-------- Gateway Interface - START
#include "qog_ovs_gateway_internal_types.h"

#define MQTT_PUBLISHER_TASK_HEAP 0x200

static qog_Task MQTTPublisherTaskImpl(Gateway * gwInst);

qog_gateway_task MQTTPublisherTaskDef = { &MQTTPublisherTaskImpl,
MQTT_PUBLISHER_TASK_HEAP, NULL };
//-------- Gateway Interface - END

MQTTClient client = { 0 };
Network network = { 0 };

uint8_t rxBuf[MQTT_BUFFER_SIZE], txBuf[MQTT_BUFFER_SIZE];
Gateway* gw = NULL;

static enum {
	MQTT_CLIENT_UNNINITIALIZED = 0,
	MQTT_CLIENT_CONNECTED,
	MQTT_CLIENT_DISCONNECTED
} MQTTClientState;

static void MessageHandler(MessageData * data) {

	//Edge Sync
	char* eval = strstr(data->topicName->lenstring.data, "Sync");
	if (eval != NULL) {
		OVS_Channel newChannel = OVS_Channel_init_default;
		pb_istream_t istream = pb_istream_from_buffer(data->message->payload,
				data->message->payloadlen);
		if (pb_decode(&istream, OVS_Channel_fields, &newChannel)) {
			if (gw->CB.gwUpdateEdge != NULL)
				gw->CB.gwUpdateEdge(&newChannel);
		}
	}

	eval = strstr(data->topicName->lenstring.data, "getList");
	if (eval != NULL) {
		if (gw->CB.gwGetEdgeList != NULL)
			gw->CB.gwGetEdgeList();
	}
}

static qog_Task MQTTPublisherTaskImpl(Gateway * gwInst) {
	pb_ostream_t ostream;
	MQTTMessage msg;
	uint8_t topic[32];
	uint8_t gwTopic[128];

	gw = (Gateway*) gwInst;
	Timer timer;
	network.SockRxQueue = gw->SocketRxQueue;
	network.SockTxQueue = gw->SocketTxQueue;
	NetworkInit(&network);
	MQTTClientInit(&client, &network, MQTT_TIMEOUT_MS, txBuf, MQTT_BUFFER_SIZE,
			rxBuf, MQTT_BUFFER_SIZE);

	MQTTPacket_connectData conn = MQTTPacket_connectData_initializer;
	conn.username.cstring = (char*) gw->BrokerParams.Username;
	conn.password.cstring = (char*) gw->BrokerParams.Password;
	conn.clientID.cstring = (char*) gw->Id.x;
	conn.cleansession = false;
	conn.keepAliveInterval = 30;

	for (;;) {
		vTaskDelay(MQTT_TASK_LOOP_MS);
		switch (MQTTClientState) {
		case MQTT_CLIENT_UNNINITIALIZED: {
			MQTTClientInit(&client, &network, MQTT_TIMEOUT_MS, txBuf,
			MQTT_BUFFER_SIZE, rxBuf, MQTT_BUFFER_SIZE);
			MQTTClientState = MQTT_CLIENT_DISCONNECTED;
		}
			break;
		case MQTT_CLIENT_DISCONNECTED: {
			if (gw->Status == GW_BROKER_SOCKET_OPEN) {
				if (!MQTTConnect(&client, &conn)) {
					MQTTClientState = MQTT_CLIENT_CONNECTED;

					//TODO hello gateway
					sprintf((char*) gwTopic, "/gateway/%s/Info", gw->Id.x);
					MQTTMessage msg;
					msg.payload = "hello";
					msg.payloadlen = 5;
					msg.qos = QOS2;
					MQTTPublish(&client, (char*) gwTopic, &msg);

					//TODO subscribe on listening topics
					sprintf((char*) gwTopic, "/gateway/%s/Edge/+", gw->Id.x);
					MQTTSubscribe(&client, (char*) gwTopic, QOS0,
							MessageHandler);

				} else {
					vTaskDelay(MQTT_CONNECT_RETRY_DELAY_MS - MQTT_TASK_LOOP_MS);
				}
			}
		}
			break;
		case MQTT_CLIENT_CONNECTED: {
			TimerInit(&timer);

#if defined(MQTT_TASK)
			MutexLock(&gw->MQTTMutex);
#endif
			TimerCountdownMS(&timer, 500); /* Don't wait too long if no traffic is incoming */
			MQTTYield(&client, 50);

			while (uxQueueSpacesAvailable(gw->DataSourceQs.DataUsedQueue)
					< OVS_NUMBER_DATA_BUFFER_SIZE) {
				uint8_t idx = 0;
				uint8_t msgBuf[OVS_ChannelNumberData_size];

				xQueueReceive(gw->DataSourceQs.DataUsedQueue, &idx, 0);

				OVS_ChannelNumberData * sample = &gw->DataSampleBuffer[idx];
				sample->has_numData = true;

				ostream = pb_ostream_from_buffer(msgBuf, sizeof(msgBuf));
				pb_encode(&ostream, OVS_ChannelNumberData_fields, sample);
				msg.qos = QOS0;
				msg.payload = msgBuf;
				msg.payloadlen = ostream.bytes_written;
				sprintf((char*) &topic, "/channel/%lu/protobuf/data",
						sample->channelId);

				uint8_t retry = MQTT_CLIENT_PUBLISH_RETRY;
				while (retry > 0) {
					if (!client.isconnected) {
						MQTTClientState = MQTT_CLIENT_DISCONNECTED;
						break;
					}
					if (MQTTPublish(&client, (char*) topic, &msg)
							== MQTT_SUCCESS) {
						xQueueSend(gw->DataSourceQs.DataAvailableQueue, &idx,
								0);
						break;
					}
					retry--;
					vTaskDelay(MQTT_CLIET_PUBLISH_RETRY_DELAY_MS);
				}

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
