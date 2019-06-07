/*
 *  MO home sensor setup for nodeMCU with: 
 *  - SCD30 co2 sensor
 *  - Microphone for noise read
 */
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <Wire.h>
#include "SparkFun_SCD30_Arduino_Library.h"
#include <ArduinoJson.h>
#include <riverdale_credentials.h>


#define DEBUG false
#define ADJ_MIC_A     75.0
#define ADJ_MIC_B     20.0
#define ADJ_TEMP_C    -3.3
#define ADJ_HUMID_C   11.0

// Domoticz
const char* domoticz_host = "192.168.1.15";
uint16_t domoticz_port = 8081;
const char* domoticz_url_co2 = "/json.htm?type=command&param=udevice&idx=1&nvalue=";
const char* domoticz_url_temphum = "/json.htm?type=command&param=udevice&idx=18&nvalue=";
const char* domoticz_url_sound = "/json.htm?type=command&param=udevice&idx=20&nvalue=0&svalue=";

// Helpers
SCD30 airSensor;
HTTPClient http;
const size_t capacity = JSON_ARRAY_SIZE(2) + JSON_OBJECT_SIZE(3) + 40;
DynamicJsonDocument jsonDoc(capacity);


void setup() 
{
  Serial.begin(115200);
  delay(100);
  startupWiFi();
  
  // init SCD
  Wire.begin(D1, D2);
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
  Serial.println(String("Requesting ") + domoticz_host + ":" + domoticz_port + url);
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


void loop() 
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
    sendDomoticz(String(domoticz_url_temphum) + 0 + "&svalue=" + temp + ";" + humidity);
  }

  int samples = 40;
  double volts = 0;
  for (int i=0; i<samples; i++)
  {
    volts += sampleSoundVolts(50);
    delay(10);
  }
  volts /= samples;
  double dbs = ADJ_MIC_A + ADJ_MIC_B * log(volts); // determined experimentally using iPhone

  Serial.print("ADC Amp: ");
  Serial.print(dbs);
  Serial.print(" dB ... ");
  Serial.print(volts);
  Serial.println(" V");

  sendDomoticz(String(domoticz_url_sound) + dbs);

  ArduinoOTA.handle();
  delay(1000);
}
