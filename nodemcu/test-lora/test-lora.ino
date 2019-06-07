#include <U8g2lib.h>
#include <U8x8lib.h>
#include "VEML7700.h"
#include <SPI.h>
#include <LoRa.h>


U8X8_SSD1306_128X64_NONAME_SW_I2C u8x8(/* clock=*/ 15, /* data=*/ 4, /* reset=*/ 16);
VEML7700 als;


char szBuffer[50];


void setup() 
{
  Serial.begin(115200);
  u8x8.begin();
  u8x8.setFont(u8x8_font_chroma48medium8_r);
  u8x8.drawString(0, 1, "Starting LoRa...");
  delay(2000);
  als.begin();
}


void loop() 
{
  static int count = 0;
  static float lux = 0.0;
  int ret;

  if (count % 1000 == 0)
  {
    u8x8.clearDisplay();
    u8x8.drawString(0, 1, "Running!");
  }
  if (count % 5 == 0) 
  {
    als.getALSLux(lux);
    Serial.println(lux);
    ret = snprintf(szBuffer, sizeof(szBuffer), "Lux: %f", lux);
    if (ret > 0) {
      u8x8.drawString(0, 2, szBuffer);
    }
  }
  
  Serial.print(".");
  delay(100);
  count = (count + 1) % 1000;
}
