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
#include "qog_ovs_gateway_internal_types.h"

#include "winc_1500_lib/driver/include/m2m_wifi.h"
#include "winc_1500_lib/bsp/include/nm_bsp.h"

#include "Overseer_Connection.h"
#include "OVS_ChannelNumberData.pb.h"
#include "OVS_Channel.pb.h"

#include "OVS_Gateway_Task.h"

#include "gpio.h"

//Replacing Sync delay with osDelay
void HAL_Delay(uint32_t ms) {
	osDelay(ms);
}


static Gateway m_gateway;

static void gw_init_gateway() {
	m_gateway.Status = GW_STARTING;
	//TODO Retrieve NV Memory config
	//TODO Ler Gateway Id URI
	//TODO Retrive WLAN config
	sprintf((char *) m_gateway.WLANConnection.WLAN_SSID, "Qogni_2.4G");
	sprintf((char *) m_gateway.WLANConnection.WLAN_PSK, "qognata33");
	m_gateway.WLANConnection.WLAN_AUTH = M2M_WIFI_SEC_WPA_PSK;

	//TODO Retrieve MQTT Broker config
	sprintf((char*) m_gateway.BrokerParams.HostName, OVS_BROKER_HOST);
	m_gateway.BrokerParams.HostPort = OVS_BROKER_PORT;
	sprintf((char*) m_gateway.BrokerParams.Username, "admin");
	sprintf((char*) m_gateway.BrokerParams.Password, "qognata");

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
			OVS_NUMBER_DATA_BUFFER_SIZE, sizeof(uint8_t));
	m_gateway.DataSourceQs.DataUsedQueue = xQueueCreate(
			OVS_NUMBER_DATA_BUFFER_SIZE, sizeof(uint8_t));

	uint8_t initq = 0;
	for (initq = 0; initq < MAX_SAMPLE_BUFFER_SIZE; initq++) {
		xQueueSend(m_gateway.DataSourceQs.DataAvailableQueue, (void * )&initq,
				100);
	}

	m_gateway.SocketRxQueue = xQueueCreate(OVS_RX_SOCKET_BUFFER_SIZE,
			sizeof(uint8_t));
	m_gateway.SocketTxQueue = xQueueCreate(OVS_TX_SOCKET_BUFFER_SIZE,
			sizeof(uint8_t));

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

void qog_ovs_run() {
	gw_init_gateway();
	gw_init_tasks();
}

//ISR
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
	if (GPIO_Pin == WIFI_IRQ_N_Pin) {
		nm_bsp_isr_cb();
	}
}

#if defined(DEBUG)
void vApplicationStackOverflowHook(TaskHandle_t xTask, signed char *pcTaskName) {
	while (1) {
	}
}
#endif
