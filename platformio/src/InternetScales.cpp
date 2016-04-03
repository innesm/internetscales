#include <Arduino.h>
#include <SPI.h>

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>

#include "WeightDisplay.h"
#include "Bitmap.h"
#include "Countdown.h"
#include "mcp3008.h"

WeightDisplay screen;

int VOLTAGE_0_TO_1 = 200;  //between bias level 0 and 1
int VOLTAGE_2_TO_3 = 800; // between bias level 2 and 3

void readSegments(int backplane)
{
  for (int x = 0; x < SEGMENT_COUNT; ++x)
    screen.pixel(x, backplane) = read_analog(x + 1) > VOLTAGE_2_TO_3;
}

// tick time in micros tweaked to take into account the time
// taken to sample the segment lines.
const int ticktime = 1900;

void wifi_connect()
{
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(1000);
  }
}

void post_to_thingspeak(int weight_grams)
{
  wifi_connect();

  HTTPClient http;

  //upload a thingspeak channel event
  String url = "http://184.106.153.149/update?key=CCR1U59IU6AAKTGD&field1=";
  url += weight_grams / 1000.0F;

  http.begin(url);//google.com
  int httpCode = http.GET();

  WiFi.disconnect(true);
}

void setup()
{
  pinMode(BUILTIN_LED, OUTPUT);
  init_mcp3008_spi();
}

enum class state
{
  idle,
  waitingedge,
  capturing
};
state currstate = state::idle;

// helper for flashing an LED at different rates
Countdown cd(2000);
bool led;

void loop()
{

  if (cd.done())
  {
    led = !led;
    digitalWrite(BUILTIN_LED, led);
  }

  // read backplane 0 'clock'
  int BP0 = read_analog(0);

  switch(currstate)
  {
    case state::idle:

      if (BP0 > VOLTAGE_2_TO_3) // find fall to 0v
      {
        // start sampling frequently to catch the edge falling from 3 to 0 volts
        currstate = state::waitingedge;

        //flash info led more quickly
        cd.setTick(250);

        delayMicroseconds(50);
      }
      else
      {
        delayMicroseconds(200);
        break;
      }

    case state::waitingedge:

      // find fall to zero, and start capturing a frame
      if (BP0 < VOLTAGE_0_TO_1)
      {
        currstate = state::capturing;
      }
      else
        delayMicroseconds(50);
      break;

    case state::capturing:

      // at frame start. begin capturing segment values.
      // read segments at each of these timepoints:
      // take readings in 3v peaks of segment signals (bp low)

      // delay to centre of tick
      delayMicroseconds(0.4 * ticktime);

      // sample segment lines during each backplane
      for (int bp = 0; bp < 4; ++bp)
      {
        readSegments(bp);

        if (bp < 3)
          delayMicroseconds(2 * ticktime);
      }

      // detect when display is indicating a "current weight"
      if (screen.is_current_weight())
      {
        // is display value valid?
        int weight_grams;
        if (screen.try_get_weight_grams(weight_grams))
        {
          digitalWrite(LED_BUILTIN, 1);
          // we captured a weight value.
          screen.try_get_weight_grams(weight_grams);
          //connect to wifi and upload
          post_to_thingspeak(weight_grams);
          //wait for scale to settle down
          delay(10000);
        }
      }

      // done capturing a frame
      currstate = state::idle;
      cd.setTick(2000);
  }
}
