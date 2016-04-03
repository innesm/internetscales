#include <Arduino.h>

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
