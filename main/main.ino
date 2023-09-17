
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

const double stepIntervalSizes[] = { 0.2, 0.333, 0.5, 0.666, 1 };  // actual values used in calc.
const int stepIntervalNames[] = { 15, 13, 12, 23, 10 };            // will be displayed with colon in middle


// time variables
double baseTime;                      // the number of seconds about which the other numbers are calculated
int stepIntervalIndex;                // which of the step sizes is chosen
int stepShift;                        // number of steps from the base time we are shifted in STEPS mode (can be negative)
int burnIntensity;                    // number of steps from baseTime we will burn for (only positive)
signed long exposureRemainingMillis;  // time in milliseconds on the current exposure - signed so we can tell when it goes negative.
unsigned long exposureStartTime;      // time this portion of exposure started. 0 says we weren't in an exposure before.
int testStrips;                       // should be in {3,5,7,9}
int currentTestStrip;                 // the strip we are currently exposing

// encoder tracking variables
int encoderCLKState;
int encoderPreviousCLKState;

// the display object
TM1637Display display(DISPLAY_CLK_PIN, DISPLAY_DIO_PIN);
int displayBrightness;
unsigned long lastChangedModeTime;       // when the mode was last switched.
unsigned long lastMovedEncoderTime;      // When the encoder was last moved.
unsigned long encoderButtonPressedTime;  // Last time an interupt button press was found
int lastLampOnBeepSecond;        // last time we beeped because the lamp was on.

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
  attachInterrupt(digitalPinToInterrupt(EXPOSE_BUTTON_PIN), exposeButtonPress, FALLING);
  attachInterrupt(digitalPinToInterrupt(FOCUS_BUTTON_PIN), focusButtonPress, FALLING);
  attachInterrupt(digitalPinToInterrupt(ENCODER_BUTTON_PIN), encoderButtonPress, FALLING);
  attachInterrupt(digitalPinToInterrupt(ENCODER_CLK_PIN), encoderMove, LOW);

  // current position of encoder
  encoderPreviousCLKState = digitalRead(ENCODER_CLK_PIN);

  // we always start in seconds mode
  menuMode = SECONDS;
  lastChangedModeTime = millis();
  lampState = OFF;

  displayBrightness = 2;
  display.setBrightness(displayBrightness);
  display.showNumberDec(1234);

  baseTime = 10;
  stepShift = 0;
  stepIntervalIndex = 2;
  burnIntensity = 1;
  testStrips = 5;
  currentTestStrip = 0;
  exposureStartTime = 0;  // we are not counting
  exposureRemainingMillis = 0;

  encoderButtonPressedTime = 0;  // zero means it wasn't pressed
  lastLampOnBeepSecond = 0;        // zero means we aren't timing.

  Serial.println("Setup Complete");
}

// the loop routine runs over and over again forever:
void loop() {

  // first thing we always do is update the display
  display.setBrightness(displayBrightness);
  updateDisplay();

  // handle long and short presses of encoder button
  if (encoderButtonPressedTime > 0) {

    long duration = millis() - encoderButtonPressedTime;

    if (digitalRead(ENCODER_BUTTON_PIN) != LOW || duration > 1000) {

      // it is no longer low so how long was it down?
      if (duration > 1000) {
        encoderButtonLongPress();
      } else {
        encoderButtonShortPress();
      }

      // pin is low so we are not in a press event
      encoderButtonPressedTime = 0;
    }
  }

  // if the lamp is on we beep ever second
  if ((lampState == EXPOSE)) {
    // time remaining in whole seconds
    int remainingSecs = floor(exposureRemainingMillis / 1000);
    if(remainingSecs < lastLampOnBeepSecond){
      tone(BUZZER_PIN, 150, 50);
    }
    lastLampOnBeepSecond = remainingSecs;
  } else {
    lastLampOnBeepSecond = 0;
  }

  // actually make and exposure if it is on
  switch (lampState) {

    case EXPOSE:
      // remove the time that has passed since the start of this exposure period from the time remaining
      // it is like we keep starting a new exposure every loop
      if (exposureRemainingMillis > 0) {
        digitalWrite(RELAY_PIN, HIGH);
        exposureRemainingMillis = exposureRemainingMillis - (millis() - exposureStartTime);
        exposureStartTime = millis();
      } else {
        // we have reached the end beautiful friend
        digitalWrite(RELAY_PIN, LOW);
        lampState = OFF;
        exposureRemainingMillis = 0;
        exposureStartTime = 0;
        tone(BUZZER_PIN, 500, 350);
      }
      break;

    case PAUSE:
      digitalWrite(RELAY_PIN, LOW);
      break;

    case FOCUS:
      // all we do with focus is turn the lamp on.
      digitalWrite(RELAY_PIN, HIGH);
      break;

    default:
      // if all fails turn the lamp off!
      digitalWrite(RELAY_PIN, LOW);
  }
}



