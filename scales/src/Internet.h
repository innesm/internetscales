
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
