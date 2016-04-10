#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>

#include "WifiConfig.h"
#include "Internet.h"
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

void setup()
{
  pinMode(BUILTIN_LED, OUTPUT);
  init_mcp3008_spi();
  Serial.begin(74880);
  Serial.println(ESP.getResetReason());
  Serial.println(ESP.getResetInfo());
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
  // read backplane 0 'clock'
  int BP0 = read_analog(0);

  if (cd.done())
  {
    if (currstate == state::idle)
    {
      Serial.print("idle ");
      Serial.println(BP0);
    }
    else
    {
      Serial.print(".");
    }
    led = !led;
    digitalWrite(BUILTIN_LED, led);
  }

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
      else if (BP0 < VOLTAGE_0_TO_1)
      {
        // check if zero for longer than a tick, meaning properly idle.
        // if so, sleep for a second.
        delay(2);
        int BP0 = read_analog(0);
        if (BP0 < VOLTAGE_0_TO_1)
        {
          Serial.println("back to sleep.");
          ESP.deepSleep(60000000, WAKE_RF_DEFAULT);//WAKE_RF_DISABLED); // a second (500,000 microseconds)
        }
      }
      else
      {
        delayMicroseconds(200);
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
          Serial.print("Weight reading: ");
          Serial.print(weight_grams);
          Serial.println("g");

          digitalWrite(LED_BUILTIN, 1);
          // we captured a weight value.
          screen.try_get_weight_grams(weight_grams);
          //connect to wifi and upload
          post_to_thingspeak(weight_grams);
          //wait for scale to settle down
          Serial.println("back to sleep after logging a weight reading.");
          ESP.deepSleep(15000000, WAKE_RF_DEFAULT);//WAKE_RF_DISABLED); // a second (500,000 microseconds)

        }
        else
        {
          if (screen.is_current_weight() || screen.is_previous_weight())
          {
            Serial.println("Weight done, but not in kgs.");
          }
        }
      }

      // done capturing a frame
      currstate = state::idle;
      cd.setTick(2000);
  }
}
