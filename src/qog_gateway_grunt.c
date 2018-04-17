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
#include "qog_gateway_power.h"
#include "qog_ovs_gateway_internal_types.h"
#include "qog_gateway_util.h"

#include "winc_1500_lib/driver/include/m2m_wifi.h"
#include "winc_1500_lib/bsp/include/nm_bsp.h"

#include "Overseer_Connection.h"
#include "OVS_ChannelNumberData.pb.h"
#include "OVS_Channel.pb.h"

#include "OVS_Gateway_Task.h"

#include "gpio.h"
#include "adc.h"

#define OVS_GRUNT_LOOP_MS 1000

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

	m_gateway.StopAll = true;

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

	m_gateway.EdgeChannels[4].Enabled = true;
	m_gateway.EdgeChannels[4].Id = 5;
	m_gateway.EdgeChannels[4].Period = 1;

	m_gateway.EdgeChannels[5].Enabled = true;
	m_gateway.EdgeChannels[5].Id = 6;
	m_gateway.EdgeChannels[5].Period = 1;
//
//	m_gateway.EdgeChannels[6].Enabled = true;
//	m_gateway.EdgeChannels[6].Id = 7;
//	m_gateway.EdgeChannels[6].Period = 1;
//
//	m_gateway.EdgeChannels[7].Enabled = true;
//	m_gateway.EdgeChannels[7].Id = 8;
//	m_gateway.EdgeChannels[7].Period = 1;

//	m_gateway.EdgeChannels[8].Enabled = true;
//	m_gateway.EdgeChannels[8].Id = 9;
//	m_gateway.EdgeChannels[8].Period = 1;
//
//	m_gateway.EdgeChannels[9].Enabled = true;
//	m_gateway.EdgeChannels[9].Id = 10;
//	m_gateway.EdgeChannels[9].Period = 1;
//
//	m_gateway.EdgeChannels[10].Enabled = true;
//	m_gateway.EdgeChannels[10].Id = 11;
//	m_gateway.EdgeChannels[10].Period = 1;
//
//	m_gateway.EdgeChannels[11].Enabled = true;
//	m_gateway.EdgeChannels[11].Id = 12;
//	m_gateway.EdgeChannels[11].Period = 1;
//
//	m_gateway.EdgeChannels[12].Enabled = true;
//	m_gateway.EdgeChannels[12].Id = 13;
//	m_gateway.EdgeChannels[12].Period = 1;
//
//	m_gateway.EdgeChannels[13].Enabled = true;
//	m_gateway.EdgeChannels[13].Id = 14;
//	m_gateway.EdgeChannels[13].Period = 1;
//
//	m_gateway.EdgeChannels[14].Enabled = true;
//	m_gateway.EdgeChannels[14].Id = 15;
//	m_gateway.EdgeChannels[14].Period = 1;
//
//	m_gateway.EdgeChannels[15].Enabled = true;
//	m_gateway.EdgeChannels[15].Id = 16;
//	m_gateway.EdgeChannels[15].Period = 1;

	m_gateway.MQTTMutex = xSemaphoreCreateMutex();
	m_gateway.LocalStorageMutex = xSemaphoreCreateMutex();

	m_gateway.Status = GW_STARTING;

	m_gateway.Tasks.WifiTask = &WifiTaskDef;
	m_gateway.Tasks.MQTTPublisherTask = &MQTTPublisherTaskDef;
	m_gateway.Tasks.DataSourceTask = &DataSourceTaskDef;
	m_gateway.Tasks.LocalStorageTask = &LocalStorageTaskDef;

	qog_gw_sys_init_watchDog();
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
			sizeof(GatewayCommand));
}

static void gw_init_tasks() {
	gw_init_shared_queues();

	osThreadDef(WifiTask, m_gateway.Tasks.WifiTask->Task, osPriorityAboveNormal,
			1, m_gateway.Tasks.WifiTask->requestedHeap);
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
	__HAL_DBGMCU_FREEZE_IWDG();
	;

	gw_init_gateway();
	gw_init_tasks();
	gw_init_callbacks();
}

//Callbacks
void gwUpdateEdge(EdgeChannel * ch) {
	GatewayCommand cmd = { };

	for (uint8_t idx = 0; idx < MAX_DATA_CHANNELS; idx++) {
		//Compare EdgeChannel
		if (qog_gw_util_isEdgeEqual(&m_gateway.EdgeChannels[idx].EdgeId,
				&ch->EdgeId)) {
			m_gateway.EdgeChannels[idx] = *ch;
			cmd.Command = EDGE_UPDATE;
			cmd.pl = &m_gateway.EdgeChannels[idx];
			break;
		}
	}

	if (cmd.pl != NULL)
		xQueueSend(m_gateway.CommandQueue, &cmd, 0);
}
void gwGetEdgeList() {
	for (uint8_t edx = 0; edx < MAX_DATA_CHANNELS; edx++) {
		if (m_gateway.EdgeChannels[edx].EdgeId.Type != OVS_EdgeType_NULL_EDGE) {
			GatewayCommand cmd = { };
			cmd.Command = EDGE_LIST;
			cmd.pl = &m_gateway.EdgeChannels[edx].EdgeId;
			xQueueSend(m_gateway.CommandQueue, &cmd, 0);
		}
	}
}
void gwAddEdge(Edge* ed) {
	GatewayCommand cmd = { };
	uint8_t idx;
	for (idx = 0; idx < MAX_DATA_CHANNELS; idx++) {
		if (m_gateway.EdgeChannels[idx].EdgeId.Type == OVS_EdgeType_NULL_EDGE) {
			m_gateway.EdgeChannels[idx].EdgeId = *ed;
			break;
		}
	}

	cmd.Command = EDGE_ADD;
	cmd.pl = &m_gateway.EdgeChannels[idx].EdgeId;
	xQueueSend(m_gateway.CommandQueue, &cmd, 0);
}
void gwDropEdge(Edge* ed) {
	GatewayCommand cmd = { };
	cmd.Command = EDGE_DROP;
	cmd.pl = ed;
	xQueueSend(m_gateway.CommandQueue, &cmd, 0);
}

