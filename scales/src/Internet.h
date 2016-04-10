#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>

void wifi_connect()
{
  Serial.println("Connect to wifi");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(1000);
  }
  Serial.println(WiFi.localIP());
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
