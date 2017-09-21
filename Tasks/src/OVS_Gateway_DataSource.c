/*
 * OVS_Gateway_DataSource.c
 *
 *  Created on: Feb 14, 2017
 *      Author: Marcel
 */

#include "qog_ovs_gateway_internal_types.h"
#include "DataSourceAPI.h"

void __attribute__((weak)) DataSourceInit()
{
	uint32_t asd = 123;
	asd++;
	asd = 12 + asd;
}
void __attribute__((weak)) DataSourceConfig(uint8_t channelNumber,
		uint8_t * configBytes)
{
}
double __attribute__((weak)) DataSourceNumberRead(uint8_t channelNumber)
{
	return 42;
}

static Gateway * m_gateway;

static double currentVal = 0;

static void PushNumberData(double val, uint32_t channel, uint32_t timestamp)
{
	uint8_t avail;
	xQueueReceive(m_gateway->DataSourceQs.DataAvailableQueue, &avail, 10);

	m_gateway->DataSampleBuffer[avail].channelId = channel;
	m_gateway->DataSampleBuffer[avail].numData.timestamp = timestamp;
	m_gateway->DataSampleBuffer[avail].numData.value = val;

	xQueueSend(m_gateway->DataSourceQs.DataUsedQueue, &avail, 10);
}

struct
{
	Channel * Channels;
	uint32_t NextMeasurement[MAX_DATA_CHANNELS];
} MeasurementSchedule;

static qog_Task DataSourceTaskImpl(Gateway * gwInst);

qog_gateway_task DataSourceTaskDef =
{ &DataSourceTaskImpl, 0x200, NULL };

qog_Task DataSourceTaskImpl(Gateway * gwInst)
{
	m_gateway = gwInst;
	TickType_t xLastWakeTime;
	const TickType_t xFrequency = portTICK_PERIOD_MS * 1000; //1000ms wait

	// Initialise the xLastWakeTime variable with the current time.
	DataSourceInit();

	//Initialise Channels and Measurement
	MeasurementSchedule.Channels = gwInst->DataChannels;

	//TODO init in DataSource Impl
	MeasurementSchedule.Channels[0].Enabled = true;
	MeasurementSchedule.Channels[0].Id = 24;

	xLastWakeTime = xTaskGetTickCount();

	for (;;)
	{
		vTaskDelayUntil(&xLastWakeTime, xFrequency);
		uint32_t thisTime = 0;

		if (m_gateway->TimeStamp)
		{
			m_gateway->TimeStamp++;
			thisTime = m_gateway->TimeStamp;
		}
		else
			continue;

		for (uint8_t idx = 0; idx < MAX_DATA_CHANNELS; idx++)
		{
			if (MeasurementSchedule.Channels[idx].Enabled == true)
			{
				if (MeasurementSchedule.NextMeasurement[idx] <= thisTime)
				{
					currentVal = DataSourceNumberRead(idx + 1);
					PushNumberData(currentVal,
							MeasurementSchedule.Channels[idx].Id, thisTime);
					MeasurementSchedule.NextMeasurement[idx] = thisTime
							+ MeasurementSchedule.Channels[idx].Period;
				}
			}
		}
	}
	return 0;
}
