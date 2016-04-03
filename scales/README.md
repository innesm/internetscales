# internetscales
Project to put some bathroom scales on the internet using an ESP8266

```
                                       +---------------------+
 +-----------+(B)    +-----------+ (A) |     +---------+     |
 |ESP8266    +-------+MCP3008    +-----+     |         |     |
 |mcu        |       |adc        |     |     +---------+     |
 +-----------+       +-----------+     |                     |
      |                                |    bathroom scale   |
      |                                |                     |
 +------------+                        |                     |
 |            |                        |                     |
 | The        |                        |                     |
 | Internet   |                        +---------------------+
 |            |
 |            |
 +------------+

(A) - read LCD display backplane and segment driver pins
(B) - decode LCD display to weight value.
      detect final weight reading.
      upload weight reading to the internet.
      
```
