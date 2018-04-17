/*
 * qog_gateway_util.c
 *
 *  Created on: 16 Nov 2017
 *      Author: marcel
 */

#include "qog_gateway_util.h"
#include "stdarg.h"
#include "usart.h"

bool qog_gw_util_isEdgeEqual(Edge * ec1, Edge *ec2) {
	bool ret = false;

	if (ec1->Type != ec2->Type)
		return ret;

	pb_istream_t istream1 = pb_istream_from_buffer(ec1->Id.bytes,
			sizeof(ec1->Id.bytes));
	pb_istream_t istream2 = pb_istream_from_buffer(ec2->Id.bytes,
			sizeof(ec2->Id.bytes));

	switch (ec1->Type) {
	case OVS_EdgeType_NULL_EDGE: {
		ret = true;
	}
		break;
	case OVS_EdgeType_QOGNI_IOB_EDGE: {
		OVS_Edge_IOB_id e1, e2;
		pb_decode(&istream1, OVS_Edge_IOB_id_fields, &e1);
		pb_decode(&istream2, OVS_Edge_IOB_id_fields, &e2);
		ret = true;

		if (e1.Iob != e2.Iob) {
			ret = false;
			break;
		}
		if (e1.IobEdge != e2.IobEdge) {
			ret = false;
			break;
		}
		if (e1.URI.size != e2.URI.size) {
			ret = false;
			break;
		}
		if (strcmp(e1.URI.bytes, e2.URI.bytes)) {
			ret = false;
			break;
		}
	}
		break;
	default:
		break;
	}

	return ret;
}

void qog_gw_util_debug_msg(char * format, ...) {
	{
#ifndef RELEASE
		va_list args;
		uint8_t msg[128];
		sprintf(msg, "DEBUG: ");
		va_start(args, format);
		vsprintf(msg, format, args);
		va_end(args);
		sprintf(msg + strlen(msg), "\n");
		HAL_UART_Transmit(&huart1, msg, strlen(msg), 100);
#endif
	}
}
