/*
 * OVS_Gateway_Wifi_Task.c
 *
 *  Created on: Dec 2, 2016
 *      Author: Marcel
 */

#if defined(GW_WIFI_TASK_ATWINC_1500)

#include "qog_gateway_error_types.h"
#include "../inc/OVS_Gateway_Task.h"
#include "driver/include/m2m_wifi.h"
#include "socket/include/socket.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"

//-------- Gateway Interface - START
#define WIFI_TASK_HEAP 0x200
static const int TIMEOUT_SOCKET_OPEN = 4000;
static const int TIMEOUT_WIFI_WLAN_CONNECT = 5000;
static const int TIMEOUT_WIFI_RESOLVE_HOST = 5000;
static const uint16_t TASK_PERIOD_MS_WIFI = 100;

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
	switch (u8Msg)
	{
	/* Socket connected */
	case SOCKET_MSG_CONNECT:
	{
//		tstrSocketConnectMsg *pstrConnect = (tstrSocketConnectMsg *) pvMsg;
		Sockets[0].status = SocketConnected;
		m_gatewayInst->Status = GW_BROKER_SOCKET_OPEN;
		break;
	}
		/* Message send */
	case SOCKET_MSG_SEND:
	{
	}
		break;
		/* Message receive */
	case SOCKET_MSG_RECV:
	{
		tstrSocketRecvMsg *pstrRecv = (tstrSocketRecvMsg *) pvMsg;
		if (pstrRecv && pstrRecv->s16BufferSize > 0)
		{
			uint16_t idx = 0;
			while (idx++ < pstrRecv->s16BufferSize)
			{
				xQueueSend(m_gatewayInst->SocketRxQueue, pstrRecv->pu8Buffer++,
						10);
			}
		}
	}
		break;

	default:
		break;
	}
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
			m2m_wifi_request_dhcp_client();
		}
		else if (pstrWifiState->u8CurrState == M2M_WIFI_DISCONNECTED)
		{
			m_gatewayInst->Status = GW_WLAN_DISCONNECTED;
		}

	}
		break;
	case M2M_WIFI_REQ_DHCP_CONF:
	{
		m_gatewayInst->Status = GW_WLAN_CONNECTED;
		/* Obtain the IP Address by network name */

	}
		break;
	case M2M_WIFI_RESP_IP_CONFLICT:
	{
		m2m_wifi_request_dhcp_client();
	}
		break;
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
static void dns_resolve_cb(uint8_t *hostName, uint32_t hostIp)
{
	m_gatewayInst->BrokerParams.HostIp = hostIp;
	m_gatewayInst->Status = GW_BROKER_DNS_RESOLVED;
}

static qog_gw_error_t GatewaySocketOpen(uint32_t timeout)
{
	close(Sockets[0].number);
	socketDeinit();
	socketInit();
	Sockets[0].status = SocketClosed;
	m_gatewayInst->Status = GW_BROKER_SOCKET_CLOSED;

	while (Sockets[0].status != SocketConnected)
	{
		switch (Sockets[0].status)
		{
		case SocketClosed:
		{
			struct sockaddr_in addr_in;
			addr_in.sin_family = AF_INET;
			addr_in.sin_port = m_gatewayInst->BrokerParams.HostPort;
			addr_in.sin_addr.s_addr = m_gatewayInst->BrokerParams.HostIp;
			if (connect(Sockets[0].number, (struct sockaddr *) &addr_in,
					sizeof(struct sockaddr_in)) != SOCK_ERR_NO_ERROR)
				Sockets[0].status = SocketWaiting;
		}
			break;
		case SocketWaiting:
		{
			vTaskDelay(100);
			m2m_wifi_handle_events(NULL);
		}
			break;
		case SocketError:
		{
			//TODO Tratar erro de socket, decidir se continua ou se para
			Sockets[0].status = SocketClosed;
		}
		}
		//send tx queue byte
		//TODO timeout
	}
	return GW_e_OK;
}

static void GatewayWaitForStatusChange(Gateway* m_gatewayInst,
		GatewayStatus status, uint32_t timeout)
{
	while (m_gatewayInst->Status != status)
	{
		vTaskDelay(100);
		m2m_wifi_handle_events(NULL);

		if (m_gatewayInst->Status == GW_ERROR)
			break;
	}
}

