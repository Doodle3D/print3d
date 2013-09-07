unsigned long serialSpeed = 115200;
//unsigned long serialSpeed = 250000;
int ledPin = 13;
int blinkDelay = 1000;
unsigned long prevBlinkTime = 0;
boolean ledmode = true;
void setup() {                
  // initialize the digital pin as an output.
  // Pin 13 has an LED connected on most Arduino boards:
  pinMode(ledPin, OUTPUT);    
  Serial.begin(serialSpeed); //115200
  Serial.println("start");
  //digitalWrite(ledPin, HIGH);   // set the LED on
  //delay(3000);
}
void loop() {
  //digitalWrite(ledPin, HIGH);   // set the LED on
  //delay(blinkDelay);              // wait for a second
  //digitalWrite(ledPin, LOW);    // set the LED off
  //delay(blinkDelay);              // wait for a second
  if(millis()-prevBlinkTime > blinkDelay) {
    ledmode = !ledmode;
    digitalWrite(ledPin, ledmode? HIGH : LOW);   // set the LED on
    prevBlinkTime = millis();
  }
}
void serialEvent() {
  while (Serial.available()) {
    // get the new byte:
    int newDelay = Serial.parseInt(); 
    if(newDelay > 0) {
      Serial.print("ok ");
      //Serial.print("newDelay: ");
      Serial.println(newDelay);
      //Serial.println("ok");
      blinkDelay = newDelay;
    }
  }
}

