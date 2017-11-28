/*
 * OVS_Gateway_DataSource_0005.c
 *
 *  Created on: 10 Nov 2017
 *      Author: marcel
 */

#if IOB == 0005

#include "qog_ovs_gateway_internal_types.h"
#include "qog_gateway_power.h"
#include "qog_gateway_system.h"

#include "adc.h"
#include "i2c.h"

#include "math.h"

static Gateway * m_gw = NULL;

void DataSourceInit(Gateway *gw) {
	GatewayId gid;
	qog_gw_sys_getUri(&gid);

	for (uint8_t id = 0; id < 4; id++) {
		OVS_Edge_IOB_id edIob = { };
		Edge ed = { };
		snprintf((char*) &edIob.URI.bytes, sizeof(edIob.URI.bytes), "%s",
				gid.x);
		edIob.URI.size = sizeof(edIob.URI.bytes);
		edIob.Iob = OVS_Edge_IOB_id_type_iob_0005;
		edIob.IobEdge = id;

		pb_ostream_t ostream = pb_ostream_from_buffer(ed.Id.bytes,
				sizeof(ed.Id.bytes));
		pb_encode(&ostream, OVS_Edge_IOB_id_fields, &edIob);
		ed.Id.size = ostream.bytes_written;
		ed.Type = OVS_EdgeType_QOGNI_IOB_EDGE;

		gw->CB.gwAddEdge(&ed);
	}

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
void DataSourceConfig(uint8_t channelNumber, uint8_t * configBytes) {
}
double DataSourceNumberRead(uint8_t channelNumber) {
	double temperature = 0;

	switch (channelNumber % 4) {
	case 0: {
		HAL_ADC_Start(&hadc);
		if (HAL_ADC_PollForConversion(&hadc, 50) == HAL_OK) {
			uint32_t dasd = HAL_ADC_GetValue(&hadc);
			temperature = dasd * 3.3 * 2 / 4096.0;
		} else {
			temperature = m_gw->TimeStamp;
		}
		HAL_ADC_Stop(&hadc);
	}
		break;
	case 1: {
		uint8_t dasd[2] = { 0, 0 };
		if (HAL_I2C_Master_Receive(&hi2c2, 0x14 << 1, dasd, 2, 100) != HAL_OK) {
			temperature = m_gw->TimeStamp;
			HAL_I2C_DeInit(&hi2c2);
			MX_I2C2_Init();
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

	case 2: {
		uint8_t dasd[2] = { 0, 0 };
		if (HAL_I2C_Mem_Read(&hi2c2, 0x40 << 1, 0xE5, 1, dasd, 2, 100)
				!= HAL_OK) {
			temperature = m_gw->TimeStamp;
			HAL_I2C_DeInit(&hi2c2);
			MX_I2C2_Init();
		} else {
			uint16_t rh_code = (dasd[0] << 8) | dasd[1];
			temperature = (125.0 * rh_code / 65536) - 6.0;
		}
	}
		break;
	case 3: {
		uint8_t dasd[2] = { 0, 0 };
		if (HAL_I2C_Mem_Read(&hi2c2, 0x40 << 1, 0xE3, 1, dasd, 2, 100)
				!= HAL_OK) {
			temperature = m_gw->TimeStamp;
			HAL_I2C_DeInit(&hi2c2);
			MX_I2C2_Init();
		} else {
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

#endif
