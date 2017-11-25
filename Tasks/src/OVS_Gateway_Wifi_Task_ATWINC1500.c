/*
 * OVS_Gateway_Wifi_Task.c
 *
 *  Created on: Dec 2, 2016
 *      Author: Marcel
 */

#if defined(GW_WIFI_TASK_ATWINC_1500)

#include "Overseer_Connection.h"
#include "FreeRTOS.h"

#include "qog_gateway_error_types.h"
#include "qog_ovs_gateway_internal_types.h"
#include "qog_gateway_power.h"
#include "qog_gateway_system.h"

#include "../inc/OVS_Gateway_Task.h"
#include "driver/include/m2m_wifi.h"
#include "socket/include/socket.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"

//-------- Gateway Interface - START
static uint8_t NTP_Buffer[48];

static const uint8_t MQTT_SOCKET = 0;
static const uint8_t NTP_SOCKET = 1;

static qog_Task WifiTaskImpl(Gateway * gwInst);

qog_gateway_task WifiTaskDef = { &WifiTaskImpl, WIFI_TASK_HEAP, NULL };
//-------- Gateway Interface - END

#define MAX_OPEN_SOCKETS 8
typedef enum {
	SocketClosed = 0, SocketConnected, SocketWaiting, SocketError
} SocketStatus;

static struct {
	SocketStatus status;
	SOCKET number;
} Sockets[MAX_OPEN_SOCKETS];

static uint8_t txBuff[TCP_RX_SOCKET_BUFFER_SIZE];
static uint8_t rxBuff[TCP_RX_SOCKET_BUFFER_SIZE];

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
static void socket_cb(SOCKET sock, uint8_t u8Msg, void *pvMsg) {
	int16_t ret;
	switch (u8Msg) {
	/* Socket connected */
	case SOCKET_MSG_BIND: {
		tstrSocketBindMsg *pstrBind = (tstrSocketBindMsg *) pvMsg;
		if (pstrBind && pstrBind->status == 0) {
			ret = recvfrom(sock, NTP_Buffer, 48, 0);
			if (ret != SOCK_ERR_NO_ERROR) {
			}
		}
	}
		break;
	case SOCKET_MSG_CONNECT: {
		tstrSocketConnectMsg *pstrConnect = (tstrSocketConnectMsg *) pvMsg;
		if (pstrConnect->s8Error == 0) {
			if (pstrConnect->sock == Sockets[MQTT_SOCKET].number) {
				Sockets[MQTT_SOCKET].status = SocketConnected;
				m_gatewayInst->Status = GW_BROKER_SOCKET_OPEN;
			}
		} else {
			//TODO socket error
			m_gatewayInst->Status = GW_BROKER_SOCKET_CLOSED;
		}
		break;
	}
		/* Message send */
	case SOCKET_MSG_SEND: {
//		int16_t bytesSent = *(int16_t *) pvMsg;
	}
		break;
		/* Message receive */
	case SOCKET_MSG_RECVFROM: {
		tstrSocketRecvMsg *pstrRx = (tstrSocketRecvMsg *) pvMsg;
		uint8_t packetBuffer[48];
		memcpy(&packetBuffer, pstrRx->pu8Buffer, sizeof(packetBuffer));

		if ((packetBuffer[0] & 0x7) != 4) { /* expect only server response */
			return; /* MODE is not server, abort */
		} else {
			uint32_t secsSince1900 = packetBuffer[40] << 24
					| packetBuffer[41] << 16 | packetBuffer[42] << 8
					| packetBuffer[43];

			/* Now convert NTP time into everyday time.
			 * Unix time starts on Jan 1 1970. In seconds, that's 2208988800.
			 * Subtract seventy years.
			 */
			const uint32_t seventyYears = 2208988800UL;
			m_gatewayInst->TimeStamp = secsSince1900 - seventyYears;

			ret = close(Sockets[NTP_SOCKET].number);
			if (ret == SOCK_ERR_NO_ERROR) {
				Sockets[NTP_SOCKET].number = -1;
				Sockets[NTP_SOCKET].status = SocketClosed;
			}
		}
	}
		break;
	case SOCKET_MSG_RECV: {
		tstrSocketRecvMsg *pstrRecv = (tstrSocketRecvMsg *) pvMsg;
		if (pstrRecv && pstrRecv->s16BufferSize > 0) {
			uint16_t idx = 0;
			while (idx++ < pstrRecv->s16BufferSize) {
				xQueueSend(m_gatewayInst->SocketRxQueue, pstrRecv->pu8Buffer++,
						0);
			}
		} else {
			close(Sockets[MQTT_SOCKET].number);
			m_gatewayInst->Status = GW_BROKER_SOCKET_CLOSED;
		}
		break;

		default:
		break;
	}
	}
}

