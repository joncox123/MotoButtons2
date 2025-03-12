/*********************************************************************
License: GNU GENERAL PUBLIC LICENSE; Version 3, 29 June 2007
Version: 2.0 with support for the following modes: DMD2, mouse cursor, MyRoute App, media (music)
Device: Seeed XIAO nRF52840 (MotoButtons 2)
*********************************************************************/
#include <bluefruit.h>
#include <Adafruit_LittleFS.h>
#include <InternalFileSystem.h>
#include <Adafruit_TinyUSB.h>
#include <stdlib.h>

using namespace Adafruit_LittleFS_Namespace;

// Enable serial debugging (turn this off if not connected to PC)
#define DEBUG false

// How long to wait until DFU reset mode is activated
#define MODE_RESET_MS 10000

// Orientation of controller
#define DEFAULT_BUTTON_MAP 0
uint8_t buttonOrientation = DEFAULT_BUTTON_MAP;

// how long to deactivate physical button after virtual button is released
#define BUTTON_VIRTUAL_TIMEOUT 500 // ms

/*----- Persistent Storage Filesystem -----*/
// This is used to store settings, such as the last mode
#define FILENAME "/MotoButtons.set"
File file(InternalFS);

// BLE classes
BLEDis bledis;
BLEHidAdafruit blehid;

// BLE configuration
#define BLE_TX_POWER 8
const char BLE_DEVICE_NAME[] = "DMD2 CTL 8K";
const char BLE_DEVICE_MODEL[] = "MotoButtons 2.0";
const char BLE_MANUFACTURER[] = "Me";
bool BLE_connected = false;

// RGB LED colors plus off
typedef enum
{
  Red,     // virtual button activated
  Blue,    // BLE connected (flashing, BLE not connected)
  Green,   // mouse mode
  Yellow,  // DMD2 mode
  Cyan,    // MRA mode
  Magenta, // media mode
  White,   // regular key press
  Off,
} Color;

Color priorLEDState = Off;
Color LEDState = Off;

#define VIRT_BUTT_COLOR Red
#define BLE_COLOR Blue
#define MOUSE_MODE_COLOR Green
#define DMD2_MODE_COLOR Yellow
#define MRA_MODE_COLOR Cyan
#define MEDIA_MODE_COLOR Magenta
#define KEY_PRESS_COLOR White
#define POWER_ON_COLOR Red
#define SETUP_COMPLETE_COLOR White
#define BUTTON_ORIENTATION_COLOR Red

/*
 * --------------------- MODE CONFIGURATION ----------------------------
 * 	DMD2: up/down/left/right arrows, enter, F6 and F7
 * 	Mouse: mouse up/down/left/right, left click, left click, ?
 * 	MyRouteApp: up/down/left/right arrows, 'c', -, +, longpress center for 'n' (hold center for skip waypoint)
 *  Media (music): vol up, vol down, previous track, next track, mute, play/pause, stop
 */
// Selected Mode: indicates the currently selected operating mode
#define MODE_TOGGLE_MS 1000
#define N_MODES 4
typedef enum
{
  Mouse = 0,
  DMD2 = 1,
  MRA = 2,
  MEDIA = 3
} Mode;
Mode currentMode;

bool modeButtonsReleased = true;

// how long we must hold a physical button before the virtual function is activated
#define VIRT_BUTT_HOLD_DURATION 1000

/* DMD2 Mode Configuration */
// https://www.drivemodedashboard.com/controller-implementation-guide/
// https://arduino.stackexchange.com/questions/65513/how-do-i-send-non-ascii-keys-over-the-ble-hid-connection-using-an-adafruit-nrf52
// https://github.com/adafruit/Adafruit_nRF52_Arduino/blob/200b3aaefb3256ac26df82ebc9b5b58923d9c37c/cores/nRF5/Adafruit_TinyUSB_Core/tinyusb/src/class/hid/hid.h#L188
// https://github.com/adafruit/Adafruit_nRF52_Arduino/issues/785
// Key codes to send for button presses:
const uint8_t DMD_KEY_UP = HID_KEY_ARROW_UP;
const uint8_t DMD_KEY_DOWN = HID_KEY_ARROW_DOWN;
const uint8_t DMD_KEY_LEFT = HID_KEY_ARROW_LEFT;
const uint8_t DMD_KEY_RIGHT = HID_KEY_ARROW_RIGHT;
const uint8_t DMD_KEY_CENTER = HID_KEY_ENTER;
const uint8_t DMD_KEY_A = HID_KEY_F6;
const uint8_t DMD_KEY_B = HID_KEY_F7;
const uint8_t DMD_KEY_C = HID_KEY_ENTER;
const uint8_t DMD_KEY_VIRTUAL = HID_KEY_F5;

/* Mouse Mode Configuration */
#define MOUSE_RATE_SLOW 5
#define MOUSE_RATE_FAST 20
#define MOUSE_RATE_DELAY 500
bool mouse_left_button_pressed = false;
const uint8_t MOUSE_KEY_A = HID_KEY_BACKSPACE;
const uint8_t MOUSE_KEY_B = HID_KEY_ENTER;

