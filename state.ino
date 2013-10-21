#include <multiCameraIrControl.h>
#include "state.h"
#include <Wire.h>

// A few helper macros
#define IS_LOW(pin) !IS_HIGH(pin)
#define IS_HIGH(pin) ((pin) == HIGH)
#define SETUP_INPUT_PIN(pin) pinMode((pin), INPUT_PULLUP)
#define SETUP_OUTPUT_PIN(pin) pinMode((pin), OUTPUT)
#define RESET_DISPLAY(addr) { Wire.beginTransmission(addr); Wire.write(0x76); Wire.endTransmission(); }

/*
 * Setup the global variable for communicating via IR to a Sony NEX-5 Camera
 */
Sony Nex(IR_OUT_PIN);

/*
 * Initialize the current state global variable to the defaults
 */
state current_state = {
  MODE_SINGLE,
  SHUTTER_READY,
  false,
  0,
  0
};

void setup() {
  // Setup the input pins to PULLUP mode.
  SETUP_INPUT_PIN(MODE_PIN1);
  SETUP_INPUT_PIN(MODE_PIN2);
  SETUP_INPUT_PIN(MODE_PIN3);
  SETUP_INPUT_PIN(INTERVAL_PIN);
  SETUP_INPUT_PIN(ADJUST_BULB_PIN);
  SETUP_INPUT_PIN(ADJUST_INTER_PIN);
  SETUP_INPUT_PIN(ENCODER_PIN1);
  SETUP_INPUT_PIN(ENCODER_PIN2);
  // The shutter release pin is pull DOWN, so needs to be configured separately
  pinMode(SHUTTER_RELEASE_IN_PIN, INPUT);
  
  // Setup the output pins
  SETUP_OUTPUT_PIN(SHUTTER_RELEASE_OUT_PIN);
  SETUP_OUTPUT_PIN(SHUTTER_ENABLE_PIN);
  SETUP_OUTPUT_PIN(STATUS_BLUE);
  SETUP_OUTPUT_PIN(STATUS_GREEN);
  SETUP_OUTPUT_PIN(STATUS_RED);
  SETUP_OUTPUT_PIN(IR_OUT_PIN);
  
  // Reset the 7-Segment displays, and send the default values.
  Wire.begin();
  RESET_DISPLAY(SHUTTER_DISP_ADDR);
  RESET_DISPLAY(INTERVAL_DISP_ADDR);
  redrawDials(false, 1);
}

void loop() {
  int shutter_trigger = digitalRead(SHUTTER_RELEASE_IN_PIN);
  // Read the inputs to update the state
  updateState();
  if (current_state.shutter == SHUTTER_READY && IS_HIGH(shutter_trigger)) {
    // If the shutter is ready, and our shutter trigger input is high, then trigger the shutter!
    triggerShutter();
  }  
}

void triggerShutter() {
  unsigned long time;
  char tempString[5];
  do {
    current_state.shutter = SHUTTER_OPEN;
    updateDisplay(); // We want to tell the user that the shutter is open...
    switch (current_state.mode) {
      case MODE_DOUBLE:
        sendShutterCommand();
        delay(1000);
        // Fallthrough intentional
      case MODE_SINGLE:
        sendShutterCommand();
        break;
      case MODE_BULB:
        sendShutterOpen();
        while (IS_HIGH(digitalRead(SHUTTER_RELEASE_IN_PIN))) {
          // Delay only 10 milliseconds, to detect the button up as soon as practical
          delay(10);
        }
        sendShutterClose();
        break;
      case MODE_BULB_TIMER:
        shutterBulb(current_state.shutter_time);
        break;
      case MODE_BULB_BRACKET: {
        unsigned long i;
        for (i = 1; i <= current_state.shutter_time; i <<= 1) {
          // Take a bulb shot with doubling shutter times (i <<= 1 will double i)
          shutterBulb((unsigned long) (1000 * i));
          delay(1000);
        }
      } break;
    }
    time = millis() / 1000;
    // While the interval mode is enabled, and the interval is not up...
    while (current_state.interval && current_state.interval_time > 0 && time + current_state.interval_time > millis() / 1000) {
      updateState();
      sprintf(tempString, "%4d", current_state.interval_time - ((millis() / 1000) - time));
      // Display the time remaining on the interval display
      i2cSendString(tempString, INTERVAL_DISP_ADDR);
      delay(10);
    }
    // Redraw the dials properly
    redrawDials(false, 1);
  } while (current_state.interval && current_state.interval_time > 0);
  // The interval is done, reset the shutter to READY state
  current_state.shutter = SHUTTER_READY;
  updateDisplay();
}

void shutterBulb(unsigned long seconds) {
  unsigned long time;
  char tempString[5];
  sendShutterOpen();
  time = (millis() / 1000);
  while (time + seconds > millis() / 1000) {
    sprintf(tempString, "%4d", seconds - ((millis() / 1000) - time));
    // Display the time remaining on the shutter display
    i2cSendString(tempString, SHUTTER_DISP_ADDR);
    delay(500);
  }
  sendShutterClose();
  // Redraw the display)
  sprintf(tempString, "%4d", current_state.shutter_time);
  i2cSendString(tempString, SHUTTER_DISP_ADDR);
}

