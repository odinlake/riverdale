/*
 *  Simple HTTP get webclient test
 */
#include <ESP8266WiFi.h>

#include <Wire.h>
#include "SparkFun_SCD30_Arduino_Library.h"


const char* ssid     = "Valhalla";
const char* password = "cloudypanda668";

const char* host_report = "192.168.1.15";
uint16_t port_report = 8080;
const char* url_report = "/api/v1/md5XHApKw4UMDAmOfB1R/telemetry";

SCD30 airSensor;


void setup() {
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

int value = 0;

void loop() {
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

    WiFiClient client;
    if (!client.connect(host_report, port_report)) {
      Serial.print("connection failed to: ");
      Serial.print(host_report);
      Serial.print(":");
      Serial.println(port_report);
    } else {
      String postData = String("{") +
        "\"co2\": " + co2 + ", " +
        "\"temperature\": " + temp + ", " +
        "\"humidity\": " + humidity +
      "}";
      Serial.print("Requesting URL: ");
      Serial.println(url_report);
      client.print(String("POST ") + url_report + " HTTP/1.1" + "\r\n" +
        "Host: " + host_report + "\r\n" +
        "Port: " + port_report + "\r\n" +
        "Content-Type: application/json" + "\r\n"
        "Content-Length:" + postData.length() + "\r\n"
      );
      client.print("\r\n");
      client.print(postData);
      client.print("\r\n\r\n");
      Serial.println(postData);
    }
  }
  
  if (value % 2) {
    digitalWrite(13, HIGH);
  } else {
    digitalWrite(13, LOW);
  }
  
  delay(1000);
  ++value;

  /*
  return;
  
  delay(5000);

  Serial.print("connecting to ");
  Serial.println(host);
  
  // Use WiFiClient class to create TCP connections
  WiFiClient client;
  const int httpPort = 80;
  if (!client.connect(host, httpPort)) {
    Serial.println("connection failed");
    return;
  }
  
  // We now create a URI for the request
  String url = "/testwifi/index.html";
  Serial.print("Requesting URL: ");
  Serial.println(url);
  
  // This will send the request to the server
  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" + 
               "Connection: close\r\n\r\n");
  delay(500);
  
  // Read all the lines of the reply from server and print them to Serial
  while(client.available()){
    String line = client.readStringUntil('\r');
    Serial.print(line);
  }
  
  Serial.println();
  Serial.println("closing connection");
  */
}
