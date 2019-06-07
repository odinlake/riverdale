/*
 *  MO home sensor setup for nodeMCU with: 
 *  - SCD30 co2 sensor
 *  - HCSR501 motion detector
 *  - Microphone for noise read
 */
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <Wire.h>
#include "SparkFun_SCD30_Arduino_Library.h"
#include <ArduinoJson.h>
#include <riverdale_credentials.h>


#define DEBUG false
#define ADJ_MIC_A     64.0
#define ADJ_MIC_B     25.0
#define ADJ_TEMP_C    -7.2
#define ADJ_HUMID_C   15.0

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
  startupWiFi();

  // init I2C and SCD
  Wire.begin(D2, D1);
  airSensor.begin();
  airSensor.setMeasurementInterval(5);
  airSensor.setAltitudeCompensation(30);
  airSensor.setAmbientPressure(1000);

  // init LED
  pinMode(BUILTIN_LED, OUTPUT);
  digitalWrite(BUILTIN_LED, LOW);
}


void sendDomoticz(String url)
{
  //Serial.println(String("Requesting ") + domoticz_host + ":" + domoticz_port + url);
  if (DEBUG) return;
  
  http.begin(domoticz_host, domoticz_port, url);
  int httpCode = http.GET();
  if (httpCode == 200)
  {
    String payload = http.getString();
    DeserializationError error = deserializeJson(jsonDoc, payload);
    //Serial.print("Domoticz response... "); 
    if (error) Serial.println("failed to parse.");
  } else {
    Serial.println(String("HTTP code: ") + httpCode + " " + (httpCode==200?"ok":"fail"));
  }
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


//
// read co2 at most every 5s
//
void readSCD30()
{
  static long lastTrigger = 0;
  unsigned long tnow = millis();
  unsigned long ellapsed = tnow - lastTrigger;
  
  if (ellapsed < 5000) return;
  if (airSensor.dataAvailable())
  {
    lastTrigger += ellapsed;
    uint16_t co2 = airSensor.getCO2();
    float temp = airSensor.getTemperature() + ADJ_TEMP_C;
    float humidity = airSensor.getHumidity() + ADJ_HUMID_C;
    
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
  const double attenuation = 0.85;
  double volts = 0;
  double dbs = 0;
  
  volts = sampleSoundVolts(sampleWindow);
  dbs = ADJ_MIC_A +  ADJ_MIC_B * log(volts); // determined experimentally using iPhone
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


//
// Read often. Records in how many of the last 60 1s intervals there was any motion.
// Reports number at most every 1s.
//
void readHCSR501(bool doLog=true)
{
  static int secCount = 0;
  static int motionCount = 0;
  static long lastTrigger = 0;
  unsigned long tnow = millis();
  unsigned long ellapsed = tnow - lastTrigger;
  int motion = digitalRead(hcscr501pin);
  
  if (motion < 0 || motion > 1)
  {
    Serial.print("ERROR: digitalRead returned ");
    Serial.println(motion);
    return;
  }
  secCount += motion;
  if (ellapsed > 1000)
  {
    lastTrigger += ellapsed;
    motion = (secCount != 0) ? 1 : 0;
    secCount = 0;
    motionCount += motion * 2 - 1;
    if (motionCount > 60) motionCount = 60;
    if (motionCount < 0) motionCount = 0;
    if (doLog)
    {
      Serial.print("Motion counter: ");
      Serial.println(motionCount);
      sendDomoticz(String(domoticz_url_motion) + motionCount);
    }
  }
}


void loop() 
{
  static int counter = 0;
  counter = (counter + 1) % 1000;
  readSCD30();
  readMic(counter % 100 == 50); // takes about 50 ms
  readHCSR501();
  ArduinoOTA.handle();
  delay(10);
}