void sendShutterCommand() {
  digitalWrite(SHUTTER_RELEASE_OUT_PIN, HIGH);
  Nex.shutterNow();
  digitalWrite(SHUTTER_RELEASE_OUT_PIN, LOW);
}

void sendShutterOpen() {
  digitalWrite(SHUTTER_RELEASE_OUT_PIN, HIGH);
  Nex.shutterNow();
}

void sendShutterClose() {
  digitalWrite(SHUTTER_RELEASE_OUT_PIN, LOW);
  Nex.shutterNow();
}

void redrawDials(boolean isBulb, unsigned long multiplier) {
  char tempString[5];
  sprintf(tempString, "%4d", current_state.shutter_time);
  i2cSendString(tempString, SHUTTER_DISP_ADDR);
  sprintf(tempString, "%4d", current_state.interval_time);
  i2cSendString(tempString, INTERVAL_DISP_ADDR);
}

void updateState() {
  updateMode();
  updateInterval();
  if (current_state.shutter != SHUTTER_OPEN) {
    // We can only enter setup if the shutter is not open
    updateAdjust();
  }
  updateDisplay();
}

void updateDisplay() {
  digitalWrite(STATUS_GREEN, LOW);
  digitalWrite(STATUS_BLUE, LOW);
  digitalWrite(STATUS_RED, LOW);
  digitalWrite(SHUTTER_ENABLE_PIN, LOW);
  if (current_state.shutter == SHUTTER_READY) {
    digitalWrite(STATUS_GREEN, HIGH);
    digitalWrite(SHUTTER_ENABLE_PIN, HIGH);
  } else if (current_state.shutter == SHUTTER_OPEN) {
    digitalWrite(STATUS_RED, HIGH);
  } else if (current_state.shutter == SHUTTER_NOT_READY) {
    digitalWrite(STATUS_BLUE, HIGH);
  }
}

void updateAdjust() {
  int bulbPin, interPin;
  unsigned long current_value;
  int8_t diff;
  bulbPin = digitalRead(ADJUST_BULB_PIN);
  interPin = digitalRead(ADJUST_INTER_PIN);
  // Check if we are either in bulb or interval setup mode
  while (IS_LOW(bulbPin) || IS_LOW(interPin)) {
    // Prevent shutter release at this state
    current_state.shutter = SHUTTER_NOT_READY;
    if (IS_LOW(bulbPin)) {
      current_value = current_state.shutter_time;
    } else {
      current_value = current_state.interval_time;
    }
    diff = read_encoder();
    if (diff != -1) {
      current_value += ((unsigned long) diff);
    } else if (1 >= current_value) {
      // The value is too low to be reduced, so set it to be 0
      current_value = 0;
    } else {
      current_value -= 1;
    }
    if (IS_LOW(bulbPin)) {
      current_state.shutter_time = current_value;
    } else {
      current_state.interval_time = current_value;
    }
    // Update and redraw the dials!
    updateDisplay();
    redrawDials(IS_LOW(bulbPin), 1);
    bulbPin = digitalRead(ADJUST_BULB_PIN);
    interPin = digitalRead(ADJUST_INTER_PIN);
  }
  current_state.shutter = SHUTTER_READY;
}

void updateInterval() {
  int pin = digitalRead(INTERVAL_PIN);
  current_state.interval = IS_LOW(pin);
}

void updateMode() {
  int pin1 = digitalRead(MODE_PIN1);
  int pin2 = digitalRead(MODE_PIN2);
  int pin3 = digitalRead(MODE_PIN3);
  // This is wired so that the 5 pins on the selector switch can be read. See: https://www.sparkfun.com/products/10541
  if (IS_LOW(pin1) && IS_HIGH(pin2) && IS_HIGH(pin3)) {
    current_state.mode = MODE_BULB_BRACKET;
  } else if (IS_LOW(pin1) && IS_LOW(pin2) && IS_HIGH(pin3)) {
    current_state.mode = MODE_BULB_TIMER;
  } else if (IS_HIGH(pin1) && IS_LOW(pin2) && IS_HIGH(pin3)) {
    current_state.mode = MODE_BULB;
  } else if (IS_HIGH(pin1) && IS_LOW(pin2) && IS_LOW(pin3)) {
    current_state.mode = MODE_DOUBLE;
  } else if (IS_HIGH(pin1) && IS_HIGH(pin2) && IS_LOW(pin3)) {
    current_state.mode = MODE_SINGLE;
  }
}

/*
 * Read the encoder. 
 * @see http://www.circuitsathome.com/mcu/programming/reading-rotary-encoder-on-arduino
 */
int8_t read_encoder() {
  static int8_t enc_states[] = {0,-1,1,0,1,0,0,-1,-1,0,0,1,0,1,-1,0};
  static uint8_t old_AB = 0;
  old_AB <<= 2;
  old_AB |= ( ENCODER_PORT & 0x03 );
  return ( enc_states[( old_AB & 0x0f )] );
}

/*
 * Send a 4 byte character array to the 7 segment display identified by addr
 */
void i2cSendString(char *toSend, byte addr) {
  Wire.beginTransmission(addr);
  for(byte x = 0; x < 4; x++) {
    Wire.write(toSend[x]);
  }
  Wire.endTransmission();
}
