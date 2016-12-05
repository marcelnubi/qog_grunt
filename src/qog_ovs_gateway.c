/*
 * qog_ovs_gateway.c
 *
 *  Created on: Dec 1, 2016
 *      Author: Marcel
 */

#include "cmsis_os.h"

#include "stdio.h"
#include "string.h"

#include "../qog_ovs_gateway.h"
#include "../qog_gateway_config.h"
#include "../qog_ovs_gateway_types.h"

#include "Overseer_Connection.h"
#include "OVS_ChannelNumberData.pb.h"
#include "OVS_Channel.pb.h"

#include "Tasks/inc/OVS_Gateway_Task.h"

static Gateway m_gateway;

static void gw_init_gateway()
{
	//TODO Retrive WLAN config

	//TODO Retrieve MQTT Broker config
	sprintf((char*) m_gateway.BrokerParams.HostName, OVS_BROKER_HOST);
	m_gateway.BrokerParams.HostPort = OVS_BROKER_PORT;
	sprintf((char*) m_gateway.BrokerParams.Username, "admin");
	sprintf((char*) m_gateway.BrokerParams.Password, "qognata");

	m_gateway.MQTTMutex = xSemaphoreCreateMutex();

	m_gateway.Status = GW_STARTING;

	m_gateway.Tasks.WifiTask = &WifiTaskDef;
	//MQTT Client
	//MQTT Publisher
	//Requested heap vem da onde?
}

static void gw_init_shared_queues()
{
	m_gateway.DataAvailableQueue = xQueueCreate(OVS_NUMBER_DATA_BUFFER_SIZE,
			sizeof(uint8_t));
	m_gateway.DataUsedQueue = xQueueCreate(OVS_NUMBER_DATA_BUFFER_SIZE,
			sizeof(uint8_t));
	uint8_t initq = 0;
	for (initq = 0; initq < MAX_SAMPLE_BUFFER_SIZE; initq++)
	{
		xQueueSend(m_gateway.DataAvailableQueue, initq, 100);
	}

	m_gateway.SocketRxQueue = xQueueCreate(OVS_RX_SOCKET_BUFFER_SIZE,
			sizeof(uint8_t));
	m_gateway.SocketTxQueue = xQueueCreate(OVS_TX_SOCKET_BUFFER_SIZE,
			sizeof(uint8_t));

}

static void gw_init_tasks()
{
	uint8_t tsk = 0;
	gw_init_shared_queues();

	xTaskCreate(&m_gateway.Tasks.WifiTask, "GW: Wifi Task",
			m_gateway.Tasks.WifiTask->requestedHeap, &m_gateway,
			tskIDLE_PRIORITY + 3, &m_gateway.Tasks.WifiTask->Handle);

	xTaskCreate(&m_gateway.Tasks.MQTTClientTask, "GW: MQTT Client",
			m_gateway.Tasks.MQTTClientTask->requestedHeap, &m_gateway,
			tskIDLE_PRIORITY, &m_gateway.Tasks.MQTTClientTask->Handle);

	xTaskCreate(&m_gateway.Tasks.MQTTPublisherTask, "GW: MQTT Publisher",
			m_gateway.Tasks.MQTTPublisherTask->requestedHeap, &m_gateway,
			tskIDLE_PRIORITY + 1, &m_gateway.Tasks.MQTTPublisherTask->Handle);

	for (tsk = 0; tsk < MAX_DATA_SOURCE_TASKS; tsk++)
	{
		if (m_gateway.Tasks.DataSourceTaskGroup[tsk]->Task != NULL)
		{
			xTaskCreate(&m_gateway.Tasks.DataSourceTaskGroup[tsk]->Task, NULL,
					m_gateway.Tasks.DataSourceTaskGroup[tsk]->requestedHeap,
					&m_gateway, tskIDLE_PRIORITY + 2,
					&m_gateway.Tasks.DataSourceTaskGroup[tsk]->Handle);
		}
	}
}

void qog_ovs_gw_register_data_source(qog_Task (*dataSourceFn)(Gateway * gwInst),
		uint32_t heapSize)
{
	uint8_t tsk = 0;
	for (tsk = 0; tsk < MAX_DATA_SOURCE_TASKS; tsk++)
	{
		if (m_gateway.Tasks.DataSourceTaskGroup[tsk]->Task == NULL)
		{
			m_gateway.Tasks.DataSourceTaskGroup[tsk]->Task = dataSourceFn;
			m_gateway.Tasks.DataSourceTaskGroup[tsk]->requestedHeap = heapSize;
			break;
		}
	}
}

void qog_ovs_run()
{
	gw_init_gateway();
	gw_init_tasks();
	osKernelStart();
}
