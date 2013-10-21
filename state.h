/*
 * The pins which go to ground via a 5-way switch.
 * They are expected to go to ground
 * @see https://www.sparkfun.com/products/10541
 */
#define MODE_PIN1 2
#define MODE_PIN2 3
#define MODE_PIN3 4

// This determines if we are in interval mode (by going to ground)
#define INTERVAL_PIN 5

// This determines if we go into bulb mode setup (by going to ground)
#define ADJUST_BULB_PIN 6

// This determines if we go into interval mode setup (by going to ground)
#define ADJUST_INTER_PIN 7

/*
 * The pins the rotary encoder is attached to (ground switched)
 * @see https://www.sparkfun.com/products/10982
 */
#define ENCODER_PIN1 8
#define ENCODER_PIN2 9
#define ENCODER_PORT PINB

/*
 * An output pin (going high) when the shutter button can be pressed
 * Useful for a lighted shutter button.
 */
#define SHUTTER_ENABLE_PIN 11

/*
 * An output pin (going high) when the shutter is going to be released
 */
#define SHUTTER_RELEASE_OUT_PIN 12

/*
 * An input pin (going high) when the shutter should be released
 */
#define SHUTTER_RELEASE_IN_PIN 13

/* 
 * The output pin for controlling the IR led for communicating with the camera
 */
#define IR_OUT_PIN A3

/*
 * Three LED outputs (going high) for displaying the shutter status 
 * BLUE == NOTREADY
 * GREEN == READY
 * READY == OPEN
 */
#define STATUS_BLUE A2
#define STATUS_GREEN A1
#define STATUS_RED A0

/*
 * The addresses of the I2C 7-segment displays for displaying shutter_time and interval_time
 * @see https://www.sparkfun.com/products/11441
 */
#define SHUTTER_DISP_ADDR 0x70
#define INTERVAL_DISP_ADDR 0x71

// ENDCONFIG
// ---DO NOT CHANGE ANYTHING BELOW THIS LINE---

/*
 * The 5 basic modes of the intervalometer
 */
enum modes {
  MODE_SINGLE, // Take a single picture with a single shutter trigger
  MODE_DOUBLE, // Take two pictures, with 2 triggers spaced 1 second apart
  MODE_BULB, // Open the shutter on button down, and close it on button up.
  MODE_BULB_TIMER, // Open the shutter for the specified time (stored in state.shutter_time)
  MODE_BULB_BRACKET // Take a series of photos, starting with 1 second shutter time, doubling until state.shutter_time is reached
};

/*
 * The 3 basic states of the device (which are mode agnostic)
 */
enum shutter_state {
  SHUTTER_NOT_READY, // This means the system is in "setup" mode, and is not ready to take a picture
  SHUTTER_READY, // This means that pressing the shutter button will cause a picture to be taken
  SHUTTER_OPEN // This means that the device is in shoot mode, and cannot enter setup
};

/*
 * A single structure which represents all of the state of the device.
 */
struct state {
  modes mode; // The currently selected mode
  shutter_state shutter; // The current shutter state
  bool interval; // Is interval shooting enabled?
  unsigned long shutter_time; // The current bulb mode shutter time
  unsigned long interval_time; // The current interval time
};

void triggerShutter();
void shutterBulb(unsigned long seconds);
void sendShutterCommand();
void sendShutterOpen();
void sendShutterClose();
void redrawDials(boolean isBulb, unsigned long multiplier);
void updateState();
void updateDisplay();
void updateAdjust();
void updateInterval();
void updateMode();
int8_t read_encoder();
void i2cSendString(char *toSend, byte addr);
