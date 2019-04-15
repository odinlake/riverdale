/*
 *  MO home sensor setup for nodeMCU with: 
 *  - SCD30 co2 sensor
 */
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <Wire.h>
#include "SparkFun_SCD30_Arduino_Library.h"
#include <ArduinoJson.h>


// WiFi
const char* ssid     = "Valhalla";
const char* password = "cloudypanda668";

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

  // init WiFi
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

  // init SCD
  Wire.begin(D1, D2);
  Serial.println("SCD30 Example");
  airSensor.begin(); //This will cause readings to occur every two seconds

  // init LED
  pinMode(13, OUTPUT);
  digitalWrite(13, LOW);
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
  double dbs = 75 + 20 * log(volts); // determined experimentally using iPhone

  Serial.print("ADC Amp: ");
  Serial.print(dbs);
  Serial.print(" dB ... ");
  Serial.print(volts);
  Serial.println(" V");

  sendDomoticz(String(domoticz_url_sound) + dbs);

  delay(1000);
}
