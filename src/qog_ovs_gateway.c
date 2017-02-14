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

#include "qog_ovs_gateway.h"
#include "qog_gateway_config.h"
#include "qog_ovs_gateway_internal_types.h"

#include "Overseer_Connection.h"
#include "OVS_ChannelNumberData.pb.h"
#include "OVS_Channel.pb.h"

#include "OVS_Gateway_Task.h"

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
	m_gateway.LocalStorageMutex = xSemaphoreCreateMutex();

	m_gateway.Status = GW_STARTING;

	m_gateway.Tasks.WifiTask = &WifiTaskDef;
	m_gateway.Tasks.MQTTClientTask = &MQTTClientTaskDef;
	m_gateway.Tasks.MQTTPublisherTask = &MQTTPublisherTaskDef;
	m_gateway.Tasks.DataSourceTask = &DataSourceTaskDef;
	m_gateway.Tasks.LocalStorageTask = &LocalStorageTaskDef;
}

static void gw_init_shared_queues()
{
	m_gateway.DataSourceQs.DataAvailableQueue = xQueueCreate(
			OVS_NUMBER_DATA_BUFFER_SIZE, sizeof(uint8_t));
	m_gateway.DataSourceQs.DataUsedQueue = xQueueCreate(
			OVS_NUMBER_DATA_BUFFER_SIZE, sizeof(uint8_t));

	uint8_t initq = 0;
	for (initq = 0; initq < MAX_SAMPLE_BUFFER_SIZE; initq++)
	{
		xQueueSend(m_gateway.DataSourceQs.DataAvailableQueue, (void * )&initq,
				100);
	}

	m_gateway.SocketRxQueue = xQueueCreate(OVS_RX_SOCKET_BUFFER_SIZE,
			sizeof(uint8_t));
	m_gateway.SocketTxQueue = xQueueCreate(OVS_TX_SOCKET_BUFFER_SIZE,
			sizeof(uint8_t));

}

static void gw_init_tasks()
{
//	uint8_t tsk = 0;
	gw_init_shared_queues();

	osThreadDef(WifiTask, m_gateway.Tasks.WifiTask->Task, osPriorityNormal, 0,
			128);
	m_gateway.Tasks.WifiTask->Handle = osThreadCreate(osThread(WifiTask),
			&m_gateway);

	osThreadDef(MQTTClient, m_gateway.Tasks.MQTTClientTask->Task,
			osPriorityNormal, 0, 128);
	m_gateway.Tasks.MQTTClientTask->Handle = osThreadCreate(
			osThread(MQTTClient), &m_gateway);

	osThreadDef(MQTTPublisherTask, m_gateway.Tasks.MQTTPublisherTask->Task,
			osPriorityNormal, 0, 128);
	m_gateway.Tasks.MQTTPublisherTask->Handle = osThreadCreate(
			osThread(MQTTPublisherTask), &m_gateway);

	osThreadDef(DataSourceTask, m_gateway.Tasks.DataSourceTask->Task,
			osPriorityNormal, 0, 128);
	m_gateway.Tasks.DataSourceTask->Handle = osThreadCreate(
			osThread(MQTTPublisherTask), &m_gateway);


//	qog_ovs_gw_init();
//	for (tsk = 0; tsk < MAX_DATA_SOURCE_TASKS; tsk++)
//	{
//		if (m_gateway.Tasks.DataSourceTaskGroup[tsk] != NULL)
//		{
//			osThreadDef(DataSource,
//					m_gateway.Tasks.DataSourceTaskGroup[tsk]->Task,
//					osPriorityNormal, 0, 128);
//			m_gateway.Tasks.DataSourceTaskGroup[tsk]->Handle = osThreadCreate(
//					osThread(DataSource), &m_gateway.DataSourceQs);
//		}
//	}
}

//void qog_ovs_gw_register_data_source(qog_gateway_task * task)
//{
//	uint8_t tsk = 0;
//	for (tsk = 0; tsk < MAX_DATA_SOURCE_TASKS; tsk++)
//	{
//		if (m_gateway.Tasks.DataSourceTaskGroup[tsk] == NULL)
//		{
//			m_gateway.Tasks.DataSourceTaskGroup[tsk] = task;
//			break;
//		}
//	}
//}

void qog_ovs_run()
{
	gw_init_gateway();
	gw_init_tasks();
	osKernelStart();
}
