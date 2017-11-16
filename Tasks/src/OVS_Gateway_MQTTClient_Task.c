/*
 * OVS_Gateway_MQTTClient_Task_FreeRTOS.c
 *
 *  Created on: Dec 5, 2016
 *      Author: Marcel
 */

#if defined(GW_MQTT_CLIENT_TASK)

#include "MQTTClient.h"
#include "MQTTPacket.h"

//-------- Gateway Interface - START
#include "qog_ovs_gateway_internal_types.h"
#include "qog_gateway_system.h"

static qog_Task MQTTPublisherTaskImpl(Gateway * gwInst);

qog_gateway_task MQTTPublisherTaskDef = { &MQTTPublisherTaskImpl,
MQTT_PUBLISHER_TASK_HEAP, NULL };
//-------- Gateway Interface - END

//Variables
MQTTClient client = { 0 };
Network network = { 0 };
uint8_t rxBuf[MQTT_RX_BUFFER_SIZE], txBuf[MQTT_TX_BUFFER_SIZE];
Gateway* gw = NULL;

//Private Functions
static void publishEdgelist(EdgeCommand*);
static void publishEdgeAdd(EdgeCommand*);
static void publishEdgeDrop(EdgeCommand*);
static void publishEdgeUpdate(EdgeCommand*);
static void publishData();

static enum {
	MQTT_CLIENT_UNNINITIALIZED = 0,
	MQTT_CLIENT_CONNECTED,
	MQTT_CLIENT_DISCONNECTED
} MQTTClientState;

static void MessageHandler(MessageData * data) {

	//Edge Sync
	char* eval = strstr(data->topicName->lenstring.data, "update");
	if (eval != NULL) {
		OVS_Channel newChannel = OVS_Channel_init_default;
		pb_istream_t istream = pb_istream_from_buffer(data->message->payload,
				data->message->payloadlen);
		if (pb_decode(&istream, OVS_Channel_fields, &newChannel)) {
			if (gw->CB.gwUpdateEdge != NULL)
				gw->CB.gwUpdateEdge(&newChannel);
		}
	}

	eval = strstr(data->topicName->lenstring.data, "GetList");
	if (eval != NULL) {
		if (gw->CB.gwGetEdgeList != NULL)
			gw->CB.gwGetEdgeList();
	}
}

