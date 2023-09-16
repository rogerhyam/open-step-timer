
/*

  Design pattern is to have state in a series of variables that 
  hardware interupts attached to the buttons change.
  
  The loop then changes the state of the display and lamp based
  on the values of the variables state.

  Resources:

  rotary encoder tutorial https://youtu.be/V1txmR8GXzE?si=XApPytMB7Z9vfcRU
  TM1637 display library https://github.com/avishorp/TM1637



*/
#include <TM1637Display.h>

// Lamp state
int lampState;
const int OFF = 0;
const int FOCUS = 1;
const int EXPOSE = 2;
const int PAUSE = 3;

// Menu modes
int menuMode;
const int SECONDS = 0;   // encoder effects base time and exposure is for base time
const int STEPS = 1;     // encoder effects step shift and exposure is for step
const int TEST = 2;      // encoder effects number of stripes in test and exposure is sequence
const int BURN = 3;      // burn effects steps of burn and exposure is for base_time adjusted by burn step
const int INTERVAL = 4;  // the size of a step is changed by the encoder
const int FADE = 5;      // encoder effects brightness of LED display

// pin mappings
const int EXPOSE_BUTTON_PIN = 15;   // aka A1
const int FOCUS_BUTTON_PIN = 16;    // aka A2
const int ENCODER_BUTTON_PIN = 17;  // aka A3
const int ENCODER_CLK_PIN = 18;     // aka A4
const int ENCODER_DT_PIN = 19;      // aka A5
const int DISPLAY_DIO_PIN = 20;     // aka A6
const int DISPLAY_CLK_PIN = 21;     // aka A7
const int BUZZER_PIN = 2;
const int RELAY_PIN = 5;

// time variables
double baseTime;           // the number of seconds about which the other numbers are calculated
double stepSize;           // the size of the a step away in fractions of a stop
int stepShift;             // number of steps from the base time we are shifted in STEPS mode (can be negative)
int burnIntensity;         // number of steps from baseTime we will burn for (only positive)
double exposureRemaining;  // time in milliseconds on the current exposure.
double exposureStart;      // time this portion of exposure started. 0 says we weren't in an exposure before.
int testSteps;             // should be in {3,5,7,9}

// encoder tracking variables
int encoderCLKState;
int encoderPreviousCLKState;

// the display object
TM1637Display display(DISPLAY_CLK_PIN, DISPLAY_DIO_PIN);
int displayBrightness;

