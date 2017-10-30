/*
 * OVS_Gateway_DataSource.c
 *
 *  Created on: Feb 14, 2017
 *      Author: Marcel
 */

#include "qog_ovs_gateway_internal_types.h"
#include "DataSourceAPI.h"
#include "qog_gateway_power.h"

#include "i2c.h"
#include "adc.h"

static Gateway * m_gateway;
static double currentVal = 0;

void __attribute__((weak)) DataSourceInit() {
	uint32_t asd = 123;
	asd++;
	asd = 12 + asd;

	ADC_ChannelConfTypeDef sConf;

	HAL_ADCEx_Calibration_Start(&hadc);

	hadc.Init.ExternalTrigConv = ADC_SOFTWARE_START;
	if (HAL_ADC_Init(&hadc) != HAL_OK) {
		Error_Handler();
	}

	sConf.Channel = ADC_CHANNEL_VBAT;
	sConf.Rank = ADC_RANK_NONE;
	sConf.SamplingTime = ADC_SAMPLETIME_1CYCLE_5;
	if (HAL_ADC_ConfigChannel(&hadc, &sConf) != HAL_OK) {
		Error_Handler();
	}

	qog_gw_pwr_iob_enable();
}
void __attribute__((weak)) DataSourceConfig(uint8_t channelNumber,
		uint8_t * configBytes) {
}
double __attribute__((weak)) DataSourceNumberRead(uint8_t channelNumber) {
	uint32_t val = 0;
	HAL_ADC_Start(&hadc);
	if (HAL_ADC_PollForConversion(&hadc, 10) == HAL_OK) {
		val = HAL_ADC_GetValue(&hadc);
		HAL_ADC_Stop(&hadc);

#define TEMP110_CAL_ADDR ((uint16_t*) ((uint32_t) 0x1FFFF7C2))
#define TEMP30_CAL_ADDR ((uint16_t*) ((uint32_t) 0x1FFFF7B8))
#define VDD_CALIB ((uint16_t) (330))
#define VDD_APPLI ((uint16_t) (330))

		double temperature; /* will contain the temperature in degrees Celsius */
		temperature = (((double) val * VDD_APPLI / VDD_CALIB)
				- (int32_t) *TEMP30_CAL_ADDR);
		temperature = temperature * (int32_t) (110 - 30);
		temperature = temperature
				/ (int32_t) (*TEMP110_CAL_ADDR - *TEMP30_CAL_ADDR);
		temperature = temperature + 30;
		temperature-=57;
		return temperature;
	}

	uint8_t dasd[2] = { 0, 0 };
	if (HAL_I2C_Mem_Read(&hi2c2, 0x40, 0xE5, 1, dasd, 2, 100) != HAL_OK)
		return m_gateway->TimeStamp;
	else {
		return (uint32_t) *dasd;
	}
}

static void PushNumberData(double val, uint32_t channel, uint32_t timestamp) {
	uint8_t avail;
	xQueueReceive(m_gateway->DataSourceQs.DataAvailableQueue, &avail, 10);

	m_gateway->DataSampleBuffer[avail].channelId = channel;
	m_gateway->DataSampleBuffer[avail].numData.timestamp = timestamp;
	m_gateway->DataSampleBuffer[avail].numData.value = val;

	xQueueSend(m_gateway->DataSourceQs.DataUsedQueue, &avail, 10);
}

struct {
	OVS_Channel * Channels;
	uint32_t NextMeasurement[MAX_DATA_CHANNELS];
} MeasurementSchedule;

static qog_Task DataSourceTaskImpl(Gateway * gwInst);

qog_gateway_task DataSourceTaskDef = { &DataSourceTaskImpl, 128, NULL };

qog_Task DataSourceTaskImpl(Gateway * gwInst) {
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

	for (;;) {
		vTaskDelayUntil(&xLastWakeTime, xFrequency);
		uint32_t thisTime = 0;

		if (m_gateway->TimeStamp) {
			m_gateway->TimeStamp++;
			thisTime = m_gateway->TimeStamp;
		} else
			continue;

		for (uint8_t idx = 0; idx < MAX_DATA_CHANNELS; idx++) {
			if (MeasurementSchedule.Channels[idx].Enabled == true) {
				if (MeasurementSchedule.NextMeasurement[idx] <= thisTime) {
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