static qog_gw_error_t GatewayWLANConnect(Gateway* m_gatewayInst)
{
	m_gatewayInst->Status = GW_WLAN_DISCONNECTED;

	m2m_wifi_connect((char*) m_gatewayInst->WLANConnection.WLAN_SSID,
			strlen((char*) m_gatewayInst->WLANConnection.WLAN_SSID),
			(tenuM2mSecType) m_gatewayInst->WLANConnection.WLAN_AUTH,
			(char*) m_gatewayInst->WLANConnection.WLAN_PSK, M2M_WIFI_CH_ALL);
	GatewayWaitForStatusChange(m_gatewayInst, GW_WLAN_CONNECTED,
			TIMEOUT_WIFI_WLAN_CONNECT);
	if (m_gatewayInst->Status != GW_WLAN_CONNECTED)
		return GW_e_WLAN_ERROR;
	return GW_e_OK;
}

static qog_gw_error_t GatewayResolveHost(Gateway* m_gatewayInst)
{
	if (m_gatewayInst->Status != GW_WLAN_CONNECTED)
		return GW_e_APPLICATION_ERROR;

	gethostbyname((uint8_t*) m_gatewayInst->BrokerParams.HostName);
	GatewayWaitForStatusChange(m_gatewayInst, GW_BROKER_DNS_RESOLVED,
			TIMEOUT_WIFI_RESOLVE_HOST);
	if (m_gatewayInst->Status != GW_BROKER_DNS_RESOLVED)
		return GW_e_HOST_ERROR;
	return GW_e_OK;
}

void GatewaySocketSend(Gateway* m_gatewayInst)
{
	uint8_t sendData = 0;
	xQueueReceive(m_gatewayInst->SocketTxQueue, sendData, 10);
	int32_t rc = 0;
	if (!send(Sockets[0].number, &sendData, 1, 0))
		rc = 1;
	else
		rc = -1;
	m2m_wifi_handle_events(NULL);
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
	}
	registerSocketCallback(socket_cb, dns_resolve_cb);

	//TODO get WLAN info -> Provisioning or Local Storage

	GatewayWLANConnect(m_gatewayInst);
	GatewayResolveHost(m_gatewayInst);
//	if (m_gatewayInst->Status == GW_BROKER_DNS_RESOLVED)
//		GatewaySocketOpen(Sockets, resolvedHostIp, gwInst);
	GatewaySocketSend(m_gatewayInst);
	TickType_t xLastWakeTime = xTaskGetTickCount();
	const TickType_t xFrequency = portTICK_PERIOD_MS * TASK_PERIOD_MS_WIFI;
	for (;;)
	{
		vTaskDelayUntil(&xLastWakeTime,xFrequency);
		switch (m_gatewayInst->Status)
		{
		case GW_STARTING:
			//TODO Arbitrar entre AP Config ou Utilizar WLAN armazenada
			break;
		case GW_AP_CONFIG_MODE:
			//TODO Ficar em AP_Config até terminar o processo ou por X segundos em caso de bateria
			break;
		case GW_BROKER_DNS_RESOLVED:
			m_gatewayInst->Status = GW_BROKER_SOCKET_CLOSED;
			break;
		case GW_BROKER_SOCKET_CLOSED:
			if (GatewaySocketOpen(TIMEOUT_SOCKET_OPEN) == GW_e_OK)
				m_gatewayInst->Status = GW_BROKER_SOCKET_OPEN;
			//TODO retry counter
			break;
		case GW_BROKER_SOCKET_OPEN:
			m2m_wifi_handle_events(NULL);
			break;
		case GW_MQTT_CLIENT_CONNECTED:
			break;
		case GW_MQTT_CLIENT_DISCONNECTED:
			break;
		case GW_WLAN_CONNECTED:
			//TODO update RTC via NTP UDP request
			//TODO retry counter
			if (GatewayResolveHost(m_gatewayInst) == GW_e_OK)
				m_gatewayInst->Status = GW_BROKER_DNS_RESOLVED;
			//TODO retry counter
			break;
		case GW_WLAN_DISCONNECTED:
			if (GatewayWLANConnect(m_gatewayInst) == GW_e_OK)
				m_gatewayInst->Status = GW_WLAN_CONNECTED;
			//TODO retry counter
			break;
		case GW_ERROR:
			break;
		default:
			break;
		}
	}
	return 0;
}

#endif
