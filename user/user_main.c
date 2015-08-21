/* main.c -- MQTT client example
*
* Copyright (c) 2014-2015, Tuan PM <tuanpm at live dot com>
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* * Redistributions of source code must retain the above copyright notice,
* this list of conditions and the following disclaimer.
* * Redistributions in binary form must reproduce the above copyright
* notice, this list of conditions and the following disclaimer in the
* documentation and/or other materials provided with the distribution.
* * Neither the name of Redis nor the names of its contributors may be used
* to endorse or promote products derived from this software without
* specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
* ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
* LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
* INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
* CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
* ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
* POSSIBILITY OF SUCH DAMAGE.
*/
#include "ets_sys.h"
#include "driver/uart.h"
#include "osapi.h"
#include "mqtt.h"
#include "wifi.h"
#include "config.h"
#include "debug.h"
#include "gpio.h"
#include "user_interface.h"
#include "mem.h"
#include "user_json.h"

MQTT_Client mqttClient;

#define PWM_CHANNEL	5  //  5:5channel ; 3:3channel
#define LIGHT_RED       0
#define LIGHT_GREEN     1
#define LIGHT_BLUE      2
#define LIGHT_COLD_WHITE      3
#define LIGHT_WARM_WHITE      4
char *pParseBuffer = NULL;

uint32 ICACHE_FLASH_ATTR
user_light_get_duty(uint8 channel)
{
    return 0;
}

uint32 ICACHE_FLASH_ATTR
user_light_get_period(void)
{
    return 0;
}

LOCAL int ICACHE_FLASH_ATTR
json_get(struct jsontree_context *js_ctx)
{
    const char *path = jsontree_path_name(js_ctx, js_ctx->depth - 1);

    if (os_strncmp(path, "switch", 6) == 0) {
        jsontree_write_string(js_ctx, "unknown");
    }

    return 0;
}

LOCAL int ICACHE_FLASH_ATTR
json_set(struct jsontree_context *js_ctx, struct jsonparse_state *parser)
{
    int type;
    while ((type = jsonparse_next(parser)) != 0) {
        if (type == JSON_TYPE_PAIR_NAME) {
            char buffer[64];
            os_bzero(buffer, 64);
            if (jsonparse_strcmp_value(parser, "switch") == 0) {
                jsonparse_next(parser);
                jsonparse_next(parser);
                jsonparse_copy_value(parser, buffer, sizeof(buffer));
                if (!strcoll(buffer, "on")) {
                	INFO("Switch on\n", buffer);
        			GPIO_OUTPUT_SET(SWITCH_GPIO, 1);
                } else if (!strcoll(buffer, "off")) {
                	INFO("Switch off\n", buffer);
        			GPIO_OUTPUT_SET(SWITCH_GPIO, 0);
                }
            }
        }
    }
    return 0;
}

LOCAL struct jsontree_callback switch_callback =
    JSONTREE_CALLBACK(json_get, json_set);
JSONTREE_OBJECT(switch_tree,
                JSONTREE_PAIR("switch", &switch_callback));
JSONTREE_OBJECT(PwmTree,
				JSONTREE_PAIR("root", &switch_tree));

void wifiConnectCb(uint8_t status)
{
	if(status == STATION_GOT_IP){
		MQTT_Connect(&mqttClient);
	} else {
		MQTT_Disconnect(&mqttClient);
	}
}

void mqttConnectedCb(uint32_t *args)
{
	MQTT_Client* client = (MQTT_Client*)args;
	INFO("MQTT: Connected\r\n");
	MQTT_Subscribe(client, sysCfg.mqtt_topic, 0);
}

void mqttDisconnectedCb(uint32_t *args)
{
	MQTT_Client* client = (MQTT_Client*)args;
	INFO("MQTT: Disconnected\r\n");
}

void mqttPublishedCb(uint32_t *args)
{
	MQTT_Client* client = (MQTT_Client*)args;
	INFO("MQTT: Published\r\n");
}

