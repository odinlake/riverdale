
#define MARANTZ_VOL_UP_1    0x2CD54D5
#define MARANTZ_VOL_UP_2    0x2AD54D5
#define MARANTZ_VOL_DOWN_1  0x59AA9A9
#define MARANTZ_VOL_DOWN_2  0x55AA9A9
#define MARANTZ_POWER_1  0x2CD552D
#define MARANTZ_POWER_2  0x2AD552D


unsigned int INPUT_PIN = D5;
unsigned int OUTPUT_PIN = D6;

unsigned int counter = 0;
int prevSig = 0;
unsigned long prevTime = 0;
int overflowTimes = 0;
char rec[100] = "";
int recLen = 0;
unsigned long recCode = 0;
int finished = 0;


unsigned long getEllapsedMicros()
{
  static unsigned long threshold = 5000000;
  unsigned long ellapsed = micros() - prevTime;
  if (ellapsed > threshold)
  {
    prevTime = micros();
    overflowTimes += 1;
  }
  if (overflowTimes) return threshold;
  return ellapsed;
}

void startTimerMicros()
{
  prevTime = micros();
  overflowTimes = 0;
}

void setup() 
{
  Serial.begin(115200);
  for (int i=0; i<5; i++) { Serial.print("."); delay(500); }
  Serial.print("Hello World.");
  pinMode(INPUT_PIN, INPUT);
  pinMode(OUTPUT_PIN, OUTPUT);
  digitalWrite(BUILTIN_LED, LOW);
  startTimerMicros();
}


inline bool measureIR(bool verbose=true)
{
  int sig = digitalRead(INPUT_PIN);
  unsigned long ellapsed = getEllapsedMicros();
  
  if (sig != prevSig)
  {
    startTimerMicros();
    if (ellapsed < 5000) {
      if (verbose) {
        Serial.print(prevSig ? "H:" : "L:");
        Serial.print(ellapsed);
        Serial.print(" ");
      }
      if (recLen < 98)
      {
        char sigmark = prevSig ? '1' : '_';
        unsigned long sigbit = prevSig ? 1 : 0;
        if (ellapsed >= 1200 && ellapsed <= 1600) sigmark = 'x';
        rec[recLen++] = sigmark;
        recCode = (recCode << 1) | sigbit;
        if (ellapsed > 1250) {
          rec[recLen++] = sigmark;
          recCode = (recCode << 1) | sigbit;
        }
      }
      rec[recLen] = 0;
    } 
    prevSig = sig;
    finished = 0;
  }
  else if (ellapsed >= 20000 && finished == 0)
  {
    if (verbose) Serial.println("");
    Serial.print(rec);
    Serial.print(" : ");
    Serial.print(recCode);
    Serial.print(" : ");
    Serial.print(recCode, BIN);
    Serial.print(" : ");
    Serial.print(recCode, HEX);
    Serial.print("\n");
    recCode = 0;
    recLen = 0;
    rec[0] = 0;
    finished = 1;
  }
  return (finished == 0);
}

inline void printIR()
{
  int sig = digitalRead(INPUT_PIN);
  Serial.print(sig ? "1" : "_");
  if (counter % 160 == 0) Serial.println("");
}


unsigned int transCode = 0;
unsigned int transMask = 0;
int transPhase = -1;
unsigned long transTime = 0;

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
  if (ellapsed < 900) return true;
  transPhase += 1;
  transTime = micros();
  if (transPhase < 3) return true;
  digitalWrite(OUTPUT_PIN, (transCode & transMask == 0) ? LOW : HIGH);
  Serial.print((transCode & transMask) == 0 ? "_" : "1");
  transMask >>= 1;
  if (transMask == 0) stopSendIR();
  return true;
}


unsigned long loopTime = 0;

void loop() 
{
  unsigned long ellapsed = millis() - loopTime;
  bool isBusy = sendCycleIR();
  //printIR();
  //isBusy = isBusy || measureIR(true);

  if (ellapsed > 5000 && !isBusy) {
    if (counter == 0) sendIR(MARANTZ_POWER_1);
    else if (counter == 1) sendIR(MARANTZ_POWER_1);
    else if (counter == 2) sendIR(MARANTZ_POWER_1);
    else if (counter == 3) sendIR(MARANTZ_POWER_2);
    else if (counter == 4) sendIR(MARANTZ_POWER_2);
    else if (counter == 5) sendIR(MARANTZ_VOL_UP_1);
    else if (counter == 6) sendIR(MARANTZ_VOL_UP_2);
    else if (counter == 7) sendIR(MARANTZ_VOL_DOWN_1);
    else if (counter == 8) sendIR(MARANTZ_VOL_DOWN_2);
    else counter = 0;
    counter += 1;
    loopTime += ellapsed;
  }
  
}
