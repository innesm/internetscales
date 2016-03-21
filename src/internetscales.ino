#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <RTClib.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>

#include "ScreenPoint.h"
#include "WeightDisplay.h"
#include "Bitmap.h"

// I2C devices
Adafruit_SSD1306 display;
RTC_DS1307 rtc;
const int SDA_PIN = 5;
const int SCL_PIN = 4;

ScreenPoint getCursorPos()
{
  ScreenPoint p;
  p.x = display.getCursorX();
  p.y = display.getCursorY();
  return p;
}

void init_i2c()
{
  Wire.begin(SDA_PIN, SCL_PIN);
}

//String get_rtc_date_time()
//{
//  if (rtc.isrunning())
//  {
//    DateTime d = rtc.now();
//
//    String dt("");
//    dt += d.day() + "/" + d.month() + "/" + d.year();
//
//    display.print(d.hour());
//    display.print(":");
//    int m = d.minute();
//    if (m < 10)
//      display.print("0");
//    display.print(m);
//
//    display.print(":");
//    int s = d.second();
//    if (s < 10)
//      display.print("0");
//    display.print(s);
//    display.println(" ");
//  }
//  else
//  {
//    display.println("RTC not running.");
//  }
//}

WeightDisplay screen;

void introDisplay()
{
  display.setTextSize(2);
  display.setTextWrap(true);

  display.setTextColor(WHITE, BLACK);
  display.println("hello");
  display.setTextSize(1);
  display.print(__DATE__);
  display.print(" ");
  display.println(__TIME__);
  String reset_reason = ESP.getResetReason();
  display.print("RST:");
  display.println(reset_reason);

  if (reset_reason == "Exception")
  {
    display.setTextSize(2);
    display.println("[press reset]");
    display.display();
    while(1){delay(1000);}
  }
  display.display();
}

void beginDisplay()
{
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  introDisplay();
}

uint8_t reversebits(uint8_t b)
{
  uint8_t r = 0;
  for (int idx = 0; idx < 8; idx++)
  if (b & bit(idx))
    r |= bit(7 - idx);
  return r;
}

void init_spi()
{
  SPI.setHwCs(true);
  SPI.begin();
  SPI.beginTransaction(SPISettings(SPI_CLOCK_DIV8, MSBFIRST, SPI_MODE0));
}

// Read channel from MCP3008 using SPI
int read_analog(int channel)
{
  uint8_t send_bytes[] = {0xC0 + (channel << 3), 0, 0, 0};
  uint8_t recv_bytes[4];
  SPI.transferBytes(send_bytes, recv_bytes, 4);
  uint8_t b3 = reversebits(recv_bytes[3]);
  uint8_t b2 = reversebits(recv_bytes[2]);
  return (b3 << 8) + b2;
}

///////////////////////////////
// Simple timer helper
///////////////////////////////
class Countdown
{
  int starttime;
  int tick;

 public:
  Countdown(int d) : starttime(millis()), tick(d)
  {
  }

  void setTick(int newTick)
  {
    tick = newTick;
  }

  bool done()
  {
    int elapsed = millis() - starttime;
    if (elapsed > tick)
    {
      starttime = millis();
      return true;
    }
    return false;
  }
};

int VOLTAGE_0_TO_1 = 200;  //between bias level 0 and 1
int VOLTAGE_2_TO_3 = 800; // between bias level 2 and 3

void readSegments(int backplane)
{
  for (int x = 0; x < SEGMENT_COUNT; ++x)
    screen.pixel(x, backplane) = read_analog(x + 1) > VOLTAGE_2_TO_3;
}

void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color)
{
  for (int row = y; row < y + h; ++row)
    display.drawFastHLine(x, row, w, color);
}

void hatchRect(int16_t x, int16_t y, int16_t w, int16_t h)
{
  for (int row = y; row < y + h; ++row)
    for (int col = x; col < x + w; ++col)
      display.drawPixel(col, row, ((col + row) % 2 == 0) ? BLACK : WHITE);
}

void drawPixel(int16_t x, int16_t y, int16_t w, int16_t h, bool set)
{
  if (set)
  {
    hatchRect(x, y, w-1, h-1);
    display.drawFastHLine(x+1, y+h-1, w-1, BLACK);
    display.drawFastVLine(x+w-1, y+1, h-1, BLACK);
  }
  else
  {
    fillRect(x, y, w, h, WHITE);
  }
}

void printSegmentReadings()
{
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0,0);

  int w = 5;
  int h = 6;
  int t = 2;

  fillRect(0, 0, 128, 4 * (h+1) + 4, WHITE);

  for (int y = 0; y < 4; ++y)
  {
    int l = 2;
    for (int x = 0; x < SEGMENT_COUNT; ++x)
    {
      if (screen.pixel(x, y))
        drawPixel(l, t + y * (h+1), w, h, true);
      l += w+1;
      if (x % 2 == 1)//digits spacing
        l+= 2;
    }
  }

  display.setCursor(0, 4 * (h+1) + 4 + 2);
  display.display();
}

// helper for flashing an LED at different rates
Countdown cd(2000);

// info led state
bool led;

// tick time in micros tweaked to take into account the time
// taken to sample the segment lines.
const int ticktime = 1900;

void cls()
{
  display.setCursor(0,0);
  display.clearDisplay();
}

void wifi_connect()
{
  display.print("WIFI");
  display.display();

  WiFi.begin("SKY8AC1E", "TDSCXVQU");

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(1000);
    display.print(".");
    display.display();
  }
  display.println(WiFi.localIP());
  display.display();
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

  display.println(httpCode);
  display.display();

  display.println(http.getString());
  display.display();

  WiFi.disconnect(true);
}

void setup()
{
  pinMode(BUILTIN_LED, OUTPUT);

  // pin used for SPI hardware chip select
  pinMode(15, OUTPUT);

  init_i2c();

  beginDisplay();

  init_spi();
}

enum class state
{
  idle,
  waitingedge,
  capturing
};

state currstate = state::idle;

void loop()
{
  if (cd.done())
  {
    led = !led;
    digitalWrite(LED_BUILTIN, led);
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
        cd.setTick(500);

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
          cls();
          screen.try_get_weight_grams(weight_grams);
          display.print(weight_grams);
          display.println("g");
          display.display();

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