static void wifi_cb(uint8_t u8MsgType, void *pvMsg) {
	switch (u8MsgType) {
	case M2M_WIFI_RESP_CON_STATE_CHANGED: {
		tstrM2mWifiStateChanged *pstrWifiState =
				(tstrM2mWifiStateChanged *) pvMsg;
		if (pstrWifiState->u8CurrState == M2M_WIFI_CONNECTED) {
			m2m_wifi_request_dhcp_client();
		} else if (pstrWifiState->u8CurrState == M2M_WIFI_DISCONNECTED) {
			if (m_gatewayInst->Status == GW_WLAN_CONNECTED)
				m_gatewayInst->Status = GW_WLAN_DISCONNECTED;
		}

	}
		break;
	case M2M_WIFI_REQ_DHCP_CONF: {
		if (m_gatewayInst->Status == GW_WLAN_DISCONNECTED)
			m_gatewayInst->Status = GW_WLAN_CONNECTED;
	}
		break;
	case M2M_WIFI_RESP_IP_CONFLICT: {
		m2m_wifi_request_dhcp_client();
	}
		break;
	case M2M_WIFI_RESP_GET_SYS_TIME: {
		tstrSystemTime * time = (tstrSystemTime*) pvMsg;
		qog_DateTime dt;
		dt.Time.Hours = time->u8Hour;
		dt.Time.Minutes = time->u8Minute;
		dt.Time.Seconds = time->u8Second;
		dt.Date.Date = time->u8Day;
		dt.Date.Month = time->u8Month;
		dt.Date.Year = time->u16Year - 2000;

		qog_gw_sys_setTime(&dt);
		//m_gatewayInst->TimeStamp = qog_gw_sys_getTimestamp();
	}
		break;
	case M2M_WIFI_RESP_PROVISION_INFO: {
		tstrM2MProvisionInfo *pstrProvInfo = (tstrM2MProvisionInfo *) pvMsg;
		printf("wifi_cb: M2M_WIFI_RESP_PROVISION_INFO.\r\n");

		if (pstrProvInfo->u8Status == M2M_SUCCESS) {
			sprintf((char *) m_gatewayInst->WLANConnection.WLAN_SSID, "%s",
					pstrProvInfo->au8SSID);
			sprintf((char *) m_gatewayInst->WLANConnection.WLAN_PSK, "%s",
					pstrProvInfo->au8Password);
			m_gatewayInst->WLANConnection.WLAN_AUTH = pstrProvInfo->u8SecType;
			m_gatewayInst->WLANConnection.valid = true;
			m_gatewayInst->Status = GW_WLAN_DISCONNECTED;
		}
	}
		break;
	default: {
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
static void dns_resolve_cb(uint8_t *hostName, uint32_t hostIp) {
	m_gatewayInst->BrokerParams.HostIp = hostIp;
	m_gatewayInst->Status = GW_BROKER_DNS_RESOLVED;
}

static qog_gw_error_t GatewaySocketOpen(uint32_t timeout, uint8_t SockNumber,
		uint32_t SockHost, uint16_t SockPort) {
	close(Sockets[SockNumber].number);
	Sockets[SockNumber].status = SocketClosed;
	m_gatewayInst->Status = GW_BROKER_SOCKET_CLOSED;

	while (Sockets[SockNumber].status != SocketConnected) {
		switch (Sockets[SockNumber].status) {
		case SocketClosed: {
			struct sockaddr_in addr_in;
			addr_in.sin_family = AF_INET;
			addr_in.sin_port = _htons(SockPort);
			addr_in.sin_addr.s_addr = SockHost;
			Sockets[SockNumber].number = socket(AF_INET, SOCK_STREAM, 0);
			if (connect(Sockets[SockNumber].number,
					(struct sockaddr *) &addr_in,
					sizeof(struct sockaddr_in)) == SOCK_ERR_NO_ERROR)
				Sockets[SockNumber].status = SocketWaiting;
		}
			break;
		case SocketWaiting: {
			vTaskDelay(TASK_PERIOD_MS_WIFI / 10);
			m2m_wifi_handle_events(NULL);
			//TODO retry timeout
		}
			break;
		case SocketError: {
			//TODO Tratar erro de socket, decidir se continua ou se para
			Sockets[SockNumber].status = SocketClosed;
		}
			break;
		case SocketConnected:
			break;
		}
		//send tx queue byte
		//TODO timeout
	}
	return GW_e_OK;
}

static void GatewayWaitForStatusChange(Gateway* m_gatewayInst,
		GatewayStatus status, TickType_t timeout, TickType_t pollingTime) {
	TickType_t xTicksToWait = timeout;
	TimeOut_t xTimeOut;

	vTaskSetTimeOutState(&xTimeOut);

	while (m_gatewayInst->Status != status) //TODO Timeout
	{
		vTaskDelay(pollingTime);
		m2m_wifi_handle_events(NULL);

		if (xTaskCheckForTimeOut(&xTimeOut, &xTicksToWait) != pdFALSE) {
			/* Timed out before the wanted number of bytes were available, exit the
			 loop. */
			m_gatewayInst->Status = GW_ERROR;
			break;
		}
		if (m_gatewayInst->Status == GW_ERROR)
			break;
	}
}

static qog_gw_error_t GatewayWLANConnect(Gateway* m_gatewayInst) {
	m_gatewayInst->Status = GW_WLAN_DISCONNECTED;

	if (m_gatewayInst->WLANConnection.valid)
		m2m_wifi_connect((char*) m_gatewayInst->WLANConnection.WLAN_SSID,
				strlen((char*) m_gatewayInst->WLANConnection.WLAN_SSID),
				(tenuM2mSecType) m_gatewayInst->WLANConnection.WLAN_AUTH,
				(char*) m_gatewayInst->WLANConnection.WLAN_PSK,
				M2M_WIFI_CH_ALL);
	else
		m2m_wifi_default_connect();
	GatewayWaitForStatusChange(m_gatewayInst, GW_WLAN_CONNECTED,
	TIMEOUT_WIFI_WLAN_CONNECT, TASK_PERIOD_MS_WIFI);
	if (m_gatewayInst->Status != GW_WLAN_CONNECTED)
		return GW_e_WLAN_ERROR;
	return GW_e_OK;
}

static qog_gw_error_t GatewayResolveHost(Gateway* m_gatewayInst) {
	if (m_gatewayInst->Status != GW_WLAN_CONNECTED)
		return GW_e_APPLICATION_ERROR;

	gethostbyname((uint8_t*) m_gatewayInst->BrokerParams.HostName);
	GatewayWaitForStatusChange(m_gatewayInst, GW_BROKER_DNS_RESOLVED,
	TIMEOUT_WIFI_RESOLVE_HOST, TASK_PERIOD_MS_WIFI);
	if (m_gatewayInst->Status != GW_BROKER_DNS_RESOLVED)
		return GW_e_HOST_ERROR;
	return GW_e_OK;
}

void GatewaySocketSend(Gateway* m_gatewayInst) {
	uint32_t sendData = 0;
	xQueueReceive(m_gatewayInst->SocketTxQueue, &sendData, 10);
	int32_t rc = 0;
	if (!send(Sockets[0].number, &sendData, 1, 0))
		rc = 1;
	else
		rc = -1;
	rc = rc;
	m2m_wifi_handle_events(NULL);
}

static qog_gw_error_t GatewayGetSystemTimeNTP(TickType_t timeout,
		TickType_t pollingTime) {
	m_gatewayInst->TimeStamp = 0;
	Sockets[NTP_SOCKET].number = socket(AF_INET, SOCK_DGRAM, 0);

	struct sockaddr_in addr;
	int8_t cDataBuf[48];
	int16_t ret;
	memset(cDataBuf, 0, sizeof(cDataBuf));
	cDataBuf[0] = '\x1b'; /* time query */
	/* Set NTP server socket address structure. */
	addr.sin_family = AF_INET;
	addr.sin_port = _htons(NTP_SERVER_PORT);
	addr.sin_addr.s_addr = _htonl(NTP_SERVER_IP);
	bind(Sockets[NTP_SOCKET].number, (struct sockaddr*) &addr,
			sizeof(struct sockaddr_in));
	/*Send an NTP time query to the NTP server*/
	m2m_wifi_handle_events(NULL);
	ret = sendto(Sockets[NTP_SOCKET].number, (int8_t*) &cDataBuf,
			sizeof(cDataBuf), 0, (struct sockaddr*) &addr, sizeof(addr));

	TickType_t xTicksToWait = timeout;
	TimeOut_t xTimeOut;

	vTaskSetTimeOutState(&xTimeOut);

	while (m_gatewayInst->TimeStamp == 0) //TODO Timeout
	{
		vTaskDelay(pollingTime);
		m2m_wifi_handle_events(NULL);

		if (xTaskCheckForTimeOut(&xTimeOut, &xTicksToWait) != pdFALSE) {
			/* Timed out before the wanted number of bytes were available, exit the
			 loop. */
			m_gatewayInst->Status = GW_ERROR;
			break;
		}
		if (m_gatewayInst->Status == GW_ERROR)
			break;
	}

	if (ret != M2M_SUCCESS) {
		return GW_e_APPLICATION_ERROR;
	}
	return GW_e_OK;
}

void GatewaySocketInit() {
	socketDeinit();
	for (uint8_t i = 0; i < MAX_OPEN_SOCKETS; i++)
		Sockets[i].number = -1;

	socketInit();
	registerSocketCallback(socket_cb, dns_resolve_cb);
}

void wifiInit() {
	tstrWifiInitParam param;
	int8_t ret;
	memset((uint8_t*) &param, 0, sizeof(tstrWifiInitParam));
	qog_gw_pwr_wifi_disable();
	HAL_Delay(200);
	qog_gw_pwr_wifi_enable();
	HAL_Delay(200);
	nm_bsp_init();
	/* USER CODE BEGIN 2 */
	/* Initialize Wi-Fi driver with data and status callbacks. */
	param.pfAppWifiCb = wifi_cb;
	ret = m2m_wifi_init(&param);
	if (M2M_SUCCESS != ret) {
		//TODO Debug port out
	}
	GatewaySocketInit();
}

qog_Task WifiTaskImpl(Gateway * gwInst) {
	m_gatewayInst = gwInst;

	WLANConnectionParams asd = { 0 };
	m_gatewayInst->WLANConnection = asd;

	wifiInit();

	for (;;) {
		vTaskDelay(TASK_PERIOD_MS_WIFI);
		switch (m_gatewayInst->Status) {
		case GW_STARTING:
			//TODO Arbitrar entre AP Config ou Utilizar WLAN armazenada
			m_gatewayInst->Status = GW_WLAN_DISCONNECTED;
			break;
		case GW_AP_CONFIG_MODE: {
			wifiInit();
			tstrM2MAPConfig apConfig = { "QOGNI_CONFIG", 1, 0,
			WEP_40_KEY_STRING_SIZE, "qognata33", (uint8) M2M_WIFI_SEC_OPEN,
					SSID_MODE_VISIBLE };

			m2m_wifi_set_device_name((uint8_t *) "QOGNI_CONFIG", 3);
			apConfig.au8DHCPServerIP[0] = 0xC0; /* 192 */
			apConfig.au8DHCPServerIP[1] = 0xA8; /* 168 */
			apConfig.au8DHCPServerIP[2] = 0x01; /* 1 */
			apConfig.au8DHCPServerIP[3] = 0x01; /* 1 */
			m2m_wifi_start_provision_mode((tstrM2MAPConfig *) &apConfig,
					(char *) "config.qogni.com", 1);

			m_gatewayInst->Status = GW_AP_CONFIG_MODE_STDBY;
		}
			//TODO Ficar em AP_Config até terminar o processo ou por X segundos em caso de bateria
			break;
		case GW_AP_CONFIG_MODE_STDBY: {
			m2m_wifi_handle_events(NULL);
		}
			break;
		case GW_BROKER_DNS_RESOLVED:
			m_gatewayInst->Status = GW_BROKER_SOCKET_CLOSED;
			break;
		case GW_BROKER_SOCKET_CLOSED:
			Sockets[MQTT_SOCKET].number = socket(AF_INET, SOCK_STREAM, 0);

			if (GatewaySocketOpen(TIMEOUT_SOCKET_OPEN, MQTT_SOCKET,
					m_gatewayInst->BrokerParams.HostIp,
					m_gatewayInst->BrokerParams.HostPort) == GW_e_OK)
				m_gatewayInst->Status = GW_BROKER_SOCKET_OPEN;

			//TODO retry counter
			break;
		case GW_BROKER_SOCKET_OPEN: {
			uint16_t qBuffSize = 0;
			uint16_t qSize = uxQueueMessagesWaiting(
					m_gatewayInst->SocketTxQueue);

			while (qSize > 0) {
				xQueueReceive(m_gatewayInst->SocketTxQueue, &txBuff[qBuffSize++],
						0);
				qSize--;
			}
			if (qBuffSize)
				send(Sockets[MQTT_SOCKET].number, txBuff, qBuffSize, 0);

			recv(Sockets[MQTT_SOCKET].number, rxBuff,
					sizeof(TCP_RX_SOCKET_BUFFER_SIZE), 0);

			m2m_wifi_handle_events(NULL);
		}
			break;
		case GW_MQTT_CLIENT_CONNECTED:
			break;
		case GW_MQTT_CLIENT_DISCONNECTED:
			m_gatewayInst->Status = GW_ERROR;
			break;
		case GW_WLAN_CONNECTED:
			//TODO retry counter
			if (GatewayGetSystemTimeNTP(TIMEOUT_SOCKET_OPEN,
			TASK_PERIOD_MS_WIFI) != GW_e_OK)
				break;
			//TODO update RTC com timestamp do gateway
			if (GatewayResolveHost(m_gatewayInst) == GW_e_OK)
				m_gatewayInst->Status = GW_BROKER_DNS_RESOLVED;
			//TODO retry counter
			break;
		case GW_WLAN_DISCONNECTED: {
			uint8_t retry = 0;
			wifiInit();
			for (retry = OVS_WLAN_RETRY; retry > 0; retry--) {
				if (GatewayWLANConnect(m_gatewayInst) == GW_e_OK) {
					m_gatewayInst->Status = GW_WLAN_CONNECTED;
					break;
				}
			}
			if (retry == 0)
				m_gatewayInst->Status = GW_AP_CONFIG_MODE;
			//TODO retry counter
		}
			break;
		case GW_ERROR:
			HAL_Delay(1000);
			m_gatewayInst->Status = GW_STARTING;
			break;
		default:
			break;
		}
	}
	return 0;
}

#endif
