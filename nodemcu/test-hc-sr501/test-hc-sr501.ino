unsigned int counter = 0;
static int MOTION_PIN = D5;

void setup() 
{
  Serial.begin(115200);
  Serial.println("Hello World.");
  pinMode(BUILTIN_LED, OUTPUT);
  pinMode(MOTION_PIN, INPUT);
  digitalWrite(BUILTIN_LED, LOW);
}

void loop() 
{
  counter += 1;
  int motion = digitalRead(MOTION_PIN);
  //digitalWrite(BUILTIN_LED, motion);
  Serial.print("MotionStatus: ");
  Serial.println(motion);
  
  for (int i=0; i<5; i++)
  {
    digitalWrite(BUILTIN_LED, HIGH); delay(50);
    digitalWrite(BUILTIN_LED, LOW); delay(50);
  }
  
  /*
  Serial.print("led="); Serial.println(BUILTIN_LED);
  Serial.print(0); Serial.print(" "); Serial.println(D0);
  Serial.print(1); Serial.print(" "); Serial.println(D1);
  Serial.print(2); Serial.print(" "); Serial.println(D2);
  Serial.print(3); Serial.print(" "); Serial.println(D3);
  Serial.print(4); Serial.print(" "); Serial.println(D4);
  Serial.print(5); Serial.print(" "); Serial.println(D5);
  Serial.print(6); Serial.print(" "); Serial.println(D6);
  Serial.print(7); Serial.print(" "); Serial.println(D7);
  Serial.print(8); Serial.print(" "); Serial.println(D8);
  */

  delay(500);
}