/* MyRouteApp Mode Configuration */
const uint8_t MRA_KEY_UP = HID_KEY_ARROW_UP;
const uint8_t MRA_KEY_DOWN = HID_KEY_ARROW_DOWN;
const uint8_t MRA_KEY_LEFT = HID_KEY_ARROW_LEFT;
const uint8_t MRA_KEY_RIGHT = HID_KEY_ARROW_RIGHT;
const uint8_t MRA_KEY_CENTER = HID_KEY_C;     // operate compass
const uint8_t MRA_KEY_A = HID_KEY_KEYPAD_ADD; // zoom in
const uint8_t MRA_KEY_B = HID_KEY_MINUS;      // zoom out
const uint8_t MRA_KEY_C = HID_KEY_N;          // open menu
const uint8_t MRA_KEY_VIRTUAL = HID_KEY_D;    // start navigation

/* Media Mode Configuration */
const uint8_t MEDIA_KEY_UP = HID_USAGE_CONSUMER_VOLUME_INCREMENT;    // volume up
const uint8_t MEDIA_KEY_DOWN = HID_USAGE_CONSUMER_VOLUME_DECREMENT;  // volume down
const uint8_t MEDIA_KEY_LEFT = HID_USAGE_CONSUMER_SCAN_PREVIOUS;     // previous song
const uint8_t MEDIA_KEY_RIGHT = HID_USAGE_CONSUMER_SCAN_NEXT;        // next song
const uint8_t MEDIA_KEY_CENTER = HID_USAGE_CONSUMER_MUTE;            // Mute
const uint8_t MEDIA_KEY_A = HID_USAGE_CONSUMER_PLAY_PAUSE;           // play - pause
const uint8_t MEDIA_KEY_B = HID_USAGE_CONSUMER_STOP;                 // stop music
const uint8_t MEDIA_KEY_C = HID_USAGE_CONSUMER_BRIGHTNESS_INCREMENT; // increase brightness
const uint8_t MEDIA_KEY_VIRTUAL = HID_USAGE_CONSUMER_MUTE;           // Mute
/*---------------------- END MODE CONFIGURATION ----------------------*/

/*----------------- BUTTON CONFIGURATION AND LOGIC -------------------*/
/* BLE key report */
#define N_KEY_REPORT 6
uint8_t keyReport[N_KEY_REPORT] = {HID_KEY_NONE, HID_KEY_NONE, HID_KEY_NONE, HID_KEY_NONE, HID_KEY_NONE, HID_KEY_NONE};
bool keyReportChanged = false;
bool forceKeyReport = false; // used to force another key report for key up activation events

// Digital IO pin mapping (default)
uint8_t BUTTON_UP = 4;
uint8_t BUTTON_DOWN = 2;
uint8_t BUTTON_LEFT = 0;
uint8_t BUTTON_RIGHT = 3;
uint8_t BUTTON_CENTER = 1;
uint8_t BUTTON_A = 5;
uint8_t BUTTON_B = 6;
uint8_t BUTTON_C = 7;
uint8_t RGB_LED_RED = 8;
uint8_t RGB_LED_BLUE = 9;
uint8_t RGB_LED_GREEN = 10;

#define DEBOUNCE_TIME_MS 50

// state of buttons
bool button_up_state = false;
bool button_down_state = false;
bool button_left_state = false;
bool button_right_state = false;
bool button_center_state = false;
bool button_A_state = false;
bool button_B_state = false;
bool button_C_state = false;
bool button_virtual_state = false; // 8th virtual button activated via long press

// prior state of button reading for debouncing purposes
bool button_up_state_prior = false;
bool button_down_state_prior = false;
bool button_left_state_prior = false;
bool button_right_state_prior = false;
bool button_center_state_prior = false;
bool button_A_state_prior = false;
bool button_B_state_prior = false;
bool button_C_state_prior = false;

// Record state change of buttons to be used to monitor when event is transmitted
bool button_up_flipped = false;
bool button_down_flipped = false;
bool button_left_flipped = false;
bool button_right_flipped = false;
bool button_center_flipped = false;
bool button_A_flipped = false;
bool button_B_flipped = false;
bool button_C_flipped = false;
bool button_virtual_flipped = false;

// a flag to indicate that virtual button was released, for center timeout purposes only
bool button_virtual_timeout = false;

// last time that button transitioned from low to high
unsigned long button_up_time = 0;
unsigned long button_down_time = 0;
unsigned long button_left_time = 0;
unsigned long button_right_time = 0;
unsigned long button_center_time = 0;
unsigned long button_A_time = 0;
unsigned long button_B_time = 0;
unsigned long button_C_time = 0;
unsigned long button_virtual_time = 0;
/*------------------- END BUTTON CONFIG & LOGIC-----------------------*/

