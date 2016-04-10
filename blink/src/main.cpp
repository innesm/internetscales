#include <Arduino.h>
#include "../../scales/src/WifiConfig.h"

void setup()
{
  pinMode(BUILTIN_LED, OUTPUT);

  Serial.begin(74880);
  Serial.println("=================================================");
  Serial.println("setup:");
  Serial.println(ESP.getResetReason());
  Serial.println(ESP.getResetInfo());
  Serial.println("=================================================");
}

void loop()
{
  Serial.println("-------------------------------------------------");
  Serial.println("loop:");
  for (int i = 0; i < 10; i++)
  {
    Serial.print("Sleep in ");
    Serial.print(10-i);
    Serial.println(" seconds.");
    digitalWrite(BUILTIN_LED, 1);
    delay(900);
    digitalWrite(BUILTIN_LED, 0);
    delay(100);
  }
  digitalWrite(BUILTIN_LED, 1);
  Serial.println("Sleep for 10 seconds.");
  ESP.deepSleep(10000000, WAKE_RF_DEFAULT);//WAKE_RF_DEFAULT);// WAKE_RF_DISABLED);
}
