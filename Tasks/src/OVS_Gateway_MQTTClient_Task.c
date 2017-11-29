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
#include "qog_gateway_util.h"
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
static void publishEdgelist(GatewayCommand*);
static void publishEdgeAdd(GatewayCommand*);
static void publishEdgeDrop(GatewayCommand*);
static void publishEdgeUpdate(GatewayCommand*);
static void publishGatewayStatus(GatewayCommand*);
static void publishData();

static enum {
	MQTT_CLIENT_RESET = 0, MQTT_CLIENT_CONNECTED, MQTT_CLIENT_DISCONNECTED
} MQTTClientState;

//Private functions

static bool publishMessage(MQTTMessage* msg, uint8_t* topic) {
	uint8_t retry = MQTT_CLIENT_PUBLISH_RETRY;
//	TickType_t finish = 0;
//	TickType_t start = xTaskGetTickCount();
	while (retry > 0) {
		if (!client.isconnected) {
			MQTTClientState = MQTT_CLIENT_DISCONNECTED;
			gw->Status = MQTT_CLIENT_DISCONNECTED;
			break;
		}

		if (MQTTPublish(&client, (char*) topic, msg) == MQTT_SUCCESS) {
			break;
		} else {
			retry--;
			gw->Diagnostics.MsgPublishFails++;
			if (retry == 0)
				return true;

			vTaskDelay(MQTT_CLIENT_PUBLISH_RETRY_DELAY_MS);
		}
	}
//	finish = xTaskGetTickCount() - start;
//	qog_gw_util_debug_msg("t:%d", finish);
	return false;
}

static void publishCommandMsg(uint8_t * msgBuf, uint8_t bufSize,
		uint8_t * topic) {
	MQTTMessage msg = { QOS2, false, false, 0, msgBuf, bufSize };

	if (publishMessage(&msg, topic)) {
		qog_gw_util_debug_msg("ERROR: Publish Command Fail, time=%d,topic=%s",
				gw->TimeStamp, topic);
	}
}

static void publishDataMsg(uint8_t * msgBuf, uint8_t bufSize, uint8_t * topic) {
	MQTTMessage msg = { QOS0, false, false, 0, msgBuf, bufSize };

	if (publishMessage(&msg, topic)) {
		qog_gw_util_debug_msg("ERROR: Publish Data Fail, time=%d,topic=%s",
				gw->TimeStamp, topic);
	}
}

static void MessageHandler(MessageData * data) {
	qog_gw_util_debug_msg("COMMAND ARRIVED, time=%d", gw->TimeStamp);
	//Edge Sync
	char* eval = strstr(data->topicName->lenstring.data, "Set");
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
	TickType_t xLastWakeTime;
	const TickType_t xFrequency = MQTT_TASK_LOOP_MS;

	gw = (Gateway*) gwInst;
	uint8_t gwTopic[OVS_MQTT_PUB_TOPIC_SIZE + OVS_MQTT_PUB_TOPIC_SZE_WILDCARD];
	network.SockRxQueue = gw->SocketRxQueue;
	network.SockTxQueue = gw->SocketTxQueue;
	NetworkInit(&network);

	MQTTClientInit(&client, &network, MQTT_TIMEOUT_MS, txBuf,
	MQTT_TX_BUFFER_SIZE, rxBuf, MQTT_RX_BUFFER_SIZE);
	GatewayId gid;
	qog_gw_sys_getUri(&gid);

	MQTTPacket_connectData conn = MQTTPacket_connectData_initializer;
	conn.username.cstring = (char*) gw->BrokerParams.Username;
	conn.password.cstring = (char*) gw->BrokerParams.Password;
	conn.clientID.cstring = (char*) gid.x;
	conn.cleansession = false;
	conn.keepAliveInterval = 120;

	// Initialise the xLastWakeTime variable with the current time.
	xLastWakeTime = xTaskGetTickCount();
	for (;;) {
		vTaskDelayUntil(&xLastWakeTime, xFrequency);
		switch (MQTTClientState) {
		case MQTT_CLIENT_RESET: {
			MQTTClientInit(&client, &network, MQTT_TIMEOUT_MS, txBuf,
			MQTT_TX_BUFFER_SIZE, rxBuf, MQTT_RX_BUFFER_SIZE);
			MQTTClientState = MQTT_CLIENT_DISCONNECTED;
			gw->Status = GW_MQTT_CLIENT_DISCONNECTED;
			gwInst->StopAll = true;
		}
			break;
		case MQTT_CLIENT_DISCONNECTED: {
			if (gw->Status == GW_BROKER_SOCKET_OPEN) {
				if (!MQTTConnect(&client, &conn)) {
					MQTTClientState = MQTT_CLIENT_CONNECTED;

					//TODO hello gateway
					sprintf((char*) gwTopic, "/gateway/%s/info", gid.x);
					MQTTMessage msg;
					msg.payload = "hello";
					msg.payloadlen = 5;
					msg.qos = QOS2;
					MQTTPublish(&client, (char*) gwTopic, &msg);

					//TODO subscribe on listening topics
					sprintf((char*) gwTopic, "/gateway/%s/cmd/+", gid.x);
					MQTTSubscribe(&client, (char*) gwTopic, QOS2,
							MessageHandler);
					gwInst->StopAll = false;
				} else {
					vTaskDelay(MQTT_CONNECT_RETRY_DELAY_MS - MQTT_TASK_LOOP_MS);
				}
			}
		}
			break;
		case MQTT_CLIENT_CONNECTED: {

			if (gw->Status != GW_BROKER_SOCKET_OPEN) {
				MQTTClientState = MQTT_CLIENT_RESET;
				break;
			}

			uint32_t avail = uxQueueSpacesAvailable(
					gw->DataSourceQs.DataAvailableQueue);

			while (avail-- > 0) {
				publishData();
				HAL_Delay(5);
			}

			//TODO ler fila de comandos OVS
			if (uxQueueMessagesWaiting(gw->CommandQueue)) {
				GatewayCommand dt = { };
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
						break;
					case GW_STATUS:
						publishGatewayStatus(&dt);
					default:
						break;
					}
				HAL_Delay(100);
			}
		}
			break;
		default:
			break;
		}

	}
	return 0;
}

