#include <Arduino.h>

void setup()
{
  pinMode(BUILTIN_LED, OUTPUT);
  Serial.begin(19200);
}

void loop()
{
  Serial.println("hiya");
  digitalWrite(BUILTIN_LED, 0);
  delay(200);
  digitalWrite(BUILTIN_LED, 1);
  delay(500);
}