/*
  Set the color of the RGB LED to one of the 7 possibilities, plus off
  For a common anode(+) LED, LOW is ON and HIGH is OFF.
*/
void setRGBColor(Color color)
{
  priorLEDState = LEDState;

  switch (color)
  {
  case Red:
    LEDState = Red;
    digitalWrite(RGB_LED_RED, LOW);
    digitalWrite(RGB_LED_BLUE, HIGH);
    digitalWrite(RGB_LED_GREEN, HIGH);
    break;
  case Blue:
    LEDState = Blue;
    digitalWrite(RGB_LED_RED, HIGH);
    digitalWrite(RGB_LED_BLUE, LOW);
    digitalWrite(RGB_LED_GREEN, HIGH);
    break;
  case Green:
    LEDState = Green;
    digitalWrite(RGB_LED_RED, HIGH);
    digitalWrite(RGB_LED_BLUE, HIGH);
    digitalWrite(RGB_LED_GREEN, LOW);
    break;
  case Yellow:
    LEDState = Yellow;
    digitalWrite(RGB_LED_RED, LOW);
    digitalWrite(RGB_LED_BLUE, HIGH);
    digitalWrite(RGB_LED_GREEN, LOW);
    break;
  case Cyan:
    LEDState = Cyan;
    digitalWrite(RGB_LED_RED, HIGH);
    digitalWrite(RGB_LED_BLUE, LOW);
    digitalWrite(RGB_LED_GREEN, LOW);
    break;
  case Magenta:
    LEDState = Magenta;
    digitalWrite(RGB_LED_RED, LOW);
    digitalWrite(RGB_LED_BLUE, LOW);
    digitalWrite(RGB_LED_GREEN, HIGH);
    break;
  case White:
    LEDState = White;
    digitalWrite(RGB_LED_RED, LOW);
    digitalWrite(RGB_LED_BLUE, LOW);
    digitalWrite(RGB_LED_GREEN, LOW);
    break;
  case Off:
  default:
    LEDState = Off;
    digitalWrite(RGB_LED_RED, HIGH);
    digitalWrite(RGB_LED_BLUE, HIGH);
    digitalWrite(RGB_LED_GREEN, HIGH);
  }
}

void flashLED(Color color, uint16_t delayMs, uint16_t durationMs)
{
  uint8_t N = durationMs / delayMs;

  priorLEDState = LEDState;
  LEDState = color;

  for (uint8_t i = 0; i < 2 * (N + 1); i++)
  {
    if (i % 2 == 0)
      setRGBColor(Off);
    else
      setRGBColor(color);
    delay(delayMs / 2);
  }
}

void restoreLEDState()
{
  setRGBColor(priorLEDState);
  LEDState = priorLEDState;
}

void indicateMode(Mode mode)
{
  // Indicate the new mode
  switch (mode)
  {
  case Mouse:
    flashLED(MOUSE_MODE_COLOR, 1000, 200);
    setRGBColor(MOUSE_MODE_COLOR);
    break;
  case DMD2:
    flashLED(DMD2_MODE_COLOR, 1000, 200);
    setRGBColor(DMD2_MODE_COLOR);
    break;
  case MRA:
    flashLED(MRA_MODE_COLOR, 1000, 200);
    setRGBColor(MRA_MODE_COLOR);
    break;
  case MEDIA:
    flashLED(MEDIA_MODE_COLOR, 1000, 200);
    setRGBColor(MEDIA_MODE_COLOR);
    break;
  default:
    setRGBColor(Red);
  }
}

void showMode(Mode mode)
{
  // Indicate the new mode
  switch (mode)
  {
  case Mouse:
    setRGBColor(MOUSE_MODE_COLOR);
    break;
  case DMD2:
    setRGBColor(DMD2_MODE_COLOR);
    break;
  case MRA:
    setRGBColor(MRA_MODE_COLOR);
    break;
  case MEDIA:
    setRGBColor(MEDIA_MODE_COLOR);
    break;
  default:
    setRGBColor(Red);
  }
}

void RGBToggle(Color color)
{
  bool ledIsOn = !digitalRead(RGB_LED_RED) || !digitalRead(RGB_LED_BLUE) || !digitalRead(RGB_LED_GREEN);

  if (ledIsOn)
    setRGBColor(Off);
  else
    setRGBColor(color);
}

// cycle through all colors of the LED for demo purposes
void colorCycle(uint16_t N)
{
  for (uint32_t i = 0; i < N * 7; i++)
  {
    setRGBColor((Color)(i % 7));
    delay(500);
  }
}

// At startup, the user can hold down a joystick direction to select an orientation
// -1 indicates no valid selection was made
int getButtonMapSelection()
{
  // Read all four directions because we can only allow a mode switch if
  // one direction is pressed
  uint8_t up = digitalRead(BUTTON_UP);
  uint8_t down = digitalRead(BUTTON_DOWN);
  uint8_t left = digitalRead(BUTTON_LEFT);
  uint8_t right = digitalRead(BUTTON_RIGHT);

  if (up + down + left + right > 1)
  {
    // user held down more than one direction, return the default mapping
    return -1;
  }

  if (up)
    return 2;
  if (down)
    return 0;
  if (left)
    return 1;
  if (right)
    return 3;

  // no selection was made
  return -1;
}