void publishData() {
	uint8_t msgBuf[OVS_ChannelNumberData_size];
	uint8_t topic[OVS_MQTT_PUB_TOPIC_SIZE_DATA];

	OVS_ChannelNumberData *sample = NULL;

	if (gw->CB.gwPopNumberData(&sample) == false) {
		return;
	}
	//qog_gw_util_debug_msg("POP C:%d V:%2.2f Time:%d", sample->channelId,
	//		sample->numData.value, sample->numData.timestamp);
	//sample->has_numData = true;

	pb_ostream_t ostream = pb_ostream_from_buffer(msgBuf, sizeof(msgBuf));
	pb_encode(&ostream, OVS_ChannelNumberData_fields, sample);
	sprintf((char*) &topic, "/channel/%lu/protobuf/data", sample->channelId);

	publishDataMsg(msgBuf, ostream.bytes_written, topic);
}

void publishEdgelist(GatewayCommand* dt) {
	uint8_t msgBuf[OVS_EdgeId_size];
	uint8_t topic[OVS_MQTT_PUB_TOPIC_SIZE + OVS_MQTT_PUB_TOPIC_SZE_LIST];
	GatewayId gid;

	qog_gw_sys_getUri(&gid);
	Edge *eId = (Edge*) dt->pl;
	pb_ostream_t ostream = pb_ostream_from_buffer(msgBuf, sizeof(msgBuf));
	pb_encode(&ostream, OVS_EdgeId_fields, eId);
	sprintf((char*) &topic, "/gateway/%s/edge/list", gid.x);

	publishCommandMsg(msgBuf, ostream.bytes_written, topic);
}

void publishEdgeAdd(GatewayCommand* dt) {
	uint8_t msgBuf[OVS_EdgeId_size];
	uint8_t topic[OVS_MQTT_PUB_TOPIC_SIZE + OVS_MQTT_PUB_TOPIC_SZE_ADD];
	GatewayId gid;

	qog_gw_sys_getUri(&gid);
	Edge *ed = (Edge *) dt->pl;
	pb_ostream_t ostream = pb_ostream_from_buffer(msgBuf, sizeof(msgBuf));
	pb_encode(&ostream, OVS_EdgeId_fields, ed);
	sprintf((char*) &topic, "/gateway/%s/edge/add", gid.x);

	publishCommandMsg(msgBuf, ostream.bytes_written, topic);
}

void publishEdgeDrop(GatewayCommand* dt) {
	//TODO
}

void publishEdgeUpdate(GatewayCommand* dt) {
	uint8_t msgBuf[OVS_EdgeId_size];
	uint8_t topic[128];
	MQTTMessage msg;
	GatewayId gid;
	qog_gw_sys_getUri(&gid);

	pb_ostream_t ostream = pb_ostream_from_buffer(msgBuf, sizeof(msgBuf));
	pb_encode(&ostream, OVS_EdgeId_fields, (EdgeChannel*) dt->pl);
	msg.qos = QOS2;
	msg.payload = msgBuf;
	msg.payloadlen = ostream.bytes_written;
	msg.retained = 0;
	sprintf((char*) &topic, "/gateway/%s/edge/update", gid.x);
	uint8_t retry = MQTT_CLIENT_PUBLISH_RETRY;
	while (retry > 0) {
		if (!client.isconnected) {
			MQTTClientState = MQTT_CLIENT_RESET;
			gw->Status = MQTT_CLIENT_DISCONNECTED;
			break;
		}
		if (MQTTPublish(&client, (char*) topic, &msg) == MQTT_SUCCESS) {
			break;
		}
		retry--;
		vTaskDelay(MQTT_CLIENT_PUBLISH_RETRY_DELAY_MS);
	}
}

void publishGatewayStatus(GatewayCommand* dt) {
	uint8_t msgBuf[OVS_EdgeId_size];
	uint8_t topic[OVS_MQTT_PUB_TOPIC_SIZE + OVS_MQTT_PUB_TOPIC_SZE_LIST];
	GatewayId gid;

	qog_gw_sys_getUri(&gid);
	GatewayDiagnostics * gwDiag = (GatewayDiagnostics *) dt->pl;
	pb_ostream_t ostream = pb_ostream_from_buffer(msgBuf, sizeof(msgBuf));
	pb_encode(&ostream, OVS_GatewayStatus_fields, gwDiag);
	sprintf((char*) &topic, "/gateway/%s/status", gid.x);

	publishCommandMsg(msgBuf, ostream.bytes_written, topic);
}
#endif
