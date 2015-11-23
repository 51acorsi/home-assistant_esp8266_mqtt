**ESP MQTT Light with Home-Assistant**
==========
This project aims to use ESP8266 modules to automate light and switches in a home automation project using the MQTT protocol and the [Home Assistant](https://home-assistant.io/) platform.

### Output
The board has 3 individual 220v relays that are controlled both by MQTT commands and direct switch command.

### Input
There are 3 individual inputs that accepts a 220v entry. Each of the entries toggles the correponding relays.

### Usage Examples
Light:
* Connect a Light in the relay 1 load
* MQTT command will turn on/off the relay/light
* Light switch connected to Input 1 will toggle the light
