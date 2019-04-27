/*
 *  MO home sensor setup for nodeMCU with: 
 *  - SCD30 co2 sensor
 */
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <Wire.h>
#include "SparkFun_SCD30_Arduino_Library.h"
#include <ArduinoJson.h>

#define DEBUG false


// WiFi
const char* ssid     = "Valhalla";
const char* password = "cloudypanda668";

// Domoticz
const char* domoticz_host = "192.168.1.15";
uint16_t domoticz_port = 8081;
const char* domoticz_url_co2 = "/json.htm?type=command&param=udevice&idx=44&nvalue=";
const char* domoticz_url_temphum = "/json.htm?type=command&param=udevice&idx=45&nvalue=0&svalue=";
const char* domoticz_url_sound = "/json.htm?type=command&param=udevice&idx=46&nvalue=0&svalue=";
const char* domoticz_url_motion = "/json.htm?type=command&param=udevice&idx=47&nvalue=0&svalue=";

// Helpers
SCD30 airSensor;
HTTPClient http;
const size_t capacity = JSON_ARRAY_SIZE(2) + JSON_OBJECT_SIZE(3) + 40;
DynamicJsonDocument jsonDoc(capacity);
const int hcscr501pin = D5;


void setup() 
{
  Serial.begin(115200);
  delay(100);
  initWiFi();

  // init I2C and SCD
  Wire.begin(D2, D1);
  airSensor.begin(); //This will cause readings to occur every two seconds

  // init LED
  pinMode(BUILTIN_LED, OUTPUT);
  digitalWrite(BUILTIN_LED, LOW);
}


void initWiFi()
{
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");  
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.print("Netmask: ");
  Serial.println(WiFi.subnetMask());
  Serial.print("Gateway: ");
  Serial.println(WiFi.gatewayIP());  
}


void sendDomoticz(String url)
{
  Serial.println(String("Requesting ") + domoticz_host + ":" + domoticz_port + url);
  if (DEBUG) return;
  
  http.begin(domoticz_host, domoticz_port, url);
  int httpCode = http.GET();
  if (httpCode == 200)
  {
    String payload = http.getString();
    DeserializationError error = deserializeJson(jsonDoc, payload);
    Serial.print("Domoticz response... "); 
    if (error) Serial.println("failed to parse.");
    else 
    {
      Serial.print((const char *)jsonDoc["title"]);
      Serial.print(" :: ");
      Serial.println((const char *)jsonDoc["status"]);
    }
  }
  Serial.println(String("HTTP code: ") + httpCode + " " + (httpCode==200?"ok":"fail"));
  http.end();
}


//
// spends "window" milliseconds measuring volume
//
double sampleSoundVolts(unsigned long window)
{
  unsigned long startMillis = millis();
  unsigned int signalMax = 0;
  unsigned int signalMin = 1024;
  unsigned int sample;
  
  while (millis() - startMillis < window)
  {
    sample = analogRead(0); 
    if (sample < 1024)  // toss out spurious readings
    {
      if (sample > signalMax)
      {
        signalMax = sample;  // save just the max levels
      }
      else if (sample < signalMin)
      {
        signalMin = sample;  // save just the min levels
      }
    }
  }
  unsigned int peakToPeak = signalMax - signalMin;
  double volts = (peakToPeak * 5.0) / 1024;
  return volts;
}


void readSCD30()
{
  if (airSensor.dataAvailable())
  {
    uint16_t co2 = airSensor.getCO2();
    float temp = airSensor.getTemperature();
    float humidity = airSensor.getHumidity();
    
    Serial.print("co2(ppm):");
    Serial.print(co2);
    Serial.print(" temp(C):");
    Serial.print(temp, 1);
    Serial.print(" humidity(%):");
    Serial.print(humidity, 1);
    Serial.println();

    sendDomoticz(String(domoticz_url_co2) + co2);
    sendDomoticz(String(domoticz_url_temphum) + temp + ";" + humidity);
  } 
}


//
// will spend about 50 ms (sampleWindow) measuring sound volume
//
void readMic(bool doLog)
{
  static double pastDbs = 0;
  const unsigned int sampleWindow = 50; // ms
  const double attenuation = 0.95;
  double volts = 0;
  double dbs = 0;
  
  volts = sampleSoundVolts(sampleWindow);
  dbs = 75 + 20 * log(volts); // determined experimentally using iPhone
  pastDbs = attenuation * pastDbs + (1.0 - attenuation) * dbs;

  if (doLog)
  {
    Serial.print("ADC Amp: ");
    Serial.print(dbs);
    Serial.print(" dB ... ");
    Serial.print(volts);
    Serial.println(" V");
    sendDomoticz(String(domoticz_url_sound) + pastDbs);
  }
}


void readHCSR501()
{
  static int motionCount = 0;
  int motion = digitalRead(hcscr501pin);
  if (motion < 0 || motion > 1)
  {
    Serial.print("ERROR: digitalRead returned ");
    Serial.println(motion);
    return;
  }
  motionCount += motion * 2 - 1;
  if (motionCount > 10) motionCount = 10;
  if (motionCount < 0) motionCount = 0;
  
  Serial.print("Motion counter: ");
  Serial.println(motionCount);
  sendDomoticz(String(domoticz_url_motion) + motionCount);  
}


// each loop takes about 150 ms
void loop() 
{
  static int counter = 0;
  counter = (counter + 1) % 1000;
  if (counter % 20 == 0) readSCD30();
  if (counter % 1 == 0) readMic(counter % 20 == 5); // takes about 50 ms
  if (counter % 20 == 10) readHCSR501();
  delay(100);
}
