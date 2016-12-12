/*
 * OVS_Gateway_Wifi_Task.c
 *
 *  Created on: Dec 2, 2016
 *      Author: Marcel
 */

#if defined(GW_WIFI_TASK_ATWINC_1500)

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
	SocketClosed = 0, SocketConnected, SocketWaiting, SocketError
} SocketStatus;

static struct
{
	SocketStatus status;
	SOCKET number;
} Sockets[MAX_OPEN_SOCKETS];

uint32_t resolvedHostIp = 0x0;

Gateway * m_gatewayInst = 0;

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
	switch (u8MsgType)
	{
	case M2M_WIFI_RESP_CON_STATE_CHANGED:
	{
		tstrM2mWifiStateChanged *pstrWifiState =
				(tstrM2mWifiStateChanged *) pvMsg;
		if (pstrWifiState->u8CurrState == M2M_WIFI_CONNECTED)
		{
//			printf("wifi_cb: M2M_WIFI_RESP_CON_STATE_CHANGED: CONNECTED\r\n");
			m2m_wifi_request_dhcp_client();
		}
		else if (pstrWifiState->u8CurrState == M2M_WIFI_DISCONNECTED)
		{
//			printf("wifi_cb: M2M_WIFI_RESP_CON_STATE_CHANGED: DISCONNECTED\r\n");
//			gbConnectedWifi = false;
//			gbHostIpByName = false;
			m2m_wifi_connect((char *) m_gatewayInst->WLANConnection.WLAN_SSID,
					sizeof(m_gatewayInst->WLANConnection.WLAN_SSID),
					m_gatewayInst->WLANConnection.WLAN_AUTH,
					(char *) m_gatewayInst->WLANConnection.WLAN_PSK,
					M2M_WIFI_CH_ALL);
		}

		break;
	}

	case M2M_WIFI_REQ_DHCP_CONF:
	{
		//	gbConnectedWifi = true;
		m_gatewayInst->Status = GW_WLAN_CONNECTED;
		/* Obtain the IP Address by network name */
		gethostbyname((uint8_t *) m_gatewayInst->BrokerParams.HostName);
		break;
	}

	default:
	{
		break;
	}
	}
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
	resolvedHostIp = hostIp;
	m_gatewayInst->Status = GW_BROKER_DNS_RESOLVED;
}

qog_Task WifiTaskImpl(Gateway * gwInst)
{
	tstrWifiInitParam param;
	int8_t ret;
	Sockets[0].number = socket(AF_INET, SOCK_STREAM, 0);
	Sockets[0].status = SocketClosed;
	memset((uint8_t *) &param, 0, sizeof(tstrWifiInitParam));

	nm_bsp_init();
	/* USER CODE BEGIN 2 */
	/* Initialize Wi-Fi driver with data and status callbacks. */
	param.pfAppWifiCb = wifi_cb;
	ret = m2m_wifi_init(&param);
	if (M2M_SUCCESS != ret)
	{
		//TODO Debug port out
		if (m_gatewayInst->Status == GW_BROKER_DNS_RESOLVED)
			while (1)
			{
				switch (Sockets[0].status)
				{
				case SocketClosed:
				{
					struct sockaddr_in addr_in;
					addr_in.sin_family = AF_INET;
					addr_in.sin_port = gwInst->BrokerParams.HostPort;
					addr_in.sin_addr.s_addr = resolvedHostIp;
//					if (connect(Sockets[0].number, (struct sockaddr *) &addr_in,
//							sizeof(struct sockaddr_in)) != SOCK_ERR_NO_ERROR)
//					{
//						Sockets[0].status  =
//					}
				}

				}
				//send tx queue byte
			}
	}
//
//	socketInit();
//	registerSocketCallback(socket_cb, resolve_cb);
//	/* Connect to router. */
//	m2m_wifi_connect((char *) QogniBrokerParams.WLAN_SSID, strlen((char*)QogniBrokerParams.WLAN_SSID),
//			(tenuM2mSecType)QogniBrokerParams.WLAN_AUTH, (char *) QogniBrokerParams.WLAN_PSK, M2M_WIFI_CH_ALL);
//
//	m2m_wifi_handle_events(NULL);
}

#endif
