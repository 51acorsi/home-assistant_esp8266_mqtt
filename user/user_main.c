/*
 *  MQTT Switch
 *
 *  This firmware is meant to control a relay in e.g. a power outlet or power strip
 *  through MQTT.
 *
 *  The ESP8266 will register itself with the MQTT server and will listen to topic
 *  /DeviceX/<chip-ID>. Inbound message are expected to be formatted as JSON messages
 *  and will be parsed for switching instruction. Please find a valid JSON instruction
 *  below:
 *
 *  {"switch":"off"}
 *
 *  The relay is supposed to be connected to ESP Pin GPIO2
 *  To experiment with the firmware, a LED will of course also do.
 *
 *  Optionally a push button can be connected meant to override messages from the
 *  MQTT broker, allowing you to physically switch the relay as well.
 *  When the push button is pressed, the relay will change its state and a JSON
 *  message is sent to the MQTT server indicating its new state.
 *  The optional push button should be connected to ESP Pin GPIO0 and when the
 *  button is pressed, this pin should be grounded.
 *
 *  (c) 2015 by Jan Penninkhof <jan@penninkhof.com>
 *
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

//TODO: Move all this to real configuration
#define MQTT_TOPIC_UPDATE		"set"
#define MQTT_SEPARATOR			"/"

#define TOGGLE01_GPIO 5
#define TOGGLE01_GPIO_MUX PERIPHS_IO_MUX_GPIO5_U
#define TOGGLE01_GPIO_FUNC FUNC_GPIO5

#define TOGGLE02_GPIO 4
#define TOGGLE02_GPIO_MUX PERIPHS_IO_MUX_GPIO4_U
#define TOGGLE02_GPIO_FUNC FUNC_GPIO4

#define TOGGLE03_GPIO 15
#define TOGGLE03_GPIO_MUX PERIPHS_IO_MUX_MTDO_U
#define TOGGLE03_GPIO_FUNC FUNC_GPIO15

int switch01Status = 3;
int switch02Status = 3;
int switch03Status = 3;

MQTT_Client mqttClient;

LOCAL int ICACHE_FLASH_ATTR
json_get(struct jsontree_context *js_ctx)
{
    const char *path = jsontree_path_name(js_ctx, js_ctx->depth - 1);
    if (os_strncmp(path, "switch", 6) == 0) {
        jsontree_write_string(js_ctx, GPIO_REG_READ(BUTTON_GPIO) & BIT2 ? "on" : "off");
    }
    return 0;
}

LOCAL int ICACHE_FLASH_ATTR
json_set(struct jsontree_context *js_ctx, struct jsonparse_state *parser)
{
	int type;
	INFO("Json set/n");
	INFO(parser);
	INFO("/n");
    while ((type = jsonparse_next(parser)) != 0) {
    	INFO (parser);
    	INFO("/n");
        if (type == JSON_TYPE_PAIR_NAME) {
            char buffer[64];
            os_bzero(buffer, 64);
            if (jsonparse_strcmp_value(parser, "switch") == 0) {
                jsonparse_next(parser);
                jsonparse_next(parser);
                jsonparse_copy_value(parser, buffer, sizeof(buffer));
                if (!strcoll(buffer, "on")) {
                	INFO("JSON: Switch on\n", buffer);
        			GPIO_OUTPUT_SET(SWITCH03_GPIO, 1);
                } else if (!strcoll(buffer, "off")) {
                	INFO("JSON: Switch off\n", buffer);
        			GPIO_OUTPUT_SET(SWITCH03_GPIO, 0);
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
JSONTREE_OBJECT(device_tree,
				JSONTREE_PAIR("device", &switch_tree));

void ICACHE_FLASH_ATTR
wifi_connect_cb(uint8_t status)
{
	if(status == STATION_GOT_IP){
		MQTT_Connect(&mqttClient);
	} else {
		MQTT_Disconnect(&mqttClient);
	}
}

void ICACHE_FLASH_ATTR
mqtt_connected_cb(uint32_t *args)
{
	MQTT_Client* client = (MQTT_Client*)args;
	INFO("MQTT: Connected\r\n");
	INFO("MQTT: Subscribe Topic: %s\n", config.mqtt_topic_s01);
	MQTT_Subscribe(client, config.mqtt_topic_s01, 0);
	MQTT_Subscribe(client, config.mqtt_topic_s02, 0);
	MQTT_Subscribe(client, config.mqtt_topic_s03, 0);
}

void ICACHE_FLASH_ATTR
mqtt_disconnected_cb(uint32_t *args)
{
	MQTT_Client* client = (MQTT_Client*)args;
	INFO("MQTT: Disconnected\r\n");
}

void ICACHE_FLASH_ATTR
mqtt_published_cb(uint32_t *args)
{
	MQTT_Client* client = (MQTT_Client*)args;
	INFO("MQTT: Published\r\n");
}

void ICACHE_FLASH_ATTR
notify_switch_status(int gpio, int status) {

	char *payload;
	char *topic;

	//Set topic
	switch (gpio) {
		case SWITCH01_GPIO:
			//topic = (char*)os_zalloc(strlen(config.mqtt_topic_s01)+strlen(MQTT_SEPARATOR)+strlen(MQTT_TOPIC_UPDATE)+1);
			//strcpy(topic, config.mqtt_topic_s01);
			//strcpy(topic, MQTT_SEPARATOR);
			//strcpy(topic, MQTT_TOPIC_UPDATE);
			//strcpy(topic, '\0');
			topic = (char*)os_zalloc(strlen(config.mqtt_topic_s01));
			topic = config.mqtt_topic_s01;
			break;
		case SWITCH02_GPIO:
			//topic = (char*)os_zalloc(strlen(config.mqtt_topic_s02)+strlen(MQTT_SEPARATOR)+strlen(MQTT_TOPIC_UPDATE)+1);
			//strcpy(topic, config.mqtt_topic_s02);
			//strcpy(topic, MQTT_SEPARATOR);
			//strcpy(topic, MQTT_TOPIC_UPDATE);
			//strcpy(topic, '\0');
			topic = (char*)os_zalloc(strlen(config.mqtt_topic_s02));
			topic = config.mqtt_topic_s02;
			break;
		case SWITCH03_GPIO:
			//topic = (char*)os_zalloc(strlen(config.mqtt_topic_s03)+strlen(MQTT_SEPARATOR)+strlen(MQTT_TOPIC_UPDATE)+1);
			//strcpy(topic, config.mqtt_topic_s03);
			//strcpy(topic, MQTT_SEPARATOR);
			//strcpy(topic, MQTT_TOPIC_UPDATE);
			//strcpy(topic, '\0');
			topic = (char*)os_zalloc(strlen(config.mqtt_topic_s03));
			topic = config.mqtt_topic_s03;
			break;
		default:
			INFO("Notification: No topic for switch %d\n", gpio);
			return;
	}

	//Set payoad
	switch (status) {
		case 0:
			payload = (char*)os_zalloc(strlen("off\0"));
			payload = "off\0";
			break;
		case 1:
			payload = (char*)os_zalloc(strlen("on\0"));
			payload = "on\0";
			break;
		default:
			INFO("Notification: Status %d not identified\n", status);
			return;
	}

	INFO("NOTIFICATION: Sending switch status\nTopic: %s\nPayload: %s\n",config.mqtt_topic_s01 ,payload);
	MQTT_Publish(&mqttClient, topic, payload, strlen(payload), 0, 0);
}

void ICACHE_FLASH_ATTR
set_switch(int gpio, int status) {
	//TODO: Send Notification

	INFO("SWITCH: Set switch request:\nSwitch:%d\nStatus: %d\n", SWITCH01_GPIO, status);

	//Change status
	switch (gpio)
	{
	case SWITCH01_GPIO:
		if (status != switch01Status)
		{
			INFO("SWITCH: Set switch %d %d\n", SWITCH01_GPIO, status);
			GPIO_OUTPUT_SET(SWITCH01_GPIO, status);
			switch01Status = status;
			notify_switch_status(SWITCH01_GPIO, status);
		}
		break;
	case SWITCH02_GPIO:
		if (status != switch02Status)
		{
			INFO("SWITCH: Set switch %d %d\n", SWITCH02_GPIO, status);
			GPIO_OUTPUT_SET(SWITCH02_GPIO, status);
			switch02Status = status;
			notify_switch_status(SWITCH02_GPIO, status);
		}
		break;
	case SWITCH03_GPIO:
		if (status != switch03Status)
		{
			INFO("SWITCH: Set switch %d %d\n", SWITCH03_GPIO, status);
			GPIO_OUTPUT_SET(SWITCH03_GPIO, status);
			switch03Status = status;
			notify_switch_status(SWITCH03_GPIO, status);
		}
		break;
	}
}

void ICACHE_FLASH_ATTR
mqtt_data_cb(uint32_t *args, const char* topic, uint32_t topic_len, const char *data, uint32_t data_len)
{
	char *topic_buf = (char*)os_zalloc(topic_len+1),
		 *data_buf  = (char*)os_zalloc(data_len+1);

	MQTT_Client* client = (MQTT_Client*)args;

	os_memcpy(topic_buf, topic, topic_len);
	topic_buf[topic_len] = 0;

	os_memcpy(data_buf, data, data_len);
	data_buf[data_len] = 0;

	INFO("MQTT: Received data on topic: \n%s\r\n", topic_buf);

	char strData[data_len + 1];
		os_memcpy(strData, data, data_len);
		strData[data_len] = '\0';

	int statusCommand = -1;

	INFO("MQTT: Data: %s\n", strData);

	if (!strcoll(strData, "on")) {
		statusCommand = 1;
	}
	else if (!strcoll(strData, "off")) {
		statusCommand = 0;
	}

	if (statusCommand == -1)
		return;

	if (!strcoll(topic_buf, config.mqtt_topic_s01))
	{
		INFO("Switch %d %s\n", SWITCH01_GPIO, strData);
		set_switch(SWITCH01_GPIO, statusCommand);
		//GPIO_OUTPUT_SET(SWITCH01_GPIO, statusCommand);
	}
	else if (!strcoll(topic_buf, config.mqtt_topic_s02))
	{
		INFO("Switch %d %s\n", SWITCH02_GPIO, strData);
		set_switch(SWITCH02_GPIO, statusCommand);
		//GPIO_OUTPUT_SET(SWITCH02_GPIO, statusCommand);
	}
	else if (!strcoll(topic_buf, config.mqtt_topic_s03))
	{
		INFO("Switch %d %s\n", SWITCH03_GPIO, strData);
		set_switch(SWITCH03_GPIO, statusCommand);
		//GPIO_OUTPUT_SET(SWITCH03_GPIO, statusCommand);
	}

		//!strcoll(topic_buf, config.mqtt_topic_s02) ||
		//!strcoll(topic_buf, config.mqtt_topic_s03)) {
		//struct jsontree_context js;
		//jsontree_setup(&js, (struct jsontree_value *)&device_tree, json_putchar);
		//json_parse(&js, data_buf);

	os_free(topic_buf);
	os_free(data_buf);
}

void ICACHE_FLASH_ATTR
toggle_changed() {
	//TODO: Refactor this shit
	ETS_GPIO_INTR_DISABLE(); // Disable gpio interrupts

	uint32 gpio_status;
	gpio_status = GPIO_REG_READ(GPIO_STATUS_ADDRESS);

	//INFO("TOGGLE: Toggle Switch %d pressed\n", index);

	if (gpio_status & BIT(TOGGLE01_GPIO))
	{
		INFO("TOGGLE: Toggle Switch %d pressed\n", TOGGLE01_GPIO);
		if (switch01Status == 0)
		{
			set_switch(SWITCH01_GPIO, 1);
		}
		else
		{
			set_switch(SWITCH01_GPIO, 0);
		}
	}
	else if (gpio_status & BIT(TOGGLE02_GPIO))
	{
		INFO("TOGGLE: Toggle Switch %d pressed\n", TOGGLE02_GPIO);
		if (switch02Status == 0)
		{
			set_switch(SWITCH02_GPIO, 1);
		}
		else
		{
			set_switch(SWITCH02_GPIO, 0);
		}
	}
	else if (gpio_status & BIT(TOGGLE03_GPIO))
	{
		INFO("TOGGLE: Toggle Switch %d pressed\n", TOGGLE03_GPIO);
		if (switch03Status == 0)
		{
			set_switch(SWITCH03_GPIO,1);
		}
		else
		{
			set_switch(SWITCH03_GPIO,0);
		}
	}

	//Flip Status
/*
	// Button interrupt received
	INFO("BUTTON: Button pressed\r\n");

	// Button pressed, flip switch
	if (GPIO_REG_READ(BUTTON_GPIO) & BIT2) {
		INFO("BUTTON: Switch off\r\n");
		GPIO_OUTPUT_SET(SWITCH03_GPIO, 0);
	} else  {
		INFO("BUTTON: Switch on\r\n");
		GPIO_OUTPUT_SET(SWITCH03_GPIO, 1);
	}

	// Send new status to the MQTT broker
	char *json_buf = NULL;
	json_buf = (char *)os_zalloc(jsonSize);
	json_ws_send((struct jsontree_value *)&device_tree, "device", json_buf);
	INFO("BUTTON: Sending current switch status\r\n");
	MQTT_Publish(&mqttClient, config.mqtt_topic_s01, json_buf, strlen(json_buf), 0, 0);
	os_free(json_buf);
	json_buf = NULL;
*/
	// Debounce
	os_delay_us(200000);

	// Clear interrupt status
	//uint32 gpio_status;
	//gpio_status = GPIO_REG_READ(GPIO_STATUS_ADDRESS);
	GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, gpio_status);

	ETS_GPIO_INTR_ENABLE(); // Enable gpio interrupts
}

