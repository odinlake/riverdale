/**
 * The MySensors Arduino library handles the wireless radio link and protocol
 * between your home built sensors/actuators and HA controller of choice.
 * The sensors forms a self healing radio network with optional repeaters. Each
 * repeater and gateway builds a routing tables in EEPROM which keeps track of the
 * network topology allowing messages to be routed to nodes.
 *
 * Created by Henrik Ekblad <henrik.ekblad@mysensors.org>
 * Copyright (C) 2013-2015 Sensnology AB
 * Full contributor list: https://github.com/mysensors/Arduino/graphs/contributors
 *
 * Documentation: http://www.mysensors.org
 * Support Forum: http://forum.mysensors.org
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 *******************************
 *
 * REVISION HISTORY
 * Version 1.0 - Henrik EKblad
 * Contribution by a-lurker and Anticimex, 
 * Contribution by Norbert Truchsess <norbert.truchsess@t-online.de>
 * Contribution by Ivo Pullens (ESP8266 support)
 * 
 * DESCRIPTION
 * The EthernetGateway sends data received from sensors to the WiFi link. 
 * The gateway also accepts input on ethernet interface, which is then sent out to the radio network.
 *
 * VERA CONFIGURATION:
 * Enter "ip-number:port" in the ip-field of the Arduino GW device. This will temporarily override any serial configuration for the Vera plugin. 
 * E.g. If you want to use the defualt values in this sketch enter: 192.168.178.66:5003
 *
 * LED purposes:
 * - To use the feature, uncomment WITH_LEDS_BLINKING in MyConfig.h
 * - RX (green) - blink fast on radio message recieved. In inclusion mode will blink fast only on presentation recieved
 * - TX (yellow) - blink fast on radio message transmitted. In inclusion mode will blink slowly
 * - ERR (red) - fast blink on error during transmission error or recieve crc error  
 * 
 * See http://www.mysensors.org/build/ethernet_gateway for wiring instructions.
 * The ESP8266 however requires different wiring:
 * nRF24L01+  ESP8266
 * VCC        VCC
 * CE         GPIO4          
 * CSN/CS     GPIO15
 * SCK        GPIO14
 * MISO       GPIO12
 * MOSI       GPIO13
 *            
 * Not all ESP8266 modules have all pins available on their external interface.
 * This code has been tested on an ESP-12 module.
 * The ESP8266 requires a certain pin configuration to download code, and another one to run code:
 * - Connect REST (reset) via 10K pullup resistor to VCC, and via switch to GND ('reset switch')
 * - Connect GPIO15 via 10K pulldown resistor to GND
 * - Connect CH_PD via 10K resistor to VCC
 * - Connect GPIO2 via 10K resistor to VCC
 * - Connect GPIO0 via 10K resistor to VCC, and via switch to GND ('bootload switch')
 * 
  * Inclusion mode button:
 * - Connect GPIO5 via switch to GND ('inclusion switch')
 * 
 * Hardware SHA204 signing is currently not supported!
 *
 * Make sure to fill in your ssid and WiFi password below for ssid & pass.
 */
#define NO_PORTB_PINCHANGES 

#include <SPI.h>  

//#include <MySigningNone.h> 
//#include <MySigningAtsha204Soft.h>
#include <EEPROM.h>
#include <MyHwESP8266.h>
#include <ESP8266WiFi.h>

#include <MyParserSerial.h>  
#include <MySensor.h>  
#include <stdarg.h>

#define I_HEARTBEAT 18

const char *ssid =  "ap_name";    // cannot be longer than 32 characters!
const char *pass =  "ap_pass"; //

// Hardware profile 
MyHwESP8266 hw;

IPAddress ip(172, 27, 0, 2);
IPAddress gw(172, 27, 0, 1);
IPAddress net(255, 255, 0, 0);
 
#define IP_PORT 5003         // The port you want to open 
#define MAX_SRV_CLIENTS 2    // how many clients should be able to telnet to this ESP8266

// a R/W server on the port
static WiFiServer server(IP_PORT);
static WiFiClient clients[MAX_SRV_CLIENTS];
static unsigned long clientsConnected[MAX_SRV_CLIENTS];

#define MAX_RECEIVE_LENGTH 100 // Max buffersize needed for messages coming from controller
#define MAX_SEND_LENGTH 120 // Max buffersize needed for messages destined for controller

#define PWM_PIN 0
#define LED_PIN 2

typedef struct
{
  char    string[MAX_RECEIVE_LENGTH];
  char    tok_end[2];
  uint8_t idx;
} inputBuffer;

inputBuffer inputString[MAX_SRV_CLIENTS];

MyParserSerial parser; 

#define ARRAY_SIZE(x)  (sizeof(x)/sizeof(x[0]))

unsigned long ledTime;
unsigned long curTime;

void output(const char *fmt, ... )
{
  char serialBuffer[MAX_SEND_LENGTH];
  va_list args;
  va_start (args, fmt );
  vsnprintf_P(serialBuffer, MAX_SEND_LENGTH, fmt, args);
  va_end (args);
  Serial.print(serialBuffer);
  for (uint8_t i = 0; i < ARRAY_SIZE(clients); i++)
  {
    if (clients[i] && clients[i].connected())
    {
//       Serial.print("Client "); Serial.print(i); Serial.println(" write");
       clients[i].write((uint8_t*)serialBuffer, strlen(serialBuffer));
       ledTime = curTime;
    }
  }
}