/**
  Return the duration of an exposure in steps mode
*/
double getStepsTime() {

  // do nothing if we aren't shifted
  if (stepShift == 0) return baseTime;

  // build a calculated time
  double calcTime = baseTime;
  for (int i = 0; i < abs(stepShift); i++) {
    if (stepShift > 0) {
      calcTime = calcTime * pow(2, stepIntervalSizes[stepIntervalIndex]);
    } else {
      calcTime = calcTime * pow(2, stepIntervalSizes[stepIntervalIndex] * -1);
    }
  }

  return calcTime;
}

/**
  Return the duration of an exposure to burn
  the current number of steps
*/
double getBurnTime() {

  // We need to keep track of the cumulative time the print will have had
  double cumulativeTime = baseTime;
  double burnTime = 0;
  for (int i = 0; i < burnIntensity; i++) {

    // what would the total exposure be if we added a step?
    double newCumulativeTime = cumulativeTime * pow(2, stepIntervalSizes[stepIntervalIndex]);

    // how much extra is this?
    double additionalTime = newCumulativeTime - cumulativeTime;

    // add this extra bit to the total time.
    burnTime = burnTime + additionalTime;

    // increase the cumulative time so when we add the next step
    // it is the correct size
    cumulativeTime = newCumulativeTime;
  }
  return burnTime;
}

/**
  Return the duration of an exposure for a strip
  in a series of strips
*/
double getTestStripTime(int strip) {
  // FIXME: a little more complex!
  return 0;
}

unsigned long getExposureTimeMillis() {

  double exposureSeconds;

  // duration of the exposure depends what mode we are in
  switch (menuMode) {
    case (SECONDS):
      exposureSeconds = baseTime;
      break;
    case (STEPS):
      exposureSeconds = getStepsTime();
      break;
    case (BURN):
      exposureSeconds = getBurnTime();
      break;
    case (TEST):
      exposureSeconds = getTestStripTime(currentTestStrip);
      break;
    default:
      exposureSeconds = 0;
      break;
  }

  return exposureSeconds * 1000;
}

void updateDisplay() {

  if (lampState == OFF) {

    // the lamp is off so ew
    // just call the appropriate function for the mode
    switch (menuMode) {

      case SECONDS:
        updateDisplaySecondsMode();
        break;

      case STEPS:
        updateDisplayStepsMode();
        break;

      case BURN:
        updateDisplayBurnMode();
        break;

      case TEST:
        updateDisplayTestMode();
        break;

      case INTERVAL:
        updateDisplayIntervalMode();
        break;

      case FADE:
        updateDisplayFadeMode();
        break;

      default:
        if (millis() - lastChangedModeTime < 1000) {
          display.showNumberDec(menuMode, false);
        } else {
          display.showNumberDec(2772, false);
        }
        break;
    }

  } else {

    // the lamp is on or paused so we display that
    switch (lampState) {

      case EXPOSE:
        updateDisplayExpose();
        break;

      case FOCUS:
        updateDisplayFocus();
        break;

      case PAUSE:
        updateDisplayPause();
        break;
    }
  }
}