void ICACHE_FLASH_ATTR
button_press() {
	ETS_GPIO_INTR_DISABLE(); // Disable gpio interrupts

	// Button interrupt received
	INFO("BUTTON: Button pressed\r\n");

	// Button pressed, flip switch
	if (GPIO_REG_READ(BUTTON_GPIO) & BIT2) {
		INFO("BUTTON: Switch off\r\n");
		GPIO_OUTPUT_SET(SWITCH03_GPIO, 0);
	} else  {
		INFO("BUTTON: Switch on\r\n");
		GPIO_OUTPUT_SET(SWITCH03_GPIO, 1);
	}

	// Send new status to the MQTT broker
	char *json_buf = NULL;
	json_buf = (char *)os_zalloc(jsonSize);
	json_ws_send((struct jsontree_value *)&device_tree, "device", json_buf);
	INFO("BUTTON: Sending current switch status\r\n");
	MQTT_Publish(&mqttClient, config.mqtt_topic_s01, json_buf, strlen(json_buf), 0, 0);
	os_free(json_buf);
	json_buf = NULL;

	// Debounce
	os_delay_us(200000);

	// Clear interrupt status
	uint32 gpio_status;
	gpio_status = GPIO_REG_READ(GPIO_STATUS_ADDRESS);
	GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, gpio_status);

	ETS_GPIO_INTR_ENABLE(); // Enable gpio interrupts
}

