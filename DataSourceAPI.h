/*
 * DataSourceAPI.h
 *
 *  Created on: Feb 14, 2017
 *      Author: Marcel
 */

#ifndef DATASOURCEAPI_H_
#define DATASOURCEAPI_H_

#include <stdint.h>

void DataSourceInit();
void DataSourceConfig(uint8_t channelNumber ,uint8_t * configBytes);
double DataSourceNumberRead(uint8_t channelNumber);

#endif /* DATASOURCEAPI_H_ */