void setup()  
{ 
  // Setup console
  hw_init();

  Serial.println(); Serial.println();
  Serial.println("ESP8266 MySensors Gateway");
  Serial.print("Connecting to "); Serial.println(ssid);

  pinMode(PWM_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);

  WiFi.mode(WIFI_STA);
  WiFi.config(ip, gw, net);

  (void)WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("Connected!");
  Serial.print("IP: "); Serial.println(WiFi.localIP());
  Serial.flush();
   
  // start listening for clients
  server.begin();
  server.setNoDelay(true);  
}

int trvLevel = 0;

const unsigned long clientTimeout = 3 * 60000;

void loop() {
  curTime = millis();
  
  // Go over list of clients and stop any that are no longer connected.
  // If the server has a new client connection it will be assigned to a free slot.
  bool allSlotsOccupied = true;
  for (uint8_t i = 0; i < ARRAY_SIZE(clients); i++)
  {
    if (!clients[i].connected() || curTime >= clientsConnected[i])
    {
      if (clientsConnected[i])
      {
        Serial.print("Client "); Serial.print(i); Serial.println(" disconnected");
        clients[i].stop();
        clientsConnected[i] = 0;
      }
      //check if there are any new clients
      if (server.hasClient())
      {
        clients[i] = server.available();
        inputString[i].idx = 0;
        Serial.print("Client "); Serial.print(i); Serial.println(" connected"); 
        output(PSTR("0;0;%d;0;%d;Gateway startup complete.\n"), C_INTERNAL, I_GATEWAY_READY);
        output(PSTR("1;0;%d;0;%d;Kitchen TRV\n"), C_PRESENTATION, S_DIMMER);
        output(PSTR("1;0;%d;0;%d;%d\n"), C_SET, V_PERCENTAGE, trvLevel);

				output(PSTR("2;0;%d;0;%d;Test RGB\n"), C_PRESENTATION, S_RGB_LIGHT);

        clientsConnected[i] = curTime + clientTimeout;
      }
    }
    bool connected = clients[i].connected();
    allSlotsOccupied &= connected;
  }
  if (allSlotsOccupied && server.hasClient())
  {
    //no free/disconnected spot so reject
    Serial.println("No free slot available");
    WiFiClient c = server.available();
    c.stop();
  }
  
  static unsigned long trvTime = 0;
  
  // Loop over clients connect and read available data
  for (uint8_t i = 0; i < ARRAY_SIZE(clients); i++)
  {
    while(clients[i].connected() && clients[i].available())
    {
      clientsConnected[i] = curTime + clientTimeout;
      ledTime = curTime;
      
      char inChar = clients[i].read();
      if ( inputString[i].idx < MAX_RECEIVE_LENGTH - 1 )
      { 
        // if newline then command is complete
        if (inChar == '\n')
        {  
          // a command was issued by the client
          // we will now try to send it to the actuator
          inputString[i].string[inputString[i].idx++] = '\n';
          inputString[i].string[inputString[i].idx] = 0;
    
          // echo the string to the serial port
          Serial.print("Client "); Serial.print(i); Serial.print(": "); Serial.print(inputString[i].string);
    
          MyMessage msg;       
          if (parser.parse(msg, inputString[i].string)) {
            uint8_t command = mGetCommand(msg);
        
            if (msg.destination==GATEWAY_ADDRESS && command==C_INTERNAL) {
              // Handle messages directed to gateway
              switch(msg.type)
              {
              case I_VERSION:
                // Request for version
                output(PSTR("0;0;%d;0;%d;%s\n"), C_INTERNAL, I_VERSION, LIBRARY_VERSION);
                break;
/*              case I_HEARTBEAT:
                output(PSTR("0;0;%d;0;%d;PONG\n"), C_INTERNAL, I_HEARTBEAT);
                break;             */
              }
            }
            else if (msg.destination == 1 && msg.sensor == 0 && command==C_SET && ( msg.type == V_DIMMER || msg.type == V_STATUS))
            {
							if (msg.type == V_STATUS)
							{
								trvLevel = msg.getBool() ? 100 : 0;
								if (mGetRequestAck(msg))
									output(PSTR("1;0;%d;1;%d;%d\n"), command, msg.type, msg.getBool());
							}
              else
              {
                trvLevel = msg.getByte();
                if(trvLevel > 100)
                  trvLevel = 100;
              }
              trvTime = curTime;
            }
						else if (msg.destination == 2 && msg.sensor == 0 && command == C_SET)
						{
							if (mGetRequestAck(msg))
							{
								char retbuf[16];
								output(PSTR("2;0;%d;1;%d;%s"), command, msg.type, msg.getString(retbuf));
							}
						}
          }
               
          // clear the string:
          inputString[i].idx = 0;
          // Finished with this client's message. Next loop() we'll see if there's more to read.
          break;
        } else {  
         // add it to the inputString:
         inputString[i].string[inputString[i].idx++] = inChar;
        }
      } else {
        // Incoming message too long. Throw away 
        Serial.print("Client "); Serial.print(i); Serial.println(": Message too long");
        inputString[i].idx = 0;
        // Finished with this client's message. Next loop() we'll see if there's more to read.
        break;
      }
    }
  }

  if( curTime >= trvTime )
  {
    trvTime = curTime + 118624;
    output(PSTR("1;0;%d;0;%d;%d\n"), C_SET, V_PERCENTAGE, trvLevel);
  }

  const unsigned long pwmMask = 0x3ff;
  digitalWrite( PWM_PIN, ( curTime & pwmMask ) * 100 < ( pwmMask + 1 ) * trvLevel );

  digitalWrite( LED_PIN, curTime > ledTime + 100 );
}
 
