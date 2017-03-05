/*
 * DataSourceAPI.h
 *
 *  Created on: Feb 14, 2017
 *      Author: Marcel
 */

#ifndef DATASOURCEAPI_H_
#define DATASOURCEAPI_H_

#include <stdint.h>

extern void DataSourceInit();
extern void DataSourceConfig(uint8_t channelNumber ,uint8_t * configBytes);
extern double DataSourceNumberRead(uint8_t channelNumber);

#endif /* DATASOURCEAPI_H_ */