void setup() {

  // for debug we run the serial monitor
  Serial.begin(9600);

  // buttons and encoder are input pins
  pinMode(EXPOSE_BUTTON_PIN, INPUT_PULLUP);
  pinMode(FOCUS_BUTTON_PIN, INPUT_PULLUP);
  pinMode(ENCODER_BUTTON_PIN, INPUT_PULLUP);
  pinMode(ENCODER_CLK_PIN, INPUT_PULLUP);
  pinMode(ENCODER_DT_PIN, INPUT_PULLUP);

  // display, buzzer and relay are output pins
  pinMode(DISPLAY_DIO_PIN, OUTPUT);
  pinMode(DISPLAY_CLK_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(RELAY_PIN, OUTPUT);

  // bind the interupts to each of the inputs
  attachInterrupt(digitalPinToInterrupt(EXPOSE_BUTTON_PIN), exposeButtonPress, LOW);
  attachInterrupt(digitalPinToInterrupt(FOCUS_BUTTON_PIN), focusButtonPress, LOW);
  attachInterrupt(digitalPinToInterrupt(ENCODER_BUTTON_PIN), encoderButtonPress, LOW);
  attachInterrupt(digitalPinToInterrupt(ENCODER_CLK_PIN), encoderMove, LOW);

  // current position of encoder
  encoderPreviousCLKState = digitalRead(ENCODER_CLK_PIN);

  // we always start in seconds mode
  menuMode = SECONDS;
  lampState = OFF;

  displayBrightness = 2;
  display.setBrightness(displayBrightness);
  display.showNumberDec(1234);

  baseTime = 10;
  stepShift = 0;
  burnIntensity = 1;
  testSteps = 5;
  exposureStart = -1;  // we are not counting
  exposureRemaining = 0;
}



// the loop routine runs over and over again forever:
void loop() {

  /*
  delay(1000);
  display.setBrightness(displayBrightness);
  display.showNumberDec(displayBrightness, false);
  
  Serial.println(displayBrightness);

  displayBrightness++;
  if(displayBrightness > 7) displayBrightness = 0;
*/
}


void encoderMove() {

  static unsigned long last_interrupt_time = 0;
  unsigned long interrupt_time = millis();
  // If interrupts come faster than 50ms, assume it's a bounce and ignore
  if (interrupt_time - last_interrupt_time > 20) {

    bool increase = digitalRead(ENCODER_DT_PIN);

    switch (menuMode) {

      case SECONDS:
        if (increase) {
          baseTime = baseTime + 1;
          if (baseTime > 99) baseTime = 99;
        } else {
          baseTime = baseTime - 1;
          if (baseTime < 1) baseTime = 1;
        }
        Serial.print("baseTime: ");
        Serial.println(baseTime);
        break;

      case STEPS:
        if (increase) {
          stepShift = stepShift + 1;
          if (stepShift > 9) stepShift = 9;
        } else {
          stepShift = stepShift - 1;
          if (stepShift < -9) stepShift = -9;
        }
        Serial.print("stepShift: ");
        Serial.println(stepShift);
        break;

      case BURN:
        if (increase) {
          burnIntensity = burnIntensity + 1;
          if (burnIntensity > 9) burnIntensity = 9;
        } else {
          burnIntensity = burnIntensity - 1;
          if (burnIntensity < 1) burnIntensity = 1;
        }
        Serial.print("burnIntensity: ");
        Serial.println(burnIntensity);
        break;

      case TEST:
        if (increase) {
          testSteps = testSteps + 2;
          if (testSteps > 9) testSteps = 9;
        } else {
          testSteps = testSteps - 2;
          if (testSteps < 3) testSteps = 3;
        }
        Serial.print("testSteps: ");
        Serial.println(testSteps);
        break;
        
      default:
        Serial.println("unrecognised menu mode");
        break;
    }
  }
  last_interrupt_time = interrupt_time;

  /*
  encoderCLKState = digitalRead(ENCODER_CLK_PIN);

  // If the previous and the current state of the inputCLK are different then a pulse has occured
  if (encoderCLKState != encoderPreviousCLKState) {

    if (digitalRead(ENCODER_DT_PIN)) {
      Serial.println("ENCODER moved UP");
    } else {
      Serial.println("ENCODER moved DOWN");
    }
  }

  // Update previousStateCLK with the current state
  encoderPreviousCLKState = encoderCLKState;
*/
}

void exposeButtonPress() {
  static unsigned long last_interrupt_time = 0;
  unsigned long interrupt_time = millis();
  // If interrupts come faster than 200ms, assume it's a bounce and ignore
  if (interrupt_time - last_interrupt_time > 200) {

    Serial.println("EXPOSE button pressed");

    // just change the lampState appropriately
    switch (lampState) {
      case OFF:
        lampState = EXPOSE;
        break;
      case PAUSE:
        lampState = EXPOSE;
        break;
      default:
        lampState = OFF;
        break;
    }
  }
  last_interrupt_time = interrupt_time;
}

void focusButtonPress() {
  static unsigned long last_interrupt_time = 0;
  unsigned long interrupt_time = millis();
  // If interrupts come faster than 200ms, assume it's a bounce and ignore
  if (interrupt_time - last_interrupt_time > 200) {

    Serial.println("FOCUS button pressed");

    // just change the lampState appropriately
    switch (lampState) {
      case OFF:
        lampState = FOCUS;
        break;
      default:
        lampState = OFF;
        break;
    }
  }
  last_interrupt_time = interrupt_time;
}

void encoderButtonPress() {
  static unsigned long last_interrupt_time = 0;
  unsigned long interrupt_time = millis();
  // If interrupts come faster than 200ms, assume it's a bounce and ignore
  if (interrupt_time - last_interrupt_time > 200) {

    Serial.println("ENCODER button pressed");

    // just change the lampState appropriately
    switch (menuMode) {
      case SECONDS:
        menuMode = STEPS;
        break;
      case STEPS:
        menuMode = BURN;
        break;
      case BURN:
        menuMode = TEST;
        break;
      case TEST:
        menuMode = INTERVAL;
        break;
      case INTERVAL:
        menuMode = FADE;
        break;
      default:
        menuMode = STEPS;  // should never happen
        break;
    }
    Serial.print("menuMode: ");
    Serial.println(menuMode);
  }
  last_interrupt_time = interrupt_time;
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