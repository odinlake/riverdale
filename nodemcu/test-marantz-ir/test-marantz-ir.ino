#include <IRremoteESP8266.h>
#include <IRrecv.h>
#include <IRutils.h>
#include <IRsend.h>

unsigned int INPUT_PIN = D5;
unsigned int OUTPUT_PIN = D6;

// https://github.com/markszabo/IRremoteESP8266/blob/master/examples/IRrecvDemo/IRrecvDemo.ino
IRrecv irrecv(INPUT_PIN);
IRsend irsend(OUTPUT_PIN); 
decode_results results;

unsigned int counter = 0;

uint16_t rawData[1] = {410};

void setup() 
{
  Serial.begin(115200);
  for (int i=0; i<5; i++) { Serial.print("."); delay(500); }
  Serial.print("Hello World.");
  pinMode(INPUT_PIN, INPUT);
  pinMode(OUTPUT_PIN, OUTPUT);
  digitalWrite(BUILTIN_LED, LOW);
  irrecv.enableIRIn();  // Start the receiver
  while (!Serial) { Serial.print("."); delay(500); }
  Serial.print("IRremoteESp8266 listening on pin ");
  Serial.println(INPUT_PIN);
}

void loop() 
{
  counter += 1;
  int sig = digitalRead(INPUT_PIN);
  digitalWrite(BUILTIN_LED, sig);

  if (irrecv.decode(&results)) {
    // print() & println() can't handle printing long longs. (uint64_t)
    serialPrintUint64(results.value, HEX);
    Serial.println("");
    irrecv.resume();  // Receive the next value
  }
  delay(100);

  if (counter%10==0)
  {
    irsend.sendRaw(rawData, 1, 38); // Send a raw data capture at 38kHz.
    Serial.print("sending: ");
    Serial.println(rawData[0]);
  }
  
  //Serial.print(sig?"1":"_");
  //if (counter%160==0) Serial.println("");
}
