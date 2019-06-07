#include "credentials.h"
#include <IRsend.h>
#include <ESP8266WiFi.h>
#include <stdarg.h>

// must use MY CUSTOMIZED versions
#include <MyProtocol.h>
#include <MyMessage.h>

#define MARANTZ_POWER_1     0x2CD552D
#define MARANTZ_POWER_2     0x2AD552D
#define MARANTZ_VOL_UP_1    0x2CD54D5
#define MARANTZ_VOL_UP_2    0x2AD54D5
#define MARANTZ_VOL_DOWN_1  0x59AA9A9
#define MARANTZ_VOL_DOWN_2  0x55AA9A9

#define MYID_AMPONOFF       1
#define AMP_ONOFF_PIN       D7
#define MAX_SRV_CLIENTS     2
#define MYSENSOR_PORT       80
#define MAX_RECEIVE_LENGTH  100
#define MAX_SEND_LENGTH     120
#define CLIENT_TIMEOUT      3 * 60000

#define ARRAY_SIZE(x)  (sizeof(x)/sizeof(x[0]))


typedef struct
{
  char    string[MAX_RECEIVE_LENGTH];
  char    tok_end[2];
  uint8_t idx;
} inputBuffer;


inputBuffer inputString[MAX_SRV_CLIENTS];
WiFiServer server(MYSENSOR_PORT);
WiFiClient clients[MAX_SRV_CLIENTS];
unsigned long clientsConnected[MAX_SRV_CLIENTS];
IRsend irSend(D3);
MyMessage msgOnOff(MYID_AMPONOFF, V_STATUS);
MyMessage msg;


void customWait(unsigned long ms) { delay(ms); }
bool customParse(MyMessage &message, char *inputString) { return protocolParse(message, inputString); }


// initial section
void setup() 
{
  Serial.begin(115200);
  for (int i=0; i<2; i++) { Serial.print("."); delay(500); }
  startupWiFi();
  Serial.println("Hello World.");
  pinMode(AMP_ONOFF_PIN, INPUT);
  irSend.begin();
  server.begin();
  server.setNoDelay(true);  
}


// main loop
void loop() 
{
  unsigned long curTime = millis();
  handle_client_connect(curTime);
  handle_client_io(curTime);
  mysBroadcastState(curTime);
}


//
// HANDLE CLIENT CONNECTIVITY
//
void handle_client_connect(unsigned long curTime)
{
  bool allSlotsOccupied = true;
  for (uint8_t i = 0; i < ARRAY_SIZE(clients); i++) {
    if (!clients[i].connected() || curTime >= clientsConnected[i]) {
      if (clientsConnected[i]) {
        Serial.print("Client ");
        Serial.print(i);
        Serial.println(" disconnected");
        clients[i].stop();
        clientsConnected[i] = 0;
      }
      if (server.hasClient()) {
        clients[i] = server.available();
        inputString[i].idx = 0;
        Serial.print("Client ");
        Serial.print(i);
        Serial.println(" connected");
        mysOutput(PSTR("0;0;%d;0;%d;Gateway startup complete.\n"), C_INTERNAL, I_GATEWAY_READY);
        mysOutput(PSTR("0;0;%d;0;%d;Study Amplifier Control\n"), C_INTERNAL, I_SKETCH_NAME);
        mysOutput(PSTR("0;0;%d;0;%d;1.0\n"), C_INTERNAL, I_SKETCH_VERSION);
        mysOutput(PSTR("1;0;%d;0;%d;Amplifier On/Off Switch\n"), C_PRESENTATION, S_BINARY);
        mysSendState();
        /*
        mysOutput(PSTR("1;0;%d;0;%d;%d\n"), C_SET, V_STATUS, 1);
        mysOutput(PSTR("1;0;%d;0;%d;Kitchen TRV\n"), C_PRESENTATION, S_DIMMER);
        mysOutput(PSTR("1;0;%d;0;%d;%d\n"), C_SET, V_PERCENTAGE, trvLevel);
        mysOutput(PSTR("2;0;%d;0;%d;Test RGB\n"), C_PRESENTATION, S_RGB_LIGHT);
        */
        clientsConnected[i] = curTime + CLIENT_TIMEOUT;
      }
    }
    allSlotsOccupied &= clients[i].connected();
  }
  while (allSlotsOccupied && server.hasClient()) {
    Serial.println("Client rejected: No free slot");
    WiFiClient c = server.available();
    c.stop();
  }
}


