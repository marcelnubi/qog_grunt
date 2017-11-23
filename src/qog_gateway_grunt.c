/*
 * qog_ovs_gateway.c
 *
 *  Created on: Dec 1, 2016
 *      Author: Marcel
 */

#include "cmsis_os.h"
#include "FreeRTOS.h"
#include "task.h"

#include "stdio.h"
#include "string.h"

#include "qog_gateway_config.h"
#include "qog_gateway_system.h"
#include "qog_ovs_gateway_internal_types.h"
#include "qog_gateway_util.h"

#include "winc_1500_lib/driver/include/m2m_wifi.h"
#include "winc_1500_lib/bsp/include/nm_bsp.h"

#include "Overseer_Connection.h"
#include "OVS_ChannelNumberData.pb.h"
#include "OVS_Channel.pb.h"

#include "OVS_Gateway_Task.h"

#include "gpio.h"

#define OVS_GRUNT_LOOP_MS 5000

static Gateway m_gateway;

void gwUpdateEdge(EdgeChannel * ch);
void gwGetEdgeList();
void gwAddEdge(Edge* ed);
void gwDropEdge(Edge* ed);
void gwPushNumberData(double val, uint32_t channel, uint32_t timestamp);
bool gwPopNumberData(OVS_ChannelNumberData ** out);

void GruntTaskImpl(void const * argument);

void gw_init_gateway() {
	m_gateway.Status = GW_STARTING;
	//TODO Retrieve NV Memory config

	//TODO Retrieve MQTT Broker config
	sprintf((char*) m_gateway.BrokerParams.HostName, OVS_BROKER_HOST);
	m_gateway.BrokerParams.HostPort = OVS_BROKER_PORT;
	sprintf((char*) m_gateway.BrokerParams.Username, "admin");
	sprintf((char*) m_gateway.BrokerParams.Password, "qognata");

	//TODO Sync Channel/Edge
	m_gateway.EdgeChannels[0].Enabled = true;
	m_gateway.EdgeChannels[0].Id = 1;
	m_gateway.EdgeChannels[0].Period = 1;

	m_gateway.EdgeChannels[1].Enabled = true;
	m_gateway.EdgeChannels[1].Id = 2;
	m_gateway.EdgeChannels[1].Period = 1;

	m_gateway.EdgeChannels[2].Enabled = true;
	m_gateway.EdgeChannels[2].Id = 3;
	m_gateway.EdgeChannels[2].Period = 1;

	m_gateway.EdgeChannels[3].Enabled = true;
	m_gateway.EdgeChannels[3].Id = 4;
	m_gateway.EdgeChannels[3].Period = 1;

	m_gateway.MQTTMutex = xSemaphoreCreateMutex();
	m_gateway.LocalStorageMutex = xSemaphoreCreateMutex();

	m_gateway.Status = GW_STARTING;

	m_gateway.Tasks.WifiTask = &WifiTaskDef;
	m_gateway.Tasks.MQTTPublisherTask = &MQTTPublisherTaskDef;
	m_gateway.Tasks.DataSourceTask = &DataSourceTaskDef;
	m_gateway.Tasks.LocalStorageTask = &LocalStorageTaskDef;
}

static void gw_init_shared_queues() {
	m_gateway.DataSourceQs.DataAvailableQueue = xQueueCreate(
			MAX_SAMPLE_BUFFER_SIZE, sizeof(uint8_t));
	m_gateway.DataSourceQs.DataUsedQueue = xQueueCreate(MAX_SAMPLE_BUFFER_SIZE,
			sizeof(uint8_t));

	uint8_t initq = 0;
	for (initq = 0; initq < MAX_SAMPLE_BUFFER_SIZE; initq++) {
		xQueueSend(m_gateway.DataSourceQs.DataAvailableQueue, (void * )&initq,
				100);
	}

	m_gateway.SocketRxQueue = xQueueCreate(TCP_RX_SOCKET_BUFFER_SIZE,
			sizeof(uint8_t));
	m_gateway.SocketTxQueue = xQueueCreate(TCP_TX_SOCKET_BUFFER_SIZE,
			sizeof(uint8_t));

	m_gateway.CommandQueue = xQueueCreate(OVS_COMMAND_QUEUE_SIZE,
			sizeof(EdgeCommand));
}

static void gw_init_tasks() {
	gw_init_shared_queues();

	osThreadDef(WifiTask, m_gateway.Tasks.WifiTask->Task, osPriorityNormal, 1,
			m_gateway.Tasks.WifiTask->requestedHeap);
	m_gateway.Tasks.WifiTask->Handle = osThreadCreate(osThread(WifiTask),
			&m_gateway);

	osThreadDef(MQTTPublisherTask, m_gateway.Tasks.MQTTPublisherTask->Task,
			osPriorityNormal, 1,
			m_gateway.Tasks.MQTTPublisherTask->requestedHeap);
	m_gateway.Tasks.MQTTPublisherTask->Handle = osThreadCreate(
			osThread(MQTTPublisherTask), &m_gateway);

	osThreadDef(DataSourceTask, m_gateway.Tasks.DataSourceTask->Task,
			osPriorityNormal, 1, m_gateway.Tasks.DataSourceTask->requestedHeap);
	m_gateway.Tasks.DataSourceTask->Handle = osThreadCreate(
			osThread(DataSourceTask), &m_gateway);
}