void ICACHE_FLASH_ATTR
gpio_init() {
	// Configure switch (relays)
	INFO("Configure Switch 1 %d\n", SWITCH01_GPIO );
	PIN_FUNC_SELECT(SWITCH01_GPIO_MUX, SWITCH01_GPIO_FUNC);
	set_switch(SWITCH01_GPIO, 0);
	//GPIO_OUTPUT_SET(SWITCH01_GPIO, 0);

	INFO("Configure Switch 2 %d\n", SWITCH02_GPIO );
	PIN_FUNC_SELECT(SWITCH02_GPIO_MUX, SWITCH02_GPIO_FUNC);
	set_switch(SWITCH02_GPIO, 0);
	//GPIO_OUTPUT_SET(SWITCH02_GPIO, 0);

	INFO("Configure Switch 3 %d\n", SWITCH03_GPIO );
	PIN_FUNC_SELECT(SWITCH03_GPIO_MUX, SWITCH03_GPIO_FUNC);
	set_switch(SWITCH03_GPIO, 0);
	//GPIO_OUTPUT_SET(SWITCH03_GPIO, 0);

	//Configure Toggle switches
	INFO("Configure Toggle 1 %d\n", TOGGLE01_GPIO );
	ETS_GPIO_INTR_DISABLE(); // Disable gpio interrupts
	ETS_GPIO_INTR_ATTACH(toggle_changed, 0);  // GPIO interrupt handler
	PIN_FUNC_SELECT(TOGGLE01_GPIO_MUX, TOGGLE01_GPIO_FUNC); // Set function
	GPIO_DIS_OUTPUT(TOGGLE01_GPIO); // Set as input
	gpio_pin_intr_state_set(GPIO_ID_PIN(TOGGLE01_GPIO), GPIO_PIN_INTR_ANYEDGE); // Interrupt on any edge
	//ETS_GPIO_INTR_ENABLE(); // Enable gpio interrupts

	INFO("Configure Toggle 2 %d\n", TOGGLE02_GPIO );
	//ETS_GPIO_INTR_DISABLE(); // Disable gpio interrupts
	//ETS_GPIO_INTR_ATTACH(toggle_changed);  // GPIO interrupt handler
	PIN_FUNC_SELECT(TOGGLE02_GPIO_MUX, TOGGLE02_GPIO_FUNC); // Set function
	GPIO_DIS_OUTPUT(TOGGLE02_GPIO); // Set as input
	gpio_pin_intr_state_set(GPIO_ID_PIN(TOGGLE02_GPIO), GPIO_PIN_INTR_ANYEDGE); // Interrupt on any edge
	//ETS_GPIO_INTR_ENABLE(); // Enable gpio interrupts


	INFO("Configure Toggle 3 %d\n", TOGGLE03_GPIO );
	//ETS_GPIO_INTR_DISABLE(); // Disable gpio interrupts
	//ETS_GPIO_INTR_ATTACH(toggle_changed);  // GPIO interrupt handler
	PIN_FUNC_SELECT(TOGGLE03_GPIO_MUX, TOGGLE03_GPIO_FUNC); // Set function
	GPIO_DIS_OUTPUT(TOGGLE03_GPIO); // Set as input
	gpio_pin_intr_state_set(GPIO_ID_PIN(TOGGLE03_GPIO), GPIO_PIN_INTR_ANYEDGE); // Interrupt on any edge
	ETS_GPIO_INTR_ENABLE(); // Enable gpio interrupts


	// Configure push button
//	INFO("Confgiure push button %d\n", BUTTON_GPIO );
//	ETS_GPIO_INTR_DISABLE(); // Disable gpio interrupts
//	ETS_GPIO_INTR_ATTACH(button_press, BUTTON_GPIO);  // GPIO0 interrupt handler
//	PIN_FUNC_SELECT(BUTTON_GPIO_MUX, BUTTON_GPIO_FUNC); // Set function
//	GPIO_DIS_OUTPUT(BUTTON_GPIO); // Set as input
//	gpio_pin_intr_state_set(GPIO_ID_PIN(BUTTON_GPIO), 2); // Interrupt on negative edge
//	ETS_GPIO_INTR_ENABLE(); // Enable gpio interrupts
}

