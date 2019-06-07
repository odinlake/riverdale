#include "credentials.h"
#include <IRsend.h>

#define MY_DEBUG
#define MY_BAUD_RATE 9600
#define MY_GATEWAY_ESP8266
#define MY_WIFI_SSID WIFI_SSID
#define MY_WIFI_PASSWORD WIFI_PASSWORD
#define MY_PORT 80
#define MY_GATEWAY_MAX_CLIENTS 2
#define MY_HOSTNAME "ESP_54E096_IR"

// If using static ip you can define Gateway and Subnet address as well
//#define MY_IP_GATEWAY_ADDRESS 192,168,1,19
//#define MY_IP_SUBNET_ADDRESS 255,255,255,0

/*
#define MY_GATEWAY_MQTT_CLIENT
#define MY_MQTT_PUBLISH_TOPIC_PREFIX "domoticz/out/MyMQTT"
#define MY_MQTT_SUBSCRIBE_TOPIC_PREFIX "domoticz/in/MyMQTT"
#define MY_MQTT_CLIENT_ID "TheAmplifier"
#define MY_MQTT_USER "admin"
#define MY_MQTT_PASSWORD "admin"

#define MY_CONTROLLER_IP_ADDRESS 192, 168, 1, 15
*/

#include <MySensors.h>
#include <ESP8266WiFi.h>

#define MARANTZ_POWER_1     0x2CD552D
#define MARANTZ_POWER_2     0x2AD552D
#define MARANTZ_VOL_UP_1    0x2CD54D5
#define MARANTZ_VOL_UP_2    0x2AD54D5
#define MARANTZ_VOL_DOWN_1  0x59AA9A9
#define MARANTZ_VOL_DOWN_2  0x55AA9A9

#define MYID_AMPONOFF       1
#define AMP_ONOFF_PIN       D7


IRsend irSend(D3);
MyMessage msgOnOff(MYID_AMPONOFF, V_STATUS);


void setup() 
{
  Serial.println("Hello World.");
  pinMode(AMP_ONOFF_PIN, INPUT);
  irSend.begin();
}


void presentation()  
{ 
  sendSketchInfo("Amplifier Control Node", "1.0");
  present(MYID_AMPONOFF, S_BINARY, "Amplifier On/Off Switch", true);
  //present(CHILD_ID, S_IR, "IR Controller for Amplifier");
}


void receive(const MyMessage &message)
{
  Serial.println("Got Message.");
  Serial.println(message.getString());

  if (message.type==V_STATUS && message.sensor==MYID_AMPONOFF) {
    Serial.print("Incoming change for sensor:");
    Serial.print(message.sensor);
    Serial.print(", New status: ");
    Serial.println(message.getBool());
    Serial.println(switch_amplifier(message.getBool()?1:0) ? "ok" : "fail");
    sendState();
  }
}


// send amplifier state to mysensor
void sendState()
{
    int amplifierStatus = digitalRead(AMP_ONOFF_PIN);
    Serial.print("Amplifier is: ");
    Serial.println((amplifierStatus==HIGH)?"on":"off");
    send(msgOnOff.set(amplifierStatus==HIGH), false);
}


void loop() 
{
  static long lastTrigger = 0;
  unsigned long ellapsed = millis() - lastTrigger;
  if (ellapsed > 10000)
  {
    lastTrigger += ellapsed;
    sendState();
  }
}



/////////////////////////
// IR Stuff begins here
/////////////////////////

// send custom recorded data with given interval length (usec)
void send_custom(unsigned int code, unsigned int interval = 1000)
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


bool switch_amplifier(int unto)
{
  unsigned int codes[] = { MARANTZ_POWER_1, MARANTZ_POWER_2 };
  for (int i=0; i<10; i++) {
    if (i > 0) wait(1000);
    int amplifierStatus = digitalRead(AMP_ONOFF_PIN);
    if (amplifierStatus == unto) return true;
    send_custom(codes[i%2]);
  }
  return false;
}


bool amplifier_volume(int amount)
{
  unsigned int codes[] = { MARANTZ_VOL_UP_1, MARANTZ_VOL_UP_2, MARANTZ_VOL_DOWN_1, MARANTZ_VOL_DOWN_2 };
  int offset = (amount < 0) ? 2 : 0;
  int n = (amount < 0) ? -amount : amount;
  if (!switch_amplifier(1)) return false;
  for (int i=0; i<n; i++) {
    send_custom(codes[offset + (i%2)]);
    wait(100);
  }
  return true;
}
