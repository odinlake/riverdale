#include <sstream>
#include <iostream>


#define MARANTZ_VOL_UP_1    0x2CD54D5
#define MARANTZ_VOL_UP_2    0x2AD54D5
#define MARANTZ_VOL_DOWN_1  0x59AA9A9
#define MARANTZ_VOL_DOWN_2  0x55AA9A9
#define MARANTZ_POWER_1  0x2CD552D
#define MARANTZ_POWER_2  0x2AD552D


unsigned int INPUT_PIN = 25;
unsigned int OUTPUT_PIN = 26;

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
  Serial.println("Hello World.");
  pinMode(INPUT_PIN, INPUT);
  //pinMode(INPUT_PIN, INPUT_PULLUP);
  pinMode(OUTPUT_PIN, OUTPUT);
  digitalWrite(BUILTIN_LED, LOW);
  digitalWrite(OUTPUT_PIN, HIGH);
  startTimerMicros();
}


inline std::string decode_manchester(const std::string &sig, unsigned long &rc5)
{
  std::string err = "";
  int start = 1;
  size_t n = sig.length();
  rc5 = 0;
  for (int i=start; i<n; i+=2) {
    if (sig[i] == sig[i-1]) {
      start = 2;
      rc5 = (sig[0] == '1') ? 1 : 0;
      break;
    }
  }
  for (int i=start; i<n; i+=2) {
    if (sig[i-1] == sig[i]) err = "bad code";
    if (sig[i-1] == 'x' || sig[i] == 'x') err = "bad signal";
    unsigned long sbit = (sig[i-1] < sig[i]) ? 1 : 0;
    rc5 = (rc5 << 1) | sbit;
  }
  if ((n - start) % 2 == 0) {
    unsigned long sbit = (sig[n-1] == '0') ? 1 : 0;
    rc5 = (rc5 << 1) | sbit;
  }
  return err;
}


inline std::string decode_manchester_ext(const std::string &sig, unsigned long &rc5, unsigned long &rc5ext)
{
  int count = 0;
  std::string err = "";
  std::string s;
  rc5 = 0;
  rc5ext = 0;
  std::istringstream f(sig);
  while (getline(f, s, '-')) {
    if (count == 1) rc5ext = rc5;
    else if (count > 1) err = "overflow";
    err = decode_manchester(s, rc5);
    count += 1;
  }
  return err;
}


void print_signal(char *szSig)
{
  unsigned long rc5 = 0;
  unsigned long rc5ext = 0;
  std::string err;
  err = decode_manchester_ext(szSig, rc5, rc5ext);
  Serial.print(" "); Serial.print(rc5, BIN); Serial.print("-"); Serial.print(rc5ext, BIN);
  Serial.print(" "); Serial.print(rc5, HEX); Serial.print("-"); Serial.print(rc5ext, HEX);
  Serial.print(" "); Serial.print(szSig);
  Serial.print(" ... "); Serial.print(err.c_str());
  Serial.println(" ");
}


inline bool measureIR(bool verbose=true)
{
  int sig = digitalRead(INPUT_PIN);
  unsigned long ellapsed = getEllapsedMicros();
  static unsigned int rc5 = 0;
  static int rc5err = 0;
  
  if (sig != prevSig && ellapsed > 50)
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
        char sigmark = prevSig ? '1' : '0';
        rec[recLen++] = sigmark;
        if (ellapsed > 2900) {
          // Marantz extension code involves extra long LOW
          rec[recLen++] = '-';
        } else if (ellapsed >= 1200 && ellapsed <= 1600) {
          rec[recLen++] = 'x'; // ambiguous
        } else if (ellapsed > 1600) {
          rec[recLen++] = sigmark;
        }
      }
      rec[recLen] = 0;
    } 
    prevSig = sig;
    finished = 0;
  }
  else if (ellapsed >= 5000 && finished == 0)
  {
    if (recLen > 4)
    {
      if (verbose) Serial.println("");
      print_signal(rec);
    }
    recCode = 0;
    recLen = 0;
    rec[0] = 0;
    finished = 1;
    rc5 = 0;
    rc5err = 0;
  }
  return (finished == 0);
}

inline void printIR()
{
  static unsigned long counter = 0;
  counter += 1;
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
  //pinMode(OUTPUT_PIN, INPUT);
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
  if (ellapsed < 1000) return true;
  Serial.print(" ");
  Serial.print(ellapsed);
  transPhase += 1;
  transTime = micros();
  if (transPhase < 3) return true;
  int sig = ((transCode & transMask) == 0) ? LOW : HIGH;
  digitalWrite(OUTPUT_PIN, sig);
  Serial.print((sig == 0) ? "_" : "1");
  sig = digitalRead(INPUT_PIN);
  Serial.print((sig == 0) ? "o" : "X");
  transMask >>= 1;
  if (transMask == 0) stopSendIR();
  return true;
}


unsigned long loopTime = 0;


void loop() 
{
  static unsigned long counter = 0;
  unsigned long ellapsed = millis() - loopTime;
  bool isBusy = false;

  /*
  char *sz1 = "10110011010101010011010101";
  char *sz2 = "101100110101010100110101001";
  char *sz3 = "10110011010101010100101101";
  char *sz4 = "101100110101010101001011001";
  print_signal(sz1);
  print_signal(sz2);
  print_signal(sz3);
  print_signal(sz4);
  Serial.print("\n");
  delay(5000);
  */
  
  //isBusy = isBusy || sendCycleIR();
  //printIR();
  isBusy = isBusy || measureIR(false);
  //isBusy = isBusy || measureIR(true);
  
  /*
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
  */
}