void ICACHE_FLASH_ATTR
mqtt_init() {
	MQTT_InitConnection(&mqttClient, config.mqtt_host, config.mqtt_port, config.security);
	MQTT_InitClient(&mqttClient, config.device_id, config.mqtt_user, config.mqtt_pass, config.mqtt_keepalive, 1);
	MQTT_InitLWT(&mqttClient, "/lwt", "offline", 0, 0);
	MQTT_OnConnected(&mqttClient, mqtt_connected_cb);
	MQTT_OnDisconnected(&mqttClient, mqtt_disconnected_cb);
	MQTT_OnPublished(&mqttClient, mqtt_published_cb);
	MQTT_OnData(&mqttClient, mqtt_data_cb);
}

void ICACHE_FLASH_ATTR
user_init(void)
{
	uart_init(BIT_RATE_115200, BIT_RATE_115200);
	INFO("\r\nSDK version: %s\n", system_get_sdk_version());
	INFO("System init...\r\n");
	system_set_os_print(1);
	os_delay_us(1000000);

	INFO("Load Config\n");
	config_load();
	INFO("GPIO Init\n");
	gpio_init();
	INFO("MQTT Init");
	mqtt_init();

	INFO("Connect wifi %s\n", config.sta_ssid);
	WIFI_Connect(config.sta_ssid, config.sta_pwd, wifi_connect_cb);
	//WIFI_Connect("Wirelessabata", "TaLi100305", wifi_connect_cb);

	INFO("\r\nSystem started ...\r\n");
}

void ICACHE_FLASH_ATTR
ICACHE_FLASH_ATTRuser_rf_pre_init(void) {}