void gwPushNumberData(double val, uint32_t channel, uint32_t timestamp) {
	uint8_t avail;
	xQueueReceive(m_gateway.DataSourceQs.DataAvailableQueue, &avail,
			portMAX_DELAY);

	m_gateway.DataSampleBuffer[avail].channelId = channel;
	m_gateway.DataSampleBuffer[avail].numData.timestamp = timestamp;
	m_gateway.DataSampleBuffer[avail].numData.value = val;

	xQueueSend(m_gateway.DataSourceQs.DataUsedQueue, &avail, portMAX_DELAY);
	/*qog_gw_util_debug_msg("PUSH C:%d V:%2.2f Time:%d",
	 m_gateway.DataSampleBuffer[avail].channelId,
	 m_gateway.DataSampleBuffer[avail].numData.value,
	 m_gateway.DataSampleBuffer[avail].numData.timestamp);*/
}

bool gwPopNumberData(OVS_ChannelNumberData ** out) {
	uint8_t idx = 0;
	if (xQueueReceive(m_gateway.DataSourceQs.DataUsedQueue, &idx,
			100) != errQUEUE_EMPTY) {
		*out = &m_gateway.DataSampleBuffer[idx];
		xQueueSend(m_gateway.DataSourceQs.DataAvailableQueue, &idx, 100);
		return true;
	}
	return false;
}

//RTOS Task
void GruntTaskImpl(void const * argument) {

	TickType_t xLastWakeTime;
	const TickType_t xFrequency = portTICK_PERIOD_MS * OVS_GRUNT_LOOP_MS;

	qog_gw_sys_kick_watchDog();

	uint8_t diagMsgCtr = 0;
	const uint8_t diagMsgDelay = 20;
	xLastWakeTime = xTaskGetTickCount();
	for (;;) {
		vTaskDelayUntil(&xLastWakeTime, xFrequency);
		if (m_gateway.TimeStamp) {
			qog_gw_util_debug_msg("Local Time: %d", m_gateway.TimeStamp++);
		}

		//Update Battery Level
		HAL_ADC_Start(&hadc);
		if (HAL_ADC_PollForConversion(&hadc, 100) == HAL_OK) {
			m_gateway.Diagnostics.BatteryLevel = HAL_ADC_GetValue(&hadc) * 2.0
					* 0.732422;
		} else
			m_gateway.Diagnostics.BatteryLevel = 0;
		HAL_ADC_Stop(&hadc);

		m_gateway.Diagnostics.Uptime += OVS_GRUNT_LOOP_MS / 1000;

		if ((diagMsgCtr++) % diagMsgDelay == 0) {
			GatewayCommand cmd = { GW_STATUS, (void *) &m_gateway.Diagnostics };
			xQueueSend(m_gateway.CommandQueue, &cmd, 0);

			qog_gw_util_debug_msg("Gateway Status");
			qog_gw_util_debug_msg("Battery Level = %d",
					m_gateway.Diagnostics.BatteryLevel);
			qog_gw_util_debug_msg("Uptime = %d", m_gateway.Diagnostics.Uptime);
			qog_gw_util_debug_msg("Message Publish Fail = %d",
					m_gateway.Diagnostics.MsgPublishFails);
		}
		qog_gw_sys_kick_watchDog();
	}
}

//ISR
static TickType_t pb_02_t = 0;
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
	switch (GPIO_Pin) {
	case WIFI_IRQ_N_Pin:
		nm_bsp_isr_cb();
		break;
	case PB_01_Pin:
		qog_gw_util_debug_msg("PB_01 Pressed");
		break;
	case PB_02_Pin: {
		GPIO_PinState st = HAL_GPIO_ReadPin(PB_02_GPIO_Port, PB_02_Pin);
		if (st == GPIO_PIN_SET) {
			qog_gw_util_debug_msg("PB_02 Pressed");
			pb_02_t = xTaskGetTickCount();
		} else {
			TickType_t temp = xTaskGetTickCount() - pb_02_t;
			qog_gw_util_debug_msg("PB_02 Released T=%u", temp);
			if (temp >= 3000 && temp < 10000) {
				m_gateway.EnterWifiConfigMode = true;
			} else if (temp > 10000) {
				qog_gw_pwr_iob_disable();
				qog_gw_pwr_wifi_disable();
				qog_gw_sys_bootloaderJump();
			}
		}

	}
		break;
	default:
		break;
	}
}

//Debug helpers
#if defined(DEBUG)
void vApplicationStackOverflowHook(TaskHandle_t xTask, signed char *pcTaskName) {
	while (1) {
	}
}
#endif