/**

  Display seconds as a two digit one decimal 
  format.

*/
void updateDisplayShowSeconds(double secs) {

  int firstDigit = floor(secs / 10);
  int secondDigit = (int)floor(secs) % 10;
  int fistDecimal = floor((fmod(secs, 1)) * 10);

  uint8_t secondsSegs[] = {
    display.encodeDigit(firstDigit),
    display.encodeDigit(secondDigit),
    SEG_G,  // -
    display.encodeDigit(fistDecimal)
  };
  display.setSegments(secondsSegs);
}

void updateDisplayExpose() {
  // Just show remaining time during an exposure
  updateDisplayShowSeconds(exposureRemainingMillis / 1000);
}

void updateDisplayFocus() {
  const uint8_t SEG_DONE[] = {
    SEG_A | SEG_F | SEG_G | SEG_E,                  // F
    SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F,  // O
    SEG_A | SEG_F | SEG_E | SEG_D,                  // C
    SEG_F | SEG_E | SEG_D | SEG_C | SEG_B           // U

  };
  display.setSegments(SEG_DONE);
}

void updateDisplayPause() {
  const uint8_t SEG_DONE[] = {
    SEG_A | SEG_F | SEG_G | SEG_B | SEG_E,          // P
    SEG_E | SEG_F | SEG_A | SEG_B | SEG_C | SEG_G,  // A
    SEG_F | SEG_E | SEG_D | SEG_C | SEG_B,          // U
    SEG_A | SEG_F | SEG_G | SEG_C | SEG_D           // S

  };
  display.setSegments(SEG_DONE);
}

void updateDisplaySecondsMode() {
  // update the display
  if (millis() - lastChangedModeTime < 2000) {
    // the title
    const uint8_t SEG_DONE[] = {
      SEG_A | SEG_F | SEG_G | SEG_C | SEG_D,  // S
      SEG_A | SEG_D | SEG_E | SEG_F | SEG_G,  // E
      SEG_A | SEG_F | SEG_E | SEG_D,          // C
      SEG_A | SEG_F | SEG_G | SEG_C | SEG_D   // S

    };
    display.setSegments(SEG_DONE);
  } else {
    updateDisplayShowSeconds(baseTime);
  }
}

void updateDisplayStepsMode() {
  // update the display
  if (millis() - lastChangedModeTime < 2000) {
    // the mode title
    const uint8_t SEG_DONE[] = {
      SEG_A | SEG_F | SEG_G | SEG_C | SEG_D,  // S
      SEG_F | SEG_G | SEG_E | SEG_D,          // t
      SEG_A | SEG_D | SEG_E | SEG_F | SEG_G,  // E
      SEG_A | SEG_F | SEG_G | SEG_B | SEG_E   // P
    };
    display.setSegments(SEG_DONE);
    // make like we moved the encode so the steps are displayed after the title
    lastMovedEncoderTime = millis();
  } else {
    if (millis() - lastMovedEncoderTime < 2000) {
      // display the step for two seconds
      display.showNumberDec(stepShift, false);
    } else {
      // then display the calculated time
      updateDisplayShowSeconds(getStepsTime());
    }
  }
}

void updateDisplayBurnMode() {
  // update the display
  if (millis() - lastChangedModeTime < 2000) {
    // the title
    const uint8_t SEG_DONE[] = {
      SEG_F | SEG_E | SEG_D | SEG_C | SEG_G,  // b
      SEG_E | SEG_D | SEG_C,                  // u
      SEG_E | SEG_G,                          // r
      SEG_E | SEG_G | SEG_C                   // n
    };
    display.setSegments(SEG_DONE);
    // make like we moved the encode so the steps are displayed after the title
    lastMovedEncoderTime = millis();
  } else {

    if (millis() - lastMovedEncoderTime < 2000) {
      // display the step for two seconds
      display.showNumberDec(burnIntensity, false);
    } else {
      // then display the calculated time
      updateDisplayShowSeconds(getBurnTime());
    }
  }
}

