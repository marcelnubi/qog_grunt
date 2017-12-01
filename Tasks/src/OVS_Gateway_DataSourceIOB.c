/*
 * OVS_Gateway_DataSource.c
 *
 *  Created on: Feb 14, 2017
 *      Author: Marcel
 */

#if defined (IOB)

#include "qog_ovs_gateway_internal_types.h"
#include "qog_gateway_data_source_IOB.h"
#include "qog_gateway_power.h"

#include "i2c.h"
#include "adc.h"

#include "math.h"

static Gateway * m_gateway;
static double currentVal = 0;

void __attribute__((weak)) DataSourceInit(Gateway * gw) {
	uint32_t asd = 123;
	asd++;
	asd = 12 + asd;

	m_gateway = gw;

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
	double temperature = 0;

	switch (channelNumber) {
	case 0: {
		uint8_t dasd[2] = { 0, 0 };
		if (HAL_I2C_Master_Receive(&hi2c2, 0x14 << 1, dasd, 2, 100) != HAL_OK) {
			temperature = m_gateway->TimeStamp;
		} else {
			uint16_t temp_code = (dasd[0] << 8) | dasd[1];
			double Vf = temp_code * 0.000045776;
			double Rt = 7150.0 * Vf / (3.0 - Vf);
			double tempDouble = (1.0f / 298.15f)
					+ (1.0f / 3560.0f) * log(Rt / 2000); // 1/T  beta-equation
			tempDouble = 1.0f / tempDouble; // T, Kelvin
			tempDouble = (double) tempDouble - 273.15f; // T , Celsius
			temperature = tempDouble;
		}

	}
		break;
	case 1: {
		uint8_t dasd[2] = { 0, 0 };
		if (HAL_I2C_Mem_Read(&hi2c2, 0x40 << 1, 0xE5, 1, dasd, 2, 100)
				!= HAL_OK)
			temperature = m_gateway->TimeStamp;
		else {
			uint16_t rh_code = (dasd[0] << 8) | dasd[1];
			temperature = (125.0 * rh_code / 65536) - 6.0;
		}
	}
		break;

	case 2: {
		uint8_t dasd[2] = { 0, 0 };
		if (HAL_I2C_Mem_Read(&hi2c2, 0x40 << 1, 0xE3, 1, dasd, 2, 100)
				!= HAL_OK)
			temperature = m_gateway->TimeStamp;
		else {
			uint16_t temp_code = (dasd[0] << 8) | dasd[1];
			temperature = (175.72 * temp_code / 65536) - 46.85;
		}
	}
		break;

	default:
		break;
	}

	return temperature;
}

struct {
	OVS_Channel * Channels;
	uint32_t NextMeasurement[MAX_DATA_CHANNELS];
} MeasurementSchedule;

static qog_Task DataSourceTaskImpl(Gateway * gwInst);

qog_gateway_task DataSourceTaskDef = { &DataSourceTaskImpl,
DATASOURCER_TASK_HEAP, NULL };

qog_Task DataSourceTaskImpl(Gateway * gwInst) {
	m_gateway = gwInst;
	TickType_t xLastWakeTime;
	const TickType_t xFrequency = portTICK_PERIOD_MS * 1000; //1000ms wait

	// Initialise the xLastWakeTime variable with the current time.
	DataSourceInit(m_gateway);

	//Initialise Channels and Measurement
	MeasurementSchedule.Channels = gwInst->EdgeChannels;

	xLastWakeTime = xTaskGetTickCount();

	for (;;) {
		vTaskDelayUntil(&xLastWakeTime, xFrequency);
		uint32_t thisTime = 0;

		if (m_gateway->TimeStamp && m_gateway->StopAll == false) {
			//	m_gateway->TimeStamp++;
			thisTime = m_gateway->TimeStamp;
		} else
			continue;

		for (uint8_t idx = 0; idx < MAX_DATA_CHANNELS; idx++) {
			if (MeasurementSchedule.Channels[idx].Enabled == true) {
				if (MeasurementSchedule.NextMeasurement[idx] <= thisTime) {
					currentVal = DataSourceNumberRead(idx);
					m_gateway->CB.gwPushNumberData(currentVal,
							MeasurementSchedule.Channels[idx].Id, thisTime);
					MeasurementSchedule.NextMeasurement[idx] = thisTime
							+ MeasurementSchedule.Channels[idx].Period;
				}
			}
			HAL_Delay(10);
		}
	}
	return 0;
}

#endif
