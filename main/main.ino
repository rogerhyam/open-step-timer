
/*

  Design pattern is to have state in a series of variables that 
  hardware interupts attached to the buttons change.
  
  The loop then changes the state of the display and lamp based
  on the values of the variables state.

*/



int pinled = 13;
int buzzerPin = 8;

void setup() {                
  // initialize the digital pin as an output.
  pinMode(pinled, OUTPUT);     
}

// the loop routine runs over and over again forever:
void loop() {
  digitalWrite(pinled, HIGH);   // turn the LED on 
  tone(buzzerPin, 440, 30);
  delay(1000);               // wait for a second
  digitalWrite(pinled, LOW);    // turn the LED off 
  noTone(buzzerPin);
  delay(1000);               // wait for a second
}


/*

handling the bounce issue

void my_interrupt_handler()
{
  static unsigned long last_interrupt_time = 0;
  unsigned long interrupt_time = millis();
  // If interrupts come faster than 200ms, assume it's a bounce and ignore
  if (interrupt_time - last_interrupt_time > 200) 
  {
    ... do your thing
  }
  last_interrupt_time = interrupt_time;
}


*/