void updateDisplayTestMode() {
  // update the display
  if (millis() - lastChangedModeTime < 2000) {
    // the title
    const uint8_t SEG_DONE[] = {
      SEG_F | SEG_G | SEG_E | SEG_D,          // t
      SEG_A | SEG_D | SEG_E | SEG_F | SEG_G,  // E
      SEG_A | SEG_F | SEG_G | SEG_C | SEG_D,  // S
      SEG_F | SEG_G | SEG_E | SEG_D           // t
    };
    display.setSegments(SEG_DONE);
  } else {
    uint8_t colon = 0b01000000;

    uint8_t SEG_TEST[] = {
      display.encodeDigit(testStrips),
      SEG_G,                          // -
      SEG_A | SEG_F | SEG_E | SEG_D,  // C
      display.encodeDigit(0)          // strip we are on
    };
    display.setSegments(SEG_TEST);
  }
}

void updateDisplayIntervalMode() {
  // update the display
  static bool beenCleared = false;
  if (millis() - lastChangedModeTime < 2000) {
    // the title
    const uint8_t SEG_DONE[] = {
      SEG_F | SEG_E,                  // I
      SEG_E | SEG_G | SEG_C,          // n
      SEG_F | SEG_G | SEG_E | SEG_D,  // t
      SEG_G                           // -
    };
    display.setSegments(SEG_DONE);
    beenCleared = false;
  } else {

    // we don't want to call this everytime it is called to render
    // or it will flicker
    if (!beenCleared) {
      display.clear();
      beenCleared = true;
    }
    // display the number astride the colon
    display.showNumberDecEx(stepIntervalNames[stepIntervalIndex], 0b11100000, false, 2, 1);
  }
}

void updateDisplayFadeMode() {
  // update the display
  if (millis() - lastChangedModeTime < 2000) {
    // the title
    const uint8_t SEG_DONE[] = {
      SEG_A | SEG_F | SEG_G | SEG_E,                  // F
      SEG_E | SEG_F | SEG_A | SEG_B | SEG_C | SEG_G,  // A
      SEG_B | SEG_C | SEG_D | SEG_E | SEG_G,          // d
      SEG_A | SEG_D | SEG_E | SEG_F | SEG_G           // E
    };
    display.setSegments(SEG_DONE);
  } else {
    display.showNumberDec(displayBrightness, false);
  }
}

void encoderMove() {

  // Do nothing if the lamp is busy
  if (lampState != OFF) return;

  static unsigned long move_last_interrupt_time = 0;
  unsigned long interrupt_time = millis();
  // If interrupts come faster than 50ms, assume it's a bounce and ignore
  if (interrupt_time - move_last_interrupt_time > 50) {

    // keep track of when it was moved so we can change the
    // display a few seconds later
    lastMovedEncoderTime = millis();

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
          testStrips = testStrips + 2;
          if (testStrips > 9) testStrips = 9;
        } else {
          testStrips = testStrips - 2;
          if (testStrips < 3) testStrips = 3;
        }
        Serial.print("testStrips: ");
        Serial.println(testStrips);
        break;

      case INTERVAL:
        if (increase) {
          stepIntervalIndex = stepIntervalIndex + 1;
          if (stepIntervalIndex > 4) stepIntervalIndex = 4;
        } else {
          stepIntervalIndex = stepIntervalIndex - 1;
          if (stepIntervalIndex < 0) stepIntervalIndex = 0;
        }
        Serial.print("stepIntervalIndex: ");
        Serial.println(stepIntervalIndex);
        break;

      case FADE:
        if (increase) {
          displayBrightness = displayBrightness + 1;
          if (displayBrightness > 7) displayBrightness = 7;
        } else {
          displayBrightness = displayBrightness - 1;
          if (displayBrightness < 0) displayBrightness = 0;
        }
        Serial.print("displayBrightness: ");
        Serial.println(displayBrightness);
        break;

      default:
        Serial.println("unrecognised menu mode");
        break;
    }
  }
  move_last_interrupt_time = interrupt_time;
}

