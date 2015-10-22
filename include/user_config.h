#ifndef _USER_CONFIG_H_
#define _USER_CONFIG_H_

#define CFG_HOLDER	0x00FF55A	/* 0x00FF55A Change this value to load default configurations */
#define CFG_LOCATION	0x3C	/* Please don't change or if you know what you doing */
#define CLIENT_SSL_ENABLE

/*DEFAULT CONFIGURATIONS*/

#define MQTT_HOST			"192.168.1.104"
#define MQTT_PORT			1883
#define MQTT_BUF_SIZE		1024
#define MQTT_KEEPALIVE		120	 /*second*/

#define MQTT_CLIENT_ID		"esp8266"
#define MQTT_TOPIC_S01		"led1"
#define MQTT_TOPIC_S02		"led2"
#define MQTT_TOPIC_S03		"led3"
#define MQTT_USER			""
#define MQTT_PASS			""

#define STA_SSID "WifiName"
#define STA_PASS "WifiPass"
#define STA_TYPE AUTH_WPA2_PSK

#define MQTT_RECONNECT_TIMEOUT 	5	/*second*/

#define DEFAULT_SECURITY		0
#define QUEUE_BUFFER_SIZE		2048

#define PROTOCOL_NAMEv31	/*MQTT version 3.1 compatible with Mosquitto v0.15*/
//PROTOCOL_NAMEv311			/*MQTT version 3.11 compatible with https://eclipse.org/paho/clients/testing/*/
#endif

#define SWITCH01_GPIO 14
#define SWITCH01_GPIO_MUX PERIPHS_IO_MUX_MTMS_U
#define SWITCH01_GPIO_FUNC FUNC_GPIO14

#define SWITCH02_GPIO 12
#define SWITCH02_GPIO_MUX PERIPHS_IO_MUX_MTDI_U
#define SWITCH02_GPIO_FUNC FUNC_GPIO12

#define SWITCH03_GPIO 13
#define SWITCH03_GPIO_MUX PERIPHS_IO_MUX_MTCK_U
#define SWITCH03_GPIO_FUNC FUNC_GPIO13

#define BUTTON_GPIO 0
#define BUTTON_GPIO_MUX PERIPHS_IO_MUX_GPIO0_U
#define BUTTON_GPIO_FUNC FUNC_GPIO0
