#include "credentials.h"
#include <string.h>

#define MARANTZ_INTERVAL_US 1000
#define MARANTZ_VOL_UP_1    0x2CD54D5
#define MARANTZ_VOL_UP_2    0x2AD54D5
#define MARANTZ_VOL_DOWN_1  0x59AA9A9
#define MARANTZ_VOL_DOWN_2  0x55AA9A9
#define MARANTZ_POWER_1     0x2CD552D
#define MARANTZ_POWER_2     0x2AD552D
#define MARANTZ_MUTE_1     0x55aaa59
//#define MARANTZ_POWER_4     0x55aaa56


unsigned int OUTPUT_PIN = D5;
unsigned int transCode = 0;
unsigned int transMask = 0;
int transPhase = -1;
unsigned long transTime = 0;
WiFiServer wifiServer(80);


void setup() 
{
  Serial.begin(115200);
  for (int i=0; i<2; i++) { Serial.print("."); delay(500); }
  startupWiFi();
  Serial.println("Hello World.");
  //pinMode(OUTPUT_PIN, OUTPUT);
  digitalWrite(BUILTIN_LED, LOW);

  pinMode(D6, OUTPUT);
  digitalWrite(D6, HIGH);
  
  wifiServer.begin();
}

void sendIR(unsigned int code)
{
  pinMode(OUTPUT_PIN, OUTPUT);
  digitalWrite(OUTPUT_PIN, LOW);
  transPhase = 0;
  transCode = code << 2;
  transMask = 1 << 31;
  transTime = micros();
  Serial.print("SENDING: ");
  Serial.print(code, HEX);
  Serial.print("  ");
}

void stopSendIR()
{
  pinMode(OUTPUT_PIN, INPUT);
  transPhase = 0;
  transCode = 0;
  transMask = 0;
  transTime = micros();
  Serial.println("");
}

bool sendCycleIR()
{
  if (transMask == 0) return false;
  unsigned long ellapsed = micros() - transTime;
  if (ellapsed < MARANTZ_INTERVAL_US) return true;
  transPhase += 1;
  transTime = micros();
  if (transPhase < 3) return true;
  int sig = ((transCode & transMask) == 0) ? LOW : HIGH;
  digitalWrite(OUTPUT_PIN, sig);
  Serial.print((sig == 0) ? "_" : "1");
  transMask >>= 1;
  if (transMask == 0) stopSendIR();
  return true;
}

bool tcpExec(const std::string &cmd) 
{
  Serial.print("\nTCP command: ");
  Serial.print(cmd.c_str());
  Serial.print(" ");
  if (cmd == "on") sendIR(MARANTZ_POWER_1);
  else if (cmd == "off") sendIR(MARANTZ_POWER_2);
  else if (cmd == "up") sendIR(MARANTZ_VOL_UP_1);
  else if (cmd == "down") sendIR(MARANTZ_VOL_DOWN_1);
  else {
    Serial.println("...unknown!");
  }
}

void loop() 
{
  static unsigned long counter = 0;
  static const int tcp_line_len = 10;
  static char tcp_buffer[tcp_line_len+1];
  int n = 0;
  
  counter += 1;
  sendCycleIR();
  WiFiClient client = wifiServer.available();
   
  if (client) {
    Serial.println("TCP Client connected.");
    if (!client.connected())
      Serial.println("TCP Client missing!");
    while (client.connected()) {
      while (client.available()>0) {
        char c = client.read();
        Serial.print(c);
        if (c == '\n') {
          tcpExec(tcp_buffer);
          n = 0;
        } 
        else if ('a' <= c && c <= 'z' && n < tcp_line_len) {
          tcp_buffer[n++] = c;
        }
        tcp_buffer[n] = 0;
      }
      while (sendCycleIR()) delayMicroseconds(100);
      delay(10);
    }
    Serial.println("TCP Client gone.");
    if (n > 0) tcpExec(tcp_buffer);
  }
}