//
// HANDLE CLIENT CONNECTIVITY
//
void handle_client_io(unsigned long curTime)
{
  for (uint8_t i = 0; i < ARRAY_SIZE(clients); i++) {
    while(clients[i].connected() && clients[i].available()) {
      customWait(1); // yield to wifi library
      clientsConnected[i] = curTime + CLIENT_TIMEOUT;
      char inChar = clients[i].read();
      if ( inputString[i].idx < MAX_RECEIVE_LENGTH - 1 ) {
        if (inChar != '\n') {
          inputString[i].string[inputString[i].idx++] = inChar;
        }
        else if (inChar == '\n') {
          inputString[i].string[inputString[i].idx++] = '\n';
          inputString[i].string[inputString[i].idx] = 0;
          Serial.print("Client "); 
          Serial.print(i); 
          Serial.print(" says: "); 
          Serial.print(inputString[i].string);
          if (customParse(msg, inputString[i].string)) {
            uint8_t command = mGetCommand(msg);
            if (msg.destination==GATEWAY_ADDRESS && command==C_INTERNAL) {
              switch(msg.type) {
              case I_VERSION:
                mysOutput(PSTR("0;0;%d;0;%d;%s\n"), C_INTERNAL, I_VERSION, LIBRARY_VERSION);
                break;
              case I_HEARTBEAT_REQUEST:
                mysOutput(PSTR("0;0;%d;0;%d;PONG\n"), C_INTERNAL, I_HEARTBEAT_RESPONSE);
                break;
              default:
                Serial.println("  ..unknown gateway message"); 
              }
            }
            else if (msg.destination==1 && msg.sensor==0 && command==C_SET && (msg.type==V_DIMMER || msg.type==V_STATUS)) {
              if (msg.type == V_STATUS) {
                irSwitchAmplifier(msg.getBool()?1:0);
                if (mGetRequestAck(msg))
                  mysOutput(PSTR("1;0;%d;1;%d;%d\n"), command, msg.type, msg.getBool());
              }
              else {
                Serial.print("  ..would set: ");
                Serial.println(msg.getByte());
                //trvLevel = msg.getByte();
                //if(trvLevel > 100)
                  //trvLevel = 100;
              }
            }
            else {
              Serial.println("  ..unknown"); 
              if (mGetRequestAck(msg))
                mysOutput(PSTR("1;0;%d;1;%d;%d\n"), command, msg.type, msg.getBool());
            }
            /*
            else if (msg.destination==1 && msg.sensor==0 && command==C_SET && (msg.type==V_DIMMER || msg.type==V_STATUS)) {
              if (msg.type == V_STATUS) {
                trvLevel = msg.getBool() ? 100 : 0;
                if (mGetRequestAck(msg))
                  mysOutput(PSTR("1;0;%d;1;%d;%d\n"), command, msg.type, msg.getBool());
              }
              else {
                trvLevel = msg.getByte();
                if(trvLevel > 100)
                  trvLevel = 100;
              }
              trvTime = curTime;
            }
            else if (msg.destination == 2 && msg.sensor == 0 && command == C_SET) {
              if (mGetRequestAck(msg))
              {
                char retbuf[16];
                mysOutput(PSTR("2;0;%d;1;%d;%s"), command, msg.type, msg.getString(retbuf));
              }
            }
            */
          }
          inputString[i].idx = 0;
          break;
        }
      } else {
        Serial.print("Client ");
        Serial.print(i);
        Serial.println(": Message dropped, too long");
        inputString[i].idx = 0;
        break;
      }
    }
  }
}

//
// Send state of amplifier to all clients periodically
//
void mysBroadcastState(unsigned long curTime)
{
  static unsigned long lastTrigger = 0;
  unsigned long ellapsed = curTime - lastTrigger;
  if (ellapsed > 60000) {
    lastTrigger = curTime;
    mysSendState();
  }
}


//
// send MySensor data to clients
//
void mysOutput(const char *fmt, ... )
{
  char serialBuffer[MAX_SEND_LENGTH];
  va_list args;
  va_start(args, fmt);
  vsnprintf_P(serialBuffer, MAX_SEND_LENGTH, fmt, args);
  va_end(args);
  Serial.print(serialBuffer);
  for (uint8_t i = 0; i < ARRAY_SIZE(clients); i++) {
    if (clients[i] && clients[i].connected()) {
       Serial.print("Client "); Serial.print(i); Serial.println(" write");
       clients[i].write((uint8_t*)serialBuffer, strlen(serialBuffer));
    }
  }
}


//
// send amplifier state to mysensor
//
void mysSendState()
{
    int amplifierStatus = digitalRead(AMP_ONOFF_PIN);
    Serial.print("Amplifier is: ");
    Serial.println((amplifierStatus==HIGH)?"on":"off");
    mysOutput(PSTR("1;0;%d;1;%d;%d\n"), C_SET, V_STATUS, amplifierStatus==1);
    //send(msgOnOff.set(amplifierStatus==HIGH), false);
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