/* Change the mapping of buttons based on a map specifier, buttMap
 * butMapp:
 * 	0: UP is GPIO pin 2
 *  1: UP is GPIO pin 3
 *  2: UP is GPIO pin 4
 *  3: UP is GPIO pin 0
 */
bool setButtonMapping(uint8_t buttMap)
{
  switch (buttMap)
  {
  case 0: // three buttons on top
    BUTTON_UP = 2;
    BUTTON_DOWN = 4;
    BUTTON_LEFT = 3;
    BUTTON_RIGHT = 0;
    BUTTON_CENTER = 1;
    BUTTON_A = 5;
    BUTTON_B = 6;
    BUTTON_C = 7;
    break;
  case 1: // three buttons on left
    BUTTON_UP = 0;
    BUTTON_DOWN = 3;
    BUTTON_LEFT = 2;
    BUTTON_RIGHT = 4;
    BUTTON_CENTER = 1;
    BUTTON_A = 5;
    BUTTON_B = 6;
    BUTTON_C = 7;
    break;
  case 2: // three buttons on bottom
    BUTTON_UP = 4;
    BUTTON_DOWN = 2;
    BUTTON_LEFT = 0;
    BUTTON_RIGHT = 3;
    BUTTON_CENTER = 1;
    BUTTON_A = 5;
    BUTTON_B = 6;
    BUTTON_C = 7;
    break;
  case 3: // three buttons toward right
    BUTTON_UP = 3;
    BUTTON_DOWN = 0;
    BUTTON_LEFT = 4;
    BUTTON_RIGHT = 2;
    BUTTON_CENTER = 1;
    BUTTON_A = 5;
    BUTTON_B = 6;
    BUTTON_C = 7;
    break;
  default:
    return true;
  }

  return false;
}

// This function returns true if the center button is in an active state
// if so, the program should ignore up/down/left/right on the joystick
bool isCenterActive()
{
  if (button_center_state)
    return true;
  if (digitalRead(BUTTON_CENTER))
    return true;

  return false;
}

bool isVirtualActive()
{
  if (millis() - button_virtual_time < BUTTON_VIRTUAL_TIMEOUT)
    return true;

  return button_virtual_state;
}

// if  This function should be called rapidly in a loop to update the debounce filter and key state
//  https://docs.arduino.cc/built-in-examples/digital/Debounce
bool debounceButton(unsigned int button, bool *state, bool *priorState, bool *buttonFlipped,
                    unsigned long *debounceTime, const char *buttonName)
{
  bool reading;
  unsigned long readTime;
  bool stateChanged = false;

  reading = digitalRead(button);
  readTime = millis();

  // If reading has changed, switch has not settled yet
  if (reading != *priorState)
    *debounceTime = readTime;
  // State has been stable for time exceeding debounce filter delay, so update state
  else if ((readTime - *debounceTime) > DEBOUNCE_TIME_MS)
    // If state changed after debounce, new state has not yet been transmitted
    if (reading != *state)
    {
      stateChanged = true;
      *state = reading;
      *buttonFlipped = true;
    }
  *priorState = reading;

  return stateChanged;
}

void releaseAllKeys()
{
  uint8_t keyReportRelease[N_KEY_REPORT] = {HID_KEY_NONE, HID_KEY_NONE, HID_KEY_NONE, HID_KEY_NONE, HID_KEY_NONE, HID_KEY_NONE};
  blehid.keyboardReport(0, keyReportRelease);
}

