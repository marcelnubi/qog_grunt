/*
 * OVS_Gateway_Wifi_Task.c
 *
 *  Created on: Dec 2, 2016
 *      Author: Marcel
 */

#include "../inc/OVS_Gateway_Task.h"
#include "driver/include/m2m_wifi.h"
#include "socket/include/socket.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"

//-------- Gateway Interface - START
#define WIFI_TASK_HEAP 0x200

static qog_Task WifiTaskImpl(Gateway * gwInst);

qog_gateway_task WifiTaskDef =
{ &WifiTaskImpl, WIFI_TASK_HEAP, NULL };
//-------- Gateway Interface - END

#define MAX_OPEN_SOCKETS 8
typedef enum
{
	SocketClosed = 0, SocketConnected, SocketError
} SocketStatus;

static struct
{
	SocketStatus status;
	int8_t number;
} Sockets[MAX_OPEN_SOCKETS];

enum
{
	WIFI_S_AP_CONFIG, WIFI_S_CONNECTED, WIFI_S_ERROR
} WifiStatus;

xQueueHandle *m_sockRxQueue, *m_sockTxQueue;

/**
 * \brief Callback function of TCP client socket.
 *
 * \param[in] sock socket handler.
 * \param[in] u8Msg Type of Socket notification
 * \param[in] pvMsg A structure contains notification informations.
 *
 * \return None.
 */
static void socket_cb(SOCKET sock, uint8_t u8Msg, void *pvMsg)
{
}

static void wifi_cb(uint8_t u8MsgType, void *pvMsg)
{
}

/**
 * \brief Callback function of IP address.
 *
 * \param[in] hostName Domain name.
 * \param[in] hostIp Server IP.
 *
 * \return None.
 */
static void resolve_cb(uint8_t *hostName, uint32_t hostIp)
{
}

qog_Task WifiTaskImpl(Gateway * gwInst)
{
	tstrWifiInitParam param;
	int8_t ret;

	memset((uint8_t *) &param, 0, sizeof(tstrWifiInitParam));

	m_sockRxQueue = (xQueueHandle *) gwInst->SocketRxQueue;
	m_sockTxQueue = (xQueueHandle *) gwInst->SocketTxQueue;

	nm_bsp_init();
	/* USER CODE BEGIN 2 */
	/* Initialize Wi-Fi driver with data and status callbacks. */
	param.pfAppWifiCb = wifi_cb;
	ret = m2m_wifi_init(&param);
	if (M2M_SUCCESS != ret)
	{
		//TODO Debug port out
		while (1)
		{
		}
	}

//	socketInit();
//	registerSocketCallback(socket_cb, resolve_cb);
//
//	sprintf((char*) QogniBrokerParams.HostName, "qogni.com");
//	QogniBrokerParams.HostPort = 1883;
//	sprintf((char*) QogniBrokerParams.WLAN_SSID, "NuggetL");
//	sprintf((char*) QogniBrokerParams.WLAN_PSK, "Furmiga1L");
//	QogniBrokerParams.WLAN_AUTH = (uint8_t) M2M_WIFI_SEC_WPA_PSK;
//
//	/* Connect to router. */
//	m2m_wifi_connect((char *) QogniBrokerParams.WLAN_SSID, strlen((char*)QogniBrokerParams.WLAN_SSID),
//			(tenuM2mSecType)QogniBrokerParams.WLAN_AUTH, (char *) QogniBrokerParams.WLAN_PSK, M2M_WIFI_CH_ALL);
//
//	m2m_wifi_handle_events(NULL);
}