void exposeButtonPress() {
  static unsigned long expose_last_interrupt_time = 0;
  unsigned long interrupt_time = millis();
  // If interrupts come faster than 200ms, assume it's a bounce and ignore
  if (interrupt_time - expose_last_interrupt_time > 200) {

    Serial.println("EXPOSE button pressed");

    // just change the lampState appropriately
    switch (lampState) {
      case OFF:
        // we are starting an new exposure period so we mark it.
        exposureStartTime = millis();
        exposureRemainingMillis = getExposureTimeMillis();
        lampState = EXPOSE;
        tone(BUZZER_PIN, 600, 350);
        break;
      case PAUSE:
        // were were on a pause but now we start a new phase
        // no need to set exposureRemaining
        exposureStartTime = millis();
        lampState = EXPOSE;
        tone(BUZZER_PIN, 600, 350);
        break;
      case EXPOSE:
        lampState = PAUSE;
        tone(BUZZER_PIN, 500, 350);
        break;
      default:
        lampState = OFF;
        break;
    }

    lastChangedModeTime = millis();  // we are back to modes
  }
  expose_last_interrupt_time = interrupt_time;
}

void focusButtonPress() {
  static unsigned long focus_last_interrupt_time = 0;
  unsigned long interrupt_time = millis();
  // If interrupts come faster than 200ms, assume it's a bounce and ignore
  if (interrupt_time - focus_last_interrupt_time > 200) {
    tone(BUZZER_PIN, 200, 100);
    Serial.println("FOCUS button pressed");

    // just change the lampState appropriately
    switch (lampState) {
      case OFF:
        lampState = FOCUS;
        break;
      case EXPOSE:
      case PAUSE:
        // focus button acts as a cancel button during
        // and exposure
        exposureStartTime = 0;
        exposureRemainingMillis = 0;
        lampState = OFF;
        break;
      default:
        // shouldn't get here but if we do we go off
        lampState = OFF;
        break;
    }

    lastChangedModeTime = millis();  // we are back to modes
  }
  focus_last_interrupt_time = interrupt_time;
}

void encoderButtonPress() {

  // Do nothing if the lamp is busy
  if (lampState != OFF) return;

  static unsigned long encoder_last_interrupt_time = 0;
  unsigned long interrupt_time = millis();
  // If interrupts come faster than 200ms, assume it's a bounce and ignore
  if (interrupt_time - encoder_last_interrupt_time > 200) {
    Serial.println("ENCODER button down");
    encoderButtonPressedTime = millis();
    Serial.println(encoderButtonPressedTime);
  }
  encoder_last_interrupt_time = interrupt_time;
}

void encoderButtonShortPress() {

  Serial.println("ENCODER short press");

  // just change the mode appropriately
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
      menuMode = SECONDS;
      break;
    case INTERVAL:
      menuMode = STEPS;
      break;
    case FADE:
      menuMode = SECONDS;
      break;
    default:
      menuMode = SECONDS;  // should never happen
      break;
  }
  lastChangedModeTime = millis();
}

void encoderButtonLongPress() {

  tone(BUZZER_PIN, 100, 350);

  // just change the mode appropriately
  switch (menuMode) {
    case SECONDS:
      // long press on seconds allows and interval to be set.
      menuMode = INTERVAL;
      break;
    case STEPS:
      // long press on steps sets the step time as the new base time.
      baseTime = getStepsTime();
      stepShift = 0; // we centre on the new base time too
      menuMode = SECONDS; // flip to the seconds so you can see what you have done
      break;
    case BURN:
      menuMode = FADE;
      break;
    /* we aren't using the long press on TEST yet!
    case TEST:
      menuMode = INTERVAL;
      break;
    */
    default:
      menuMode = SECONDS;  // should never happen
      break;
  }
  lastChangedModeTime = millis();
}