void updateButtons()
{
  bool stateChanged = false;

  // Read the state of all physical buttons
  stateChanged |= debounceButton(BUTTON_UP, &button_up_state, &button_up_state_prior, &button_up_flipped, &button_up_time, "UP");
  stateChanged |= debounceButton(BUTTON_DOWN, &button_down_state, &button_down_state_prior, &button_down_flipped, &button_down_time, "DOWN");
  stateChanged |= debounceButton(BUTTON_LEFT, &button_left_state, &button_left_state_prior, &button_left_flipped, &button_left_time, "LEFT");
  stateChanged |= debounceButton(BUTTON_RIGHT, &button_right_state, &button_right_state_prior, &button_right_flipped, &button_right_time, "RIGHT");
  stateChanged |= debounceButton(BUTTON_CENTER, &button_center_state, &button_center_state_prior, &button_center_flipped, &button_center_time, "CENTER");
  stateChanged |= debounceButton(BUTTON_A, &button_A_state, &button_A_state_prior, &button_A_flipped, &button_A_time, "A");
  stateChanged |= debounceButton(BUTTON_B, &button_B_state, &button_B_state_prior, &button_B_flipped, &button_B_time, "B");
  stateChanged |= debounceButton(BUTTON_C, &button_C_state, &button_C_state_prior, &button_C_flipped, &button_C_time, "C");

  /* ----------- Detect state of virtual press for Button C ---------------------*/
  if (button_C_state && (millis() - button_C_time > VIRT_BUTT_HOLD_DURATION))
  {
    // virtual button activated
    if (!button_virtual_state)
    {
      if (DEBUG)
        Serial.println("Virtual button C activated.");
      button_virtual_time = millis();
      button_virtual_flipped = true;
      stateChanged |= true;

      // indicate virtual button
      setRGBColor(VIRT_BUTT_COLOR);
    }
    button_virtual_state = true;
  }
  else if (button_virtual_state)
  {
    // virtual button is not active
    button_virtual_flipped = true;
    button_virtual_state = false;
    button_virtual_timeout = true;
    button_virtual_time = millis();
    if (DEBUG)
      Serial.println("Virtual button C deactivated, timeout enabled.");
    showMode(currentMode);
    stateChanged |= true;
  }

  // Also, a race condition can occur where the virtual button is released and the physical button flips from high to low, activating the physical button
  // Therefore, the release action of the virtual button is to disable the physical button for a period of time
  if (button_virtual_timeout)
    button_C_flipped = false;
  if (button_virtual_timeout && (millis() - button_virtual_time > BUTTON_VIRTUAL_TIMEOUT))
  {
    button_virtual_timeout = false;
    if (DEBUG)
      Serial.println("Virtual button C timeout disabled.");
  }

  /*------------------------------------------------------------------*/

  // Indicate whether any buttons changed state
  keyReportChanged = stateChanged;

  /* Hard reset and enter firmware udpate mode (DFU)
   * This mode is necessary because the bootloader in the Seed nRF52840 has a bug that prevents uploading new software
   * from the Arduino IDE if a BLE sketch is uploaded previously. Thus, it is necessary to enter via triggering a DFU reset event.
   */
  if (button_A_state && button_B_state && (millis() - button_A_time > MODE_RESET_MS) && (millis() - button_B_time > MODE_RESET_MS))
  {
    if (DEBUG)
      Serial.println("Resetting and entering firmware update (FDU) mode...");
    enterSerialDfu();
  }

  /*------------------- Handle mode cycling --------------------------*/
  // Check for release of mode cycle button combination
  if (!button_A_state || !button_B_state)
  {
    modeButtonsReleased = true;
  }

  if (button_A_state && button_B_state && (millis() - button_A_time > MODE_TOGGLE_MS) && (millis() - button_B_time > MODE_TOGGLE_MS) && modeButtonsReleased)
  {
    // Buttons A and B were both long-pressed, which means we should advance the mode
    currentMode = (Mode)(((int)currentMode + 1) % N_MODES);
    if (DEBUG)
    {
      Serial.print("Mode advanced to ");
      Serial.println(currentMode);
    }

    // Since mode has been changed, inactivate any ongoing key or mouse presses
    releaseAllKeys();
    blehid.mouseButtonRelease();
    modeButtonsReleased = false;

    // store new mode to flash memory
    writeSettings();

    indicateMode(currentMode);
  }
  /*------------------------------------------------------------------*/
}

void mapButtonsToKeyReport()
{
  unsigned int i = 0;

  bool centerActive = isCenterActive();
  bool virtualActive = isVirtualActive();

  switch (currentMode)
  {
  case DMD2:
    if (button_up_state && !centerActive)
    {
      keyReport[i] = DMD_KEY_UP;
      ++i;
    }
    if (button_down_state && !centerActive)
    {
      keyReport[i] = DMD_KEY_DOWN;
      ++i;
    }
    if (button_left_state && !centerActive)
    {
      keyReport[i] = DMD_KEY_LEFT;
      ++i;
    }
    if (button_right_state && !centerActive)
    {
      keyReport[i] = DMD_KEY_RIGHT;
      ++i;
    }
    if (!button_center_state && button_center_flipped)
    {
      button_center_flipped = false;
      forceKeyReport = true;
      keyReport[i] = DMD_KEY_CENTER;
      ++i;
    }
    if (!button_A_state && button_A_flipped && !button_B_state)
    {
      button_A_flipped = false;
      forceKeyReport = true;
      keyReport[i] = DMD_KEY_A;
      ++i;
    }
    if (!button_B_state && button_B_flipped && !button_A_state && (i < N_KEY_REPORT))
    {
      button_B_flipped = false;
      forceKeyReport = true;
      keyReport[i] = DMD_KEY_B;
      ++i;
    }
    if (!button_C_state && button_C_flipped && !virtualActive && (i < N_KEY_REPORT))
    {
      button_C_flipped = false;
      forceKeyReport = true;
      keyReport[i] = DMD_KEY_C;
      ++i;
    }
    if (!button_virtual_state && (i < N_KEY_REPORT) && button_virtual_flipped)
    {
      button_virtual_flipped = false;
      forceKeyReport = true;
      keyReport[i] = DMD_KEY_VIRTUAL;
      ++i;
    }
    break;

  case Mouse:
    if (!button_A_state && button_A_flipped)
    {
      button_A_flipped = false;
      forceKeyReport = true;
      keyReport[i] = MOUSE_KEY_A;
      ++i;
      
    }
    if (!button_B_state && button_B_flipped)
    {
      button_B_flipped = false;
      forceKeyReport = true;
      keyReport[i] = MOUSE_KEY_B;
      ++i;
    }
    break;

  case MRA:
    if (button_up_state && !centerActive)
    {
      if (DEBUG)
        Serial.println("MRA UP");
      keyReport[i] = MRA_KEY_UP;
      ++i;
    }
    if (button_down_state && !centerActive)
    {
      if (DEBUG)
        Serial.println("MRA DOWN");
      keyReport[i] = MRA_KEY_DOWN;
      ++i;
    }
    if (button_left_state && !centerActive)
    {
      if (DEBUG)
        Serial.println("MRA LEFT");
      keyReport[i] = MRA_KEY_LEFT;
      ++i;
    }
    if (button_right_state && !centerActive)
    {
      if (DEBUG)
        Serial.println("MRA RIGHT");
      keyReport[i] = MRA_KEY_RIGHT;
      ++i;
    }
    if (!button_center_state && button_center_flipped)
    {
      if (DEBUG)
        Serial.println("MRA CENTER");
      button_center_flipped = false;
      forceKeyReport = true;
      keyReport[i] = MRA_KEY_CENTER;
      ++i;
    }
    if (!button_A_state && button_A_flipped && !button_B_state)
    {
      if (DEBUG)
        Serial.println("MRA A");
      button_A_flipped = false;
      forceKeyReport = true;
      keyReport[i] = MRA_KEY_A;
      ++i;
    }
    if (!button_B_state && button_B_flipped && !button_A_state && (i < N_KEY_REPORT))
    {
      if (DEBUG)
        Serial.println("MRA B");
      button_B_flipped = false;
      forceKeyReport = true;
      keyReport[i] = MRA_KEY_B;
      ++i;
    }
    if (!button_C_state && button_C_flipped && !virtualActive && (i < N_KEY_REPORT))
    {
      if (DEBUG)
        Serial.println("MRA C");
      button_C_flipped = false;
      forceKeyReport = true;
      keyReport[i] = MRA_KEY_C;
      ++i;
    }
    if (!button_virtual_state && (i < N_KEY_REPORT) && button_virtual_flipped)
    {
      if (DEBUG)
        Serial.println("MRA VIRTUAL");
      button_virtual_flipped = false;
      forceKeyReport = true;
      keyReport[i] = MRA_KEY_VIRTUAL;
      ++i;
    }
    break;

  case MEDIA:
    // the media keys must be reported via a different ("consumer") function to work on iOS
    if (button_up_state && !centerActive)
    {
      if (DEBUG)
        Serial.println("Media key UP");
      blehid.consumerKeyPress(0, MEDIA_KEY_UP);
      blehid.consumerKeyRelease(0);
      ++i;
    }
    if (button_down_state && !centerActive)
    {
      if (DEBUG)
        Serial.println("Media key DOWN");
      blehid.consumerKeyPress(0, MEDIA_KEY_DOWN);
      blehid.consumerKeyRelease(0);
      ++i;
    }
    if (button_left_state && !centerActive)
    {
      if (DEBUG)
        Serial.println("Media key LEFT");
      blehid.consumerKeyPress(0, MEDIA_KEY_LEFT);
      blehid.consumerKeyRelease(0);
      ++i;
    }
    if (button_right_state && !centerActive)
    {
      if (DEBUG)
        Serial.println("Media key RIGHT");
      blehid.consumerKeyPress(0, MEDIA_KEY_RIGHT);
      blehid.consumerKeyRelease(0);
      ++i;
    }
    if (!button_center_state && button_center_flipped)
    {
      if (DEBUG)
        Serial.println("Media key CENTER");
      button_center_flipped = false;
      forceKeyReport = true;
      blehid.consumerKeyPress(0, MEDIA_KEY_CENTER);
      blehid.consumerKeyRelease(0);
      ++i;
    }
    if (!button_A_state && button_A_flipped && !button_B_state)
    {
      if (DEBUG)
        Serial.println("Media key A");
      button_A_flipped = false;
      forceKeyReport = true;
      blehid.consumerKeyPress(0, MEDIA_KEY_A);
      blehid.consumerKeyRelease(0);
      ++i;
    }
    if (!button_B_state && button_B_flipped && !button_A_state && (i < N_KEY_REPORT))
    {
      if (DEBUG)
        Serial.println("Media key B");
      button_B_flipped = false;
      forceKeyReport = true;
      blehid.consumerKeyPress(0, MEDIA_KEY_B);
      blehid.consumerKeyRelease(0);
      ++i;
    }
    if (!button_C_state && button_C_flipped && !virtualActive && (i < N_KEY_REPORT))
    {
      if (DEBUG)
        Serial.println("Media key C");
      button_C_flipped = false;
      forceKeyReport = true;
      blehid.consumerKeyPress(0, MEDIA_KEY_C);
      blehid.consumerKeyRelease(0);
      ++i;
    }
    if (!button_virtual_state && (i < N_KEY_REPORT) && button_virtual_flipped)
    {
      if (DEBUG)
        Serial.println("Media VIRTUAL");
      button_virtual_flipped = false;
      forceKeyReport = true;
      keyReport[i] = MEDIA_KEY_VIRTUAL;
      ++i;
    }
    break;

  default:
    break;
  }

  for (unsigned int j = i; j < N_KEY_REPORT; j++)
  {
    keyReport[j] = HID_KEY_NONE;
  }
}

void handleMouse()
{
  // handle LEFT click release
  if ((!button_center_state && button_center_flipped) || (!button_C_state && button_C_flipped) && mouse_left_button_pressed)
  {
    if (DEBUG)
      Serial.print("Mouse left button released.");
    blehid.mouseButtonRelease();
    button_center_flipped = false;
    button_C_flipped = false;
    mouse_left_button_pressed = false;
  }

  // Handle LEFT click press
  if ((button_center_state && button_center_flipped) || (button_C_state && button_C_flipped) && !mouse_left_button_pressed)
  {
    if (DEBUG)
      Serial.print("Mouse left button pressed.");
    blehid.mouseButtonPress(MOUSE_BUTTON_LEFT);
    button_center_flipped = false;
    button_C_flipped = false;
    mouse_left_button_pressed = true;
  }

  // Move pointer
  if (!isCenterActive())
  {
    int rate = MOUSE_RATE_SLOW;
    unsigned long currTimeMs = millis();
    if (((currTimeMs - button_up_time > MOUSE_RATE_DELAY) && button_up_state) ||
        ((currTimeMs - button_down_time > MOUSE_RATE_DELAY) && button_down_state) ||
        ((currTimeMs - button_left_time > MOUSE_RATE_DELAY) && button_left_state) ||
        ((currTimeMs - button_right_time > MOUSE_RATE_DELAY) && button_right_state))
      rate = MOUSE_RATE_FAST;
    else
      rate = MOUSE_RATE_SLOW;

    if (button_up_state)
      blehid.mouseMove(0, -rate);
    if (button_down_state)
      blehid.mouseMove(0, rate);
    if (button_left_state)
      blehid.mouseMove(-rate, 0);
    if (button_right_state)
      blehid.mouseMove(rate, 0);
  }
}

void setupDigitalIO()
{
  pinMode(BUTTON_UP, INPUT_PULLDOWN);
  pinMode(BUTTON_DOWN, INPUT_PULLDOWN);
  pinMode(BUTTON_LEFT, INPUT_PULLDOWN);
  pinMode(BUTTON_RIGHT, INPUT_PULLDOWN);
  pinMode(BUTTON_CENTER, INPUT_PULLDOWN);
  pinMode(BUTTON_A, INPUT_PULLDOWN);
  pinMode(BUTTON_B, INPUT_PULLDOWN);
  pinMode(BUTTON_C, INPUT_PULLDOWN);

  pinMode(RGB_LED_RED, OUTPUT);
  pinMode(RGB_LED_BLUE, OUTPUT);
  pinMode(RGB_LED_GREEN, OUTPUT);
}

bool writeSettings()
{
  char modeStr[8];
  char orientationStr[8];
  char settingsStr[16];

  // Settings file does not exist, we need to create it
  if (DEBUG)
    Serial.print("Creating " FILENAME " to store settings...");

  if (file.open(FILENAME, FILE_O_WRITE))
  {
    if (DEBUG)
      Serial.println("File opened successfully.");
    // file is opened in append mode by default
    file.truncate(0);
    file.seek(0);

    itoa((int)currentMode, modeStr, 10);
    itoa((int)buttonOrientation, orientationStr, 10);
    strcpy(settingsStr, modeStr);
    strcat(settingsStr, ",");
    strcat(settingsStr, orientationStr);

    if (DEBUG)
    {
      Serial.print("Writing to settings file: ");
      Serial.println(settingsStr);
    }

    file.write(settingsStr, strlen(settingsStr));
    file.close();

    return true;
  }
  else
  {
    if (DEBUG)
      Serial.println("Error writing settings file!");

    return false;
  }
}

// return true for error
bool readSettings()
{
  file.open(FILENAME, FILE_O_READ);
  // Does settings file already exist?
  if (file)
  {
    // File already exists
    if (DEBUG)
      Serial.println(FILENAME " settings file exists, reading...");

    uint32_t readlen;
    char buffer[16] = {0};
    readlen = file.read(buffer, sizeof(buffer));

    buffer[readlen] = 0;
    if (DEBUG)
    {
      Serial.print("Read from settings: ");
      Serial.println(buffer);
    }
    file.close();

    // split the settings from a common separated list of values to individual variables
    char *token = strtok(buffer, ",");
    if (token)
    {
      // first value is the app mode
      int n = atoi(token);
      if (n < 0 || (n > N_MODES - 1))
      {
        if (DEBUG)
          Serial.println("Invalid mode value, setting to zero.");
        n = 0;
        return true; // error
      }
      currentMode = (Mode)n;
      token = strtok(NULL, " ");
    }
    if (token)
    {
      // second value is the device orientation
      int n = atoi(token);
      if (n < 0 || n > 3)
      {
        if (DEBUG)
          Serial.println("Invalid orientation value, setting to default.");
        n = DEFAULT_BUTTON_MAP;
        return true; // error
      }
      buttonOrientation = (uint8_t)n;
      setButtonMapping(buttonOrientation);
    }

    return false; // no error
  }
  else
  {
    if (DEBUG)
      Serial.println(FILENAME " settings file not found.");

    return true;
  }
}

