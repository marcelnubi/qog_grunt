/*
 * DataSourceAPI.h
 *
 *  Created on: Feb 14, 2017
 *      Author: Marcel
 */

#ifndef DATASOURCEAPI_H_
#define DATASOURCEAPI_H_

#include <stdint.h>

qog_DataSource_error DataSourceInit(Gateway *);
qog_DataSource_error DataSourceConfig(uint8_t channelNumber ,uint8_t * configBytes);
qog_DataSource_error DataSourceNumberRead(OVS_EdgeId * edgeId, double *out);

#endif /* DATASOURCEAPI_H_ */