static qog_Task MQTTPublisherTaskImpl(Gateway * gwInst) {
	uint8_t gwTopic[128];

	gw = (Gateway*) gwInst;
	Timer timer;
	network.SockRxQueue = gw->SocketRxQueue;
	network.SockTxQueue = gw->SocketTxQueue;
	NetworkInit(&network);
	MQTTClientInit(&client, &network, MQTT_TIMEOUT_MS, txBuf,
	MQTT_TX_BUFFER_SIZE, rxBuf, MQTT_RX_BUFFER_SIZE);

	MQTTPacket_connectData conn = MQTTPacket_connectData_initializer;
	conn.username.cstring = (char*) gw->BrokerParams.Username;
	conn.password.cstring = (char*) gw->BrokerParams.Password;
	conn.clientID.cstring = (char*) gw->Id.x;
	conn.cleansession = false;
	conn.keepAliveInterval = 120;

	for (;;) {
		vTaskDelay(MQTT_TASK_LOOP_MS);
		switch (MQTTClientState) {
		case MQTT_CLIENT_UNNINITIALIZED: {
			MQTTClientInit(&client, &network, MQTT_TIMEOUT_MS, txBuf,
			MQTT_TX_BUFFER_SIZE, rxBuf, MQTT_RX_BUFFER_SIZE);
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
					MQTTSubscribe(&client, (char*) gwTopic, QOS2,
							MessageHandler);

				} else {
					vTaskDelay(MQTT_CONNECT_RETRY_DELAY_MS - MQTT_TASK_LOOP_MS);
				}
			}
		}
			break;
		case MQTT_CLIENT_CONNECTED: {
//			TimerInit(&timer);

#if defined(MQTT_TASK)
			MutexLock(&gw->MQTTMutex);
#endif
//			TimerCountdownMS(&timer, 100); /* Don't wait too long if no traffic is incoming */
			MQTTYield(&client, 50);

			while (uxQueueSpacesAvailable(gw->DataSourceQs.DataUsedQueue)
					< MAX_SAMPLE_BUFFER_SIZE) {
				publishData();
			}

			//TODO ler fila de comandos OVS
			EdgeCommand dt = { };
			if (xQueueReceive(gw->CommandQueue, &dt, 0) != errQUEUE_EMPTY)
				switch (dt.Command) {
				case EDGE_LIST:
					publishEdgelist(&dt);
					break;
				case EDGE_ADD:
					publishEdgeAdd(&dt);
					break;
				case EDGE_DROP:
					publishEdgeDrop(&dt);
					break;
				case EDGE_UPDATE:
					publishEdgeUpdate(&dt);
				default:
					break;
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

//Private functions

void publishData() {
	uint8_t idx = 0;
	uint8_t msgBuf[OVS_ChannelNumberData_size];
	uint8_t topic[128];
	MQTTMessage msg;

	xQueueReceive(gw->DataSourceQs.DataUsedQueue, &idx, 0);
	OVS_ChannelNumberData* sample = &gw->DataSampleBuffer[idx];
	sample->has_numData = true;
	pb_ostream_t ostream = pb_ostream_from_buffer(msgBuf, sizeof(msgBuf));
	pb_encode(&ostream, OVS_ChannelNumberData_fields, sample);
	msg.qos = QOS0;
	msg.payload = msgBuf;
	msg.payloadlen = ostream.bytes_written;
	msg.retained = 0;
	sprintf((char*) &topic, "/channel/%lu/protobuf/data", sample->channelId);
	uint8_t retry = MQTT_CLIENT_PUBLISH_RETRY;
	while (retry > 0) {
		if (!client.isconnected) {
			MQTTClientState = MQTT_CLIENT_DISCONNECTED;
			break;
		}
		if (MQTTPublish(&client, (char*) topic, &msg) == MQTT_SUCCESS) {
			xQueueSend(gw->DataSourceQs.DataAvailableQueue, &idx, 0);
			break;
		}
		retry--;
		vTaskDelay(MQTT_CLIET_PUBLISH_RETRY_DELAY_MS);
	}
}

void publishEdgelist(EdgeCommand* dt) {
//	OVS_EdgeList list = OVS_EdgeList_init_zero;

	for (uint8_t edx = 0; edx < MAX_DATA_CHANNELS; edx++) {

		if (gw->EdgeChannels[edx].EdgeId.Type != OVS_EdgeType_NULL_EDGE) {
			uint8_t msgBuf[OVS_EdgeId_size];
			uint8_t topic[128];
			MQTTMessage msg;

			pb_ostream_t ostream = pb_ostream_from_buffer(msgBuf,
					sizeof(msgBuf));
			pb_encode(&ostream, OVS_EdgeId_fields,
					&gw->EdgeChannels[edx].EdgeId);
			msg.qos = QOS2;
			msg.payload = msgBuf;
			msg.payloadlen = ostream.bytes_written;
			msg.retained = 0;
			sprintf((char*) &topic, "/gateway/%s/Edge/List", gw->Id.x);
			uint8_t retry = MQTT_CLIENT_PUBLISH_RETRY;
			while (retry > 0) {
				if (!client.isconnected) {
					MQTTClientState = MQTT_CLIENT_DISCONNECTED;
					break;
				}
				if (MQTTPublish(&client, (char*) topic, &msg) == MQTT_SUCCESS) {
					break;
				}
				retry--;
				vTaskDelay(MQTT_CLIET_PUBLISH_RETRY_DELAY_MS);
			}

		}
	}
}

void publishEdgeAdd(EdgeCommand* dt) {
	uint8_t msgBuf[OVS_EdgeId_size];
	uint8_t topic[128];
	MQTTMessage msg;

	Edge *ed = (Edge *) dt->pl;
	pb_ostream_t ostream = pb_ostream_from_buffer(msgBuf, sizeof(msgBuf));
	pb_encode(&ostream, OVS_EdgeId_fields, ed);
	msg.qos = QOS2;
	msg.payload = msgBuf;
	msg.payloadlen = ostream.bytes_written;
	msg.retained = 0;
	sprintf((char*) &topic, "/gateway/%s/Edge/Add", gw->Id.x);
	uint8_t retry = MQTT_CLIENT_PUBLISH_RETRY;
	while (retry > 0) {
		if (!client.isconnected) {
			MQTTClientState = MQTT_CLIENT_DISCONNECTED;
			break;
		}
		if (MQTTPublish(&client, (char*) topic, &msg) == MQTT_SUCCESS) {
			break;
		}
		retry--;
		vTaskDelay(MQTT_CLIET_PUBLISH_RETRY_DELAY_MS);
	}
}
void publishEdgeDrop(EdgeCommand* dt) {
	uint8_t msgBuf[OVS_EdgeId_size];
	uint8_t topic[128];
	MQTTMessage msg;

	pb_ostream_t ostream = pb_ostream_from_buffer(msgBuf, sizeof(msgBuf));
	pb_encode(&ostream, OVS_EdgeId_fields, (Edge*) dt->pl);
	msg.qos = QOS2;
	msg.payload = msgBuf;
	msg.payloadlen = ostream.bytes_written;
	msg.retained = 0;
	sprintf((char*) &topic, "/gateway/%s/Edge/Drop", gw->Id.x);
	uint8_t retry = MQTT_CLIENT_PUBLISH_RETRY;
	while (retry > 0) {
		if (!client.isconnected) {
			MQTTClientState = MQTT_CLIENT_DISCONNECTED;
			break;
		}
		if (MQTTPublish(&client, (char*) topic, &msg) == MQTT_SUCCESS) {
			break;
		}
		retry--;
		vTaskDelay(MQTT_CLIET_PUBLISH_RETRY_DELAY_MS);
	}
}
void publishEdgeUpdate(EdgeCommand* dt) {
	uint8_t msgBuf[OVS_EdgeId_size];
	uint8_t topic[128];
	MQTTMessage msg;

	pb_ostream_t ostream = pb_ostream_from_buffer(msgBuf, sizeof(msgBuf));
	pb_encode(&ostream, OVS_EdgeId_fields, (EdgeChannel*) dt->pl);
	msg.qos = QOS2;
	msg.payload = msgBuf;
	msg.payloadlen = ostream.bytes_written;
	msg.retained = 0;
	sprintf((char*) &topic, "/gateway/%s/Edge/Update", gw->Id.x);
	uint8_t retry = MQTT_CLIENT_PUBLISH_RETRY;
	while (retry > 0) {
		if (!client.isconnected) {
			MQTTClientState = MQTT_CLIENT_DISCONNECTED;
			break;
		}
		if (MQTTPublish(&client, (char*) topic, &msg) == MQTT_SUCCESS) {
			break;
		}
		retry--;
		vTaskDelay(MQTT_CLIET_PUBLISH_RETRY_DELAY_MS);
	}
}
#endif