void mqttDataCb(uint32_t *args, const char* topic, uint32_t topic_len, const char *data, uint32_t data_len)
{
	char *topicBuf = (char*)os_zalloc(topic_len+1),
		 *dataBuf  = (char*)os_zalloc(data_len+1);

	MQTT_Client* client = (MQTT_Client*)args;

	os_memcpy(topicBuf, topic, topic_len);
	topicBuf[topic_len] = 0;

	os_memcpy(dataBuf, data, data_len);
	dataBuf[data_len] = 0;

	INFO("Receive topic: %s, data: %s \r\n", topicBuf, dataBuf);

	if (!strcoll(topicBuf, sysCfg.mqtt_topic)) {
		struct jsontree_context js;
		jsontree_setup(&js, (struct jsontree_value *)&PwmTree, json_putchar);
		json_parse(&js, dataBuf);
	}

	os_free(topicBuf);
	os_free(dataBuf);
}

void ICACHE_FLASH_ATTR
do_switch() {
	if (GPIO_REG_READ(BUTTON_GPIO) & BIT2) {
		INFO("Need to switch off\r\n");
		GPIO_OUTPUT_SET(SWITCH_GPIO, 0);
	} else  {
		INFO("Need to switch on\r\n");
		GPIO_OUTPUT_SET(SWITCH_GPIO, 1);
	}
}

void ICACHE_FLASH_ATTR
button_press() {
	ETS_GPIO_INTR_DISABLE(); // Disable gpio interrupts

	INFO("Button pressed\r\n");
	do_switch();
	os_delay_us(200000);	// Debounce

	//clear interrupt status
	uint32 gpio_status;
	gpio_status = GPIO_REG_READ(GPIO_STATUS_ADDRESS);
	GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, gpio_status);

	ETS_GPIO_INTR_ENABLE(); // Enable gpio interrupts
}

void ICACHE_FLASH_ATTR
gpio_init() {
	// Configure switch (relay)
	PIN_FUNC_SELECT(SWITCH_GPIO_MUX, SWITCH_GPIO_FUNC);
	GPIO_OUTPUT_SET(SWITCH_GPIO, 0);

	// Configure push button
	ETS_GPIO_INTR_DISABLE(); // Disable gpio interrupts
	ETS_GPIO_INTR_ATTACH(button_press, BUTTON_GPIO);  // GPIO0 interrupt handler
	PIN_FUNC_SELECT(BUTTON_GPIO_MUX, BUTTON_GPIO_FUNC); // Set function
	GPIO_DIS_OUTPUT(BUTTON_GPIO); // Set as input
	gpio_pin_intr_state_set(GPIO_ID_PIN(BUTTON_GPIO), 2); // Interrupt on negative edge
	ETS_GPIO_INTR_ENABLE(); // Enable gpio interrupts
}

void user_init(void)
{
	uart_init(BIT_RATE_115200, BIT_RATE_115200);
	INFO("\r\nSDK version: %s\n", system_get_sdk_version());
	INFO("System init...\r\n");
	system_set_os_print(1);
	os_delay_us(1000000);

	CFG_Load();
	gpio_init();

	MQTT_InitConnection(&mqttClient, sysCfg.mqtt_host, sysCfg.mqtt_port, sysCfg.security);
	MQTT_InitClient(&mqttClient, sysCfg.device_id, sysCfg.mqtt_user, sysCfg.mqtt_pass, sysCfg.mqtt_keepalive, 1);
	MQTT_InitLWT(&mqttClient, "/lwt", "offline", 0, 0);
	MQTT_OnConnected(&mqttClient, mqttConnectedCb);
	MQTT_OnDisconnected(&mqttClient, mqttDisconnectedCb);
	MQTT_OnPublished(&mqttClient, mqttPublishedCb);
	MQTT_OnData(&mqttClient, mqttDataCb);

	WIFI_Connect(sysCfg.sta_ssid, sysCfg.sta_pwd, wifiConnectCb);

	INFO("\r\nSystem started ...\r\n");
}

void user_rf_pre_init(void) {}