void gw_init_callbacks() {
	m_gateway.CB.gwUpdateEdge = &gwUpdateEdge;
	m_gateway.CB.gwAddEdge = &gwAddEdge;
	m_gateway.CB.gwDropEdge = &gwDropEdge;
	m_gateway.CB.gwGetEdgeList = &gwGetEdgeList;
	m_gateway.CB.gwPushNumberData = &gwPushNumberData;
	m_gateway.CB.gwPopNumberData = &gwPopNumberData;
}

void qog_ovs_run() {
	gw_init_gateway();
	gw_init_tasks();
	gw_init_callbacks();
}

//Callbacks
void gwUpdateEdge(EdgeChannel * ch) {
	EdgeCommand cmd = { };

	for (uint8_t idx = 0; idx < MAX_DATA_CHANNELS; idx++) {
		//Compare EdgeChannel
		if (qog_gw_util_isEdgeEqual(&m_gateway.EdgeChannels[idx].EdgeId,
				&ch->EdgeId)) {
			m_gateway.EdgeChannels[idx] = *ch;
			cmd.Command = EDGE_UPDATE;
			cmd.pl = &ch;
			break;
		}
	}

	if (cmd.pl == &ch)
		xQueueSend(m_gateway.CommandQueue, (void * )&cmd, 0);
}
void gwGetEdgeList() {
	for (uint8_t edx = 0; edx < MAX_DATA_CHANNELS; edx++) {
		if (m_gateway.EdgeChannels[edx].EdgeId.Type != OVS_EdgeType_NULL_EDGE) {
			EdgeCommand cmd = { };
			cmd.Command = EDGE_LIST;
			cmd.pl = &m_gateway.EdgeChannels[edx].EdgeId;
			xQueueSend(m_gateway.CommandQueue, (void * )&cmd, 0);
		}
	}
}
void gwAddEdge(Edge* ed) {
	EdgeCommand cmd = { };
	uint8_t idx;
	for (idx = 0; idx < MAX_DATA_CHANNELS; idx++) {
		if (m_gateway.EdgeChannels[idx].EdgeId.Type == OVS_EdgeType_NULL_EDGE) {
			m_gateway.EdgeChannels[idx].EdgeId = *ed;
			break;
		}
	}

	cmd.Command = EDGE_ADD;
	cmd.pl = &m_gateway.EdgeChannels[idx].EdgeId;
	xQueueSend(m_gateway.CommandQueue, (void * )&cmd, 0);
}
void gwDropEdge(Edge* ed) {
	EdgeCommand cmd = { };
	cmd.Command = EDGE_DROP;
	cmd.pl = ed;
	xQueueSend(m_gateway.CommandQueue, &cmd, 0);
}

void gwPushNumberData(double val, uint32_t channel, uint32_t timestamp) {
	uint8_t avail;
	xQueueReceive(m_gateway.DataSourceQs.DataAvailableQueue, &avail, 10);

	m_gateway.DataSampleBuffer[avail].channelId = channel;
	m_gateway.DataSampleBuffer[avail].numData.timestamp = timestamp;
	m_gateway.DataSampleBuffer[avail].numData.value = val;

	xQueueSend(m_gateway.DataSourceQs.DataUsedQueue, &avail, 10);
}

bool gwPopNumberData(OVS_ChannelNumberData ** out) {
	uint8_t idx = 0;
	if (xQueueReceive(m_gateway.DataSourceQs.DataUsedQueue, &idx,
			10) != errQUEUE_EMPTY) {
		*out = &m_gateway.DataSampleBuffer[idx];
		xQueueSend(m_gateway.DataSourceQs.DataAvailableQueue, &idx, 10);
		return true;
	}
	return false;
}

//RTOS Task
void GruntTaskImpl(void const * argument) {

	TickType_t xLastWakeTime;
	const TickType_t xFrequency = portTICK_PERIOD_MS * OVS_GRUNT_LOOP_MS;

	xLastWakeTime = xTaskGetTickCount();
	for (;;) {
		vTaskDelayUntil(&xLastWakeTime, xFrequency);
		if (m_gateway.Status == GW_BROKER_SOCKET_OPEN) {
//			gwAddEdge(&m_gateway.EdgeChannels[0].EdgeId);
//			gwUpdateEdge(&m_gateway.EdgeChannels[1]);
//			gwDropEdge(&m_gateway.EdgeChannels[0].EdgeId);
		}
	}
}

//ISR
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
	if (GPIO_Pin == WIFI_IRQ_N_Pin) {
		nm_bsp_isr_cb();
	}
}

//Debug helpers
#if defined(DEBUG)
void vApplicationStackOverflowHook(TaskHandle_t xTask, signed char *pcTaskName) {
	while (1) {
	}
}
#endif
