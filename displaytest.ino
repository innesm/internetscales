#include <Wire.h>

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>

#include <Adafruit_MQTT.h>
#include <Adafruit_MQTT_Client.h>

Adafruit_SSD1306 display;

//////////////////////////////////////////////////
// Generic pixel display
//////////////////////////////////////////////////
template<typename TPixel, size_t W, size_t H>
class Bitmap
{
  TPixel _pixels[W*H];
  
public:
  Bitmap()
  {
  }

  TPixel& pixel(size_t x, size_t y)
  {
    return _pixels[y * W + x];    
  } 
};

const int SEGMENT_COUNT = 7;
int VOLTAGE_0_TO_1 = 200;  //between bias level 0 and 1
int VOLTAGE_2_TO_3 = 800; // between bias level 2 and 3

////////////////////////////////////////////////
// Bathroom scales lcd display
////////////////////////////////////////////////
class WeightDisplay : public Bitmap<bool, SEGMENT_COUNT, 4>
{
  //7x4 'pixels'
  //cols:
  // 0-1 = tens digit
  // 2-3 = units digit (and decimal pt)
  // 4-5 = tenths digit
  // 6 = icons
  //  row 0 = KG
  //  row 1 = Lbs?
  //  row 2 = "current wt"
  //  row 3 = "previous wt"
  
public:

  int decode_digit(int digit, bool blank_is_zero=false)
  {
    // each digit is displayed in two adjacent columns
    int s0 = digit * 2;
    int s1 = digit * 2 + 1;
    
    return decode_7_segment(blank_is_zero,
      (pixel(s0,3) << 6) + 
      (pixel(s0,0) << 5) + 
      (pixel(s1,1) << 4) + 
      (pixel(s0,1) << 3) + 
      (pixel(s0,2) << 2) + 
      (pixel(s1,2) << 1) + 
      pixel(s1,0));
  }

//  String to_string()
//  {
//    String s;
//    s+= decode_digit(0);
//    s+= decode_digit(1);
//    s+= pixel(3, 3) ? "." : " ";
//    s+= decode_digit(2);
//    return s;
//  }

  bool is_current_weight()
  {
    return pixel(6, 2);
  }

  bool is_previous_weight()
  {
    return pixel(6, 3);
  }

  bool is_kilograms()
  {
    return pixel(6, 0);
  }

  bool try_get_weight_grams(int& grams)
  {
    grams = -1;

    //todo convert other units to grams
    if (!is_kilograms())
      return false;
      
    int tens = decode_digit(0, true);
    if (tens < 0)
      return false;
    grams = 10000 * tens;
    
    int units = decode_digit(1);
    if (units < 0)
      return false;
    grams += 1000 * units;
      
    int tenths = decode_digit(2);    
    if (tenths < 0)
      return false;
    grams += 100 * tenths;
    
    return true;
  }

  static int decode_7_segment(bool blank_is_zero, uint8_t bits)
  {
      if (bits == 0) return blank_is_zero ? 0 : -1;
      if (bits == 0b1110111) return 0;
      if (bits == 0b0010010) return 1;
      if (bits == 0b1011101) return 2;
      if (bits == 0b1011011) return 3;
      if (bits == 0b0111010) return 4;
      if (bits == 0b1101011) return 5;
      if (bits == 0b1101111) return 6;
      if (bits == 0b0010011) return 7;
      if (bits == 0b1111111) return 8;
      if (bits == 0b1111011) return 9;
      return -2;
  }
};

struct Pt
{
  int x, y;
};

Pt getCursorPos()
{
  Pt p;
  p.x = display.getCursorX();
  p.y = display.getCursorY();
  return p;
}

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

enum class state
{
  idle,
  waitingedge,
  capturing
};

state currstate = state::idle;

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

bool mqtt_connect(Adafruit_MQTT_Client& mqtt)
{
  display.print("MQTT");
  display.display();
  delay(1000);
  display.print(".");
  display.display();
  
  int retries = 3;
  while (!mqtt.connected() && retries-- > 0)
  {
    int8_t ret = mqtt.connect();
    if (ret != 0)
    {
      display.println();
      display.print("error=");
      display.display();
      delay(5000);
      display.print(mqtt.connectErrorString(ret));
      display.display();
      delay(5000);
    }
  }
  
  if (mqtt.connected())
  {
    display.println("...OK");
    display.display();
    return true;
  }
  else
  {
    display.println("...FAILED");
    display.display();
    return false;
  }
}

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

const char* AIO_SERVER     = "io.adafruit.com";
const int   AIO_SERVERPORT = 1883;
const char* AIO_USERNAME   = "innesm";
const char* AOI_KEY        = "233488e1b7b097415fe6569ced205228e41c1d33";
const char* WEIGHT_FEED = "innesm/f/weight";

void post_to_adafruitio(int32_t weight_grams)
{
  cls();
  
  wifi_connect();
  
  // client to connect to AIO
  WiFiClient client;

  // Setup the MQTT client class by passing in the WiFi client and MQTT server and login details.
  Adafruit_MQTT_Client mqtt(&client, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AOI_KEY);

  // Create publish object
  Adafruit_MQTT_Publish weight_feed = Adafruit_MQTT_Publish(&mqtt, WEIGHT_FEED);
  
  if (mqtt_connect(mqtt))
  {
    //publish the weight value
    display.print("publish to ");
    display.print(AIO_SERVER);
    display.print(":");
    display.print(AIO_SERVERPORT);
    display.print(" ");
    display.println(WEIGHT_FEED);
    display.display();
    display.println(weight_feed.publish(weight_grams));
    display.display();
  }
  
  delay(5000);
    
  WiFi.disconnect(true);
  
  display.println("");
  display.print("wait for scale off...");
  display.display();
  
  delay(5000);
}

void post_to_thingspeak(int weight_grams)
{
  wifi_connect();
  
  HTTPClient http;

  //upload a thingspeak channel event
  String url = "http://184.106.153.149/update?key=CCR1U59IU6AAKTGD&field1=";
  url += weight_grams / 1000.0F;
//  url += "&field2=";
//  url += micros();
  
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
#ifdef ESP_03
  // set up non-standard I2C pins for ESP-03
  Wire.begin(/*SDA*/12, /*SCL*/13);
#endif

  pinMode(BUILTIN_LED, OUTPUT);

  // pin used for SPI hardware chip select
  pinMode(15, OUTPUT);

  beginDisplay();

  init_spi();
}


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

