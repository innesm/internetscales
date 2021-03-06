#include <Arduino.h>
#include <SPI.h>

#define CHIPSELECT 4

uint8_t reversebits(uint8_t b)
{
  uint8_t r = 0;
  for (int idx = 0; idx < 8; idx++)
  if (b & bit(idx))
    r |= bit(7 - idx);
  return r;
}

void init_mcp3008_spi()
{
  // pin used for SPI hardware chip select
  //pinMode(15, OUTPUT);
  //digitalWrite(15, 1);

  pinMode(CHIPSELECT, OUTPUT);
  digitalWrite(CHIPSELECT, 1);
  SPI.setHwCs(false);//true);
  SPI.begin();
  SPI.beginTransaction(SPISettings(SPI_CLOCK_DIV8, MSBFIRST, SPI_MODE0));
}


// Read channel from MCP3008 using SPI
int read_analog(int channel)
{
  uint8_t send_bytes[] = {static_cast<uint8_t>(0xC0 + (channel << 3)), 0, 0, 0};
  uint8_t recv_bytes[4];
  digitalWrite(CHIPSELECT, 0);
  SPI.transferBytes(send_bytes, recv_bytes, 4);
  digitalWrite(CHIPSELECT, 1);
  uint8_t b3 = reversebits(recv_bytes[3]);
  uint8_t b2 = reversebits(recv_bytes[2]);
  return (b3 << 8) + b2;
}
