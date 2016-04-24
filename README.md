# internetscales
Project to put some bathroom scales on the internet using an ESP8266

The idea here is that the MCU, an ESP8266, will use an ADC (MCP3008) to read the control lines of the LCD display of a set of bathroom scales, and upload weight readings to the internet.
The ESP8266 should be normally unpowered.
It gets woken up when stuff starts to happen on the control lines, then detects the weight reading by decoding what's displayed on the LCD.
Then, it uploads this to some cloud IoT thing and switches itself back off to save power.

