/*
 * qog_gateway_system.c
 *
 *  Created on: Oct 31, 2017
 *      Author: Marcel
 */

#include "qog_gateway_system.h"
#include "cmsis_os.h"

#if defined (STM32F091xC)
#include "stm32f0xx.h"
#endif

qog_gw_error_t qog_gw_sys_getUri(GatewayId * id) {
	uint8_t bf[12];

	memcpy(bf, (uint8_t *) UID_BASE, 12);
	snprintf((char*) id->x,sizeof(id->x), "%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X",
			bf[0], bf[1], bf[2], bf[3], bf[4], bf[5], bf[6], bf[7], bf[8],
			bf[9], bf[10], bf[11]);

	return GW_e_OK;
}

//Replacing Sync delay with osDelay
void HAL_Delay(uint32_t ms) {
	osDelay(ms);
}

//Update RTC with input time
qog_gw_error_t qog_gw_sys_setTime(qog_DateTime * dt) {
	if (HAL_RTC_SetTime(&hrtc, &dt->Time, RTC_FORMAT_BIN) != HAL_OK) {
		_Error_Handler(__FILE__, __LINE__);
	}

	if (HAL_RTC_SetDate(&hrtc, &dt->Date, RTC_FORMAT_BIN) != HAL_OK) {
		_Error_Handler(__FILE__, __LINE__);
	}
	return GW_e_OK;
}

uint32_t qog_gw_sys_getTimestamp() {
	const uint32_t JULIAN_DATE_BASE = 2440588;
	volatile uint8_t a;
	volatile uint16_t y;
	volatile uint8_t m;
	volatile uint32_t JDN;

	RTC_TimeTypeDef time;
	RTC_DateTypeDef date;

	HAL_RTC_GetDate(&hrtc, &date, RTC_FORMAT_BIN);
	HAL_RTC_GetTime(&hrtc, &time, RTC_FORMAT_BIN);
	// These hardcore math's are taken from http://en.wikipedia.org/wiki/Julian_day

	// Calculate some coefficients
	a = (14 - date.Month) / 12;
	y = (date.Year + 2000) + 4800 - a; // years since 1 March, 4801 BC
	m = date.Month + (12 * a) - 3; // since 1 March, 4801 BC

	// Gregorian calendar date compute
	JDN = date.Date;
	JDN += (153 * m + 2) / 5;
	JDN += 365 * y;
	JDN += y / 4;
	JDN += -y / 100;
	JDN += y / 400;
	JDN = JDN - 32045;
	JDN = JDN - JULIAN_DATE_BASE;    // Calculate from base date
	JDN *= 86400;                     // Days to seconds
	JDN += time.Hours * 3600;    // ... and today seconds
	JDN += time.Minutes * 60;
	JDN += time.Seconds;

	return JDN;
}