void setup()
{
  currentMode = Mouse;

  setupDigitalIO();
  setRGBColor(POWER_ON_COLOR);

  if (DEBUG)
  {
    Serial.begin(115200);
    while (!Serial)
      delay(10); // for nrf52840 with native usb

    Serial.println("MotoButtons 2 BLE Controller");
    Serial.println("-----------------------------\n");
    Serial.println();
  }

  Bluefruit.begin();
  // HID Device can have a min connection interval of 9*1.25 = 11.25 ms
  Bluefruit.Periph.setConnInterval(9, 16); // min = 9*1.25=11.25 ms, max = 16*1.25=20ms
  Bluefruit.setTxPower(BLE_TX_POWER);      // Check bluefruit.h for supported values
  Bluefruit.setName(BLE_DEVICE_NAME);

  // Configure and Start Device Information Service
  bledis.setManufacturer(BLE_MANUFACTURER);
  bledis.setModel(BLE_DEVICE_MODEL);
  bledis.begin();

  // BLE HID
  blehid.begin();

  // Set up and start advertising
  startBLEAdvertise();

  if (DEBUG)
    Serial.println("Setup complete.");
  setRGBColor(SETUP_COMPLETE_COLOR);

  // Initialize Internal File System
  InternalFS.begin();

  // let the user change the orientation of the device at startup
  int buttonMap = getButtonMapSelection();
  if (buttonMap >= 0)
  {
    buttonOrientation = (uint8_t)buttonMap;
    setButtonMapping(buttonOrientation);

    if (DEBUG)
    {
      Serial.print("Button orientation changed to: ");
      Serial.println(buttonOrientation);
    }

    // Indicate new button mode selection
    flashLED(BUTTON_ORIENTATION_COLOR, 250, 250 * (((uint8_t)buttonOrientation) + 1));
  }
  else
    Serial.println("Button orientation not changed.");

  // Restore settings
  bool success = false;
  success = readSettings();
  // new selection for buttonMap/orientation gets overwritten by readSettings
  if (buttonMap >= 0)
  {
    buttonOrientation = (uint8_t)buttonMap;
    setButtonMapping(buttonOrientation);
  }
  if (!success || buttonMap >= 0)
    writeSettings();

  indicateMode(currentMode);
}

void startBLEAdvertise(void)
{
  // Advertising packet
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();
  // Bluefruit.Advertising.addAppearance(BLE_APPEARANCE_HID_MOUSE);
  Bluefruit.Advertising.addAppearance(BLE_APPEARANCE_HID_KEYBOARD);

  // Include BLE HID service
  Bluefruit.Advertising.addService(blehid);

  // There is enough room for 'Name' in the advertising packet
  Bluefruit.Advertising.addName();

  /* Start Advertising
   * - Enable auto advertising if disconnected
   * - Interval:  fast mode = 20 ms, slow mode = 152.5 ms
   * - Timeout for fast mode is 30 seconds
   * - Start(timeout) with timeout = 0 will advertise forever (until connected)
   *
   * For recommended advertising interval
   * https://developer.apple.com/library/content/qa/qa1931/_index.html
   */
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(32, 244); // in unit of 0.625 ms
  Bluefruit.Advertising.setFastTimeout(30);   // number of seconds in fast mode
  Bluefruit.Advertising.start(0);             // 0 = Don't stop advertising after n seconds
}

void loop()
{
  // Indicate whether the device is connected and running
  if (!BLE_connected && (Bluefruit.connected() > 0))
  {
    if (DEBUG)
      Serial.println("BLE connected to host.");
    showMode(currentMode);
    BLE_connected = true;
  }
  else if (Bluefruit.connected() == 0)
  {
    RGBToggle(BLE_COLOR);
    delay(200);
    BLE_connected = false;
  }

  // Read current state of buttons
  updateButtons();

  if (BLE_connected)
  {
    // Compile the BLE HID key report
    if (keyReportChanged || forceKeyReport)
    {
      if (forceKeyReport) {
        forceKeyReport = false;
        if (DEBUG)
          Serial.println("Key report forced.");
      }
      mapButtonsToKeyReport();
      blehid.keyboardReport(0, keyReport);
      if (DEBUG)
        Serial.println("Key report sent.");
    }

    if (currentMode == Mouse)
      handleMouse();
  }
}
