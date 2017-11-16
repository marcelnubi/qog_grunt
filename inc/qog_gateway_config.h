/*
 * qog_gateway_config.h
 *
 *  Created on: Dec 1, 2016
 *      Author: Marcel
 */

#ifndef QOG_GATEWAY_CONFIG_H_
#define QOG_GATEWAY_CONFIG_H_

//Tasks
#define WIFI_TASK_HEAP 0x200
#define MQTT_PUBLISHER_TASK_HEAP 0x350
#define DATASOURCER_TASK_HEAP 0x150

#define TIMEOUT_SOCKET_OPEN 4000
#define TIMEOUT_WIFI_WLAN_CONNECT 5000
#define TIMEOUT_WIFI_RESOLVE_HOST 5000
#define TASK_PERIOD_MS_WIFI 100

#define OVS_MQTT_PUB_TOPIC_SIZE 39
#define OVS_MQTT_PUB_TOPIC_SZE_LIST 4
#define OVS_MQTT_PUB_TOPIC_SZE_ADD 3
#define OVS_MQTT_PUB_TOPIC_SZE_DROP 4
#define OVS_MQTT_PUB_TOPIC_SZE_LIST 4
#define OVS_MQTT_PUB_TOPIC_SZE_UPDATE 6
#define OVS_MQTT_PUB_TOPIC_SZE_WILDCARD 1
#define OVS_MQTT_PUB_TOPIC_SIZE_DATA 33

#define TCP_RX_SOCKET_BUFFER_SIZE 512
#define TCP_TX_SOCKET_BUFFER_SIZE 1000

#define OVS_COMMAND_QUEUE_SIZE 16

#define MAX_SAMPLE_BUFFER_SIZE 96
#define MAX_DATA_CHANNELS 48

#define MQTT_TIMEOUT_MS 1000
#define MQTT_RX_BUFFER_SIZE 512
#define MQTT_TX_BUFFER_SIZE 512
#define MQTT_CONNECT_RETRY_DELAY_MS 5000
#define MQTT_CLIENT_PUBLISH_RETRY 5
#define MQTT_CLIET_PUBLISH_RETRY_DELAY_MS 100
#define MQTT_TASK_LOOP_MS 100

#endif /* QOG_GATEWAY_CONFIG_H_ */
