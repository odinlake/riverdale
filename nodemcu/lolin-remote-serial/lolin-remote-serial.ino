/*
 *  MO home sensor setup for Lolin ESP8266 with: 
 *  - stock IR shield
 */
#include <IRsend.h>
#include <stdarg.h>
#include <riverdale_credentials.h>

#define MARANTZ_POWER_1     0x2CD552D
#define MARANTZ_POWER_2     0x2AD552D
#define MARANTZ_VOL_UP_1    0x2CD54D5
#define MARANTZ_VOL_UP_2    0x2AD54D5
#define MARANTZ_VOL_DOWN_1  0x59AA9A9
#define MARANTZ_VOL_DOWN_2  0x55AA9A9

#define AMP_ONOFF_PIN       D7

IRsend irSend(D3);
char szBuffer[100] = "";
int nBuffer = 0;
void customWait(unsigned long ms) { delay(ms); }


// initial section
void setup() 
{
  Serial.begin(115200);
  delay(100);
  startupWiFi();

  pinMode(AMP_ONOFF_PIN, INPUT);
  irSend.begin();
}


// main loop
void loop() 
{
  static unsigned long lastTrigger = 0;
  static unsigned int lastAmplifierStatus = 0;
  unsigned long curTime = millis();
  unsigned long ellapsed = curTime - lastTrigger;
  int amplifierStatus = digitalRead(AMP_ONOFF_PIN);

  if (Serial.available() > 0) {
    char cIn = Serial.read();
    if (cIn == '\n') {
      handleCommand(szBuffer);
      nBuffer = 0;
    } else if (nBuffer < 99) {
      szBuffer[nBuffer++] = cIn;
    }
    szBuffer[nBuffer] = 0;
  }

  if (ellapsed > 10000 || lastAmplifierStatus != amplifierStatus) {
    lastTrigger += ellapsed;
    lastAmplifierStatus = amplifierStatus;
    Serial.print(":status=");
    Serial.println(amplifierStatus);
  }
  
  ArduinoOTA.handle();
}


// carry out orders as and when given
void handleCommand(char *szCmd)
{
  if (strcmp(szCmd, ":amp-on")) irSwitchAmplifier(1);
  else if (strcmp(szCmd, ":amp-off")) irSwitchAmplifier(0);
  else {
    Serial.print("Heard: ");
    Serial.println(szBuffer);
  }
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IR Stuff begins here
////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// send custom recorded data with given interval length (usec)
void irSendCustom(unsigned int code, unsigned int interval = 1000)
{
  static uint16_t rawData[128];
  unsigned int mask = 1 << 31;
  int dpos = 0;
  
  Serial.print("  sending ");
  Serial.print(code, HEX);
  Serial.print(" :: ");
  Serial.print(code, BIN);
  Serial.println("...");
  
  rawData[dpos] = 0;
  while (!(mask & code) && mask) mask = mask >> 1;
  while (mask) {
    if ((mask & code) && (dpos & 1)) { dpos += 1; rawData[dpos] = 0; }
    if (!(mask & code) && !(dpos & 1)) { dpos += 1; rawData[dpos] = 0; }
    rawData[dpos] += interval;
    mask = mask >> 1;
  }
  irSend.sendRaw(rawData, dpos+1, 38);
}


// keep calling this function until it returns true
bool irSwitchAmplifier(int unto)
{
  static unsigned int counter = 0;
  static unsigned int codes[] = { MARANTZ_POWER_1, MARANTZ_POWER_2 };
  counter = (counter + 1) % 1000;
  int amplifierStatus = digitalRead(AMP_ONOFF_PIN);
  if (amplifierStatus == unto) return true;
  irSendCustom(codes[counter%2]);
  return false;
}


bool irAmplifierVolume(int amount)
{
  unsigned int codes[] = { MARANTZ_VOL_UP_1, MARANTZ_VOL_UP_2, MARANTZ_VOL_DOWN_1, MARANTZ_VOL_DOWN_2 };
  int offset = (amount < 0) ? 2 : 0;
  int n = (amount < 0) ? -amount : amount;
  for (int i=0; i<10; i++) {
    if (irSwitchAmplifier(1)) break;
    customWait(500);
  }
  if (!irSwitchAmplifier(1)) return false;
  for (int i=0; i<n; i++) {
    irSendCustom(codes[offset + (i%2)]);
    customWait(100);
  }
  return true;
}
