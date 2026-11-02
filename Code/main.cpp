/*
  bag_tag v1 - basic bring-up firmware
  MCU:     nice!nano (nRF52840, Adafruit UF2 bootloader)
  Display: 128x64 I2C OLED (SH1106 by default, see USE_SH1106)
  Buttons: 3x Choc switches to GND (internal pull-ups, active LOW)

  - Sleeps in System OFF. Press WAKE (centre button) to boot and play.
  - CYCLE      -> next animation
  - INTERACT   -> placeholder (pause / resume) - edit onInteract()
  - Idle for IDLE_TIMEOUT_MS -> display off, back to System OFF.

  GPIO numbers below are the nRF52840 port/pin from your schematic, so the
  code does not depend on any board's Arduino pin numbering.
*/

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include "nrf_gpio.h"
#include "nrf_sdm.h"
#include "nrf_soc.h"

// ---------------- config ----------------
#define USE_SH1106  1        // 1.3" modules are usually SH1106. Set 0 for SSD1306 (0.96").
#define OLED_ADDR   0x3C

// From the schematic
#define GPIO_SDA     NRF_GPIO_PIN_MAP(0, 17)
#define GPIO_SCL     NRF_GPIO_PIN_MAP(0, 20)
#define BTN_WAKE     NRF_GPIO_PIN_MAP(0, 10)   // SW2 - assumed to be the centre switch
#define BTN_CYCLE    NRF_GPIO_PIN_MAP(1, 11)   // SW1
#define BTN_INTERACT NRF_GPIO_PIN_MAP(0, 9)    // SW3

#define IDLE_TIMEOUT_MS 30000UL
#define DEBOUNCE_MS     25
// ----------------------------------------

#if USE_SH1106
  #include <Adafruit_SH110X.h>
  Adafruit_SH1106G display(128, 64, &Wire, -1);
  #define COL_ON        SH110X_WHITE
  #define DISPLAY_INIT() display.begin(OLED_ADDR, true)
  #define DISPLAY_OFF()  display.oled_command(SH110X_DISPLAYOFF)
#else
  #include <Adafruit_SSD1306.h>
  Adafruit_SSD1306 display(128, 64, &Wire, -1);
  #define COL_ON        SSD1306_WHITE
  #define DISPLAY_INIT() display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)
  #define DISPLAY_OFF()  display.ssd1306_command(SSD1306_DISPLAYOFF)
#endif

#include "animations.h"

struct Button { uint32_t pin; bool down; uint32_t changedAt; };
static Button bWake     = {BTN_WAKE,     false, 0};
static Button bCycle    = {BTN_CYCLE,    false, 0};
static Button bInteract = {BTN_INTERACT, false, 0};

static int      curAnim = 0;
static int      curFrame = 0;
static bool     paused = false;
static uint32_t nextFrameAt = 0;
static uint32_t lastActivity = 0;

// Find the Arduino pin number that maps to a given nRF GPIO, whatever the board variant is.
static int arduinoPinFor(uint32_t gpio) {
  for (int i = 0; i < PINS_COUNT; i++) {
    if (g_ADigitalPinMap[i] == gpio) return i;
  }
  return -1;
}

static void buttonInit(Button &b) {
  nrf_gpio_cfg_input(b.pin, NRF_GPIO_PIN_PULLUP);
  b.down = (nrf_gpio_pin_read(b.pin) == 0);   // wake press shouldn't count as a new press
  b.changedAt = millis();
}

static bool buttonPressed(Button &b) {        // true once per debounced press
  bool down = (nrf_gpio_pin_read(b.pin) == 0);
  uint32_t now = millis();
  if (down != b.down && (now - b.changedAt) > DEBOUNCE_MS) {
    b.down = down;
    b.changedAt = now;
    return down;
  }
  return false;
}

static void drawFrame() {
  const Anim &a = ANIMS[curAnim];
  display.clearDisplay();
  display.drawBitmap(0, 0, a.frames[curFrame], 128, 64, COL_ON);
  display.display();
}

static void goToSleep() {
  DISPLAY_OFF();

  // Don't sleep while a button is held, it would wake us straight away.
  while (nrf_gpio_pin_read(BTN_WAKE) == 0 ||
         nrf_gpio_pin_read(BTN_CYCLE) == 0 ||
         nrf_gpio_pin_read(BTN_INTERACT) == 0) {
    delay(10);
  }
  delay(50);

  nrf_gpio_cfg_sense_input(BTN_WAKE, NRF_GPIO_PIN_PULLUP, NRF_GPIO_PIN_SENSE_LOW);

  uint8_t sdEnabled = 0;
  sd_softdevice_is_enabled(&sdEnabled);
  if (sdEnabled) sd_power_system_off();
  else           NRF_POWER->SYSTEMOFF = 1;
  while (true) { }                            // wake = reset -> setup()
}

// The spare button. Replace with whatever you like later.
static void onInteract() {
  paused = !paused;
}

static void halt(const char *msg) {           // visible over USB serial for debugging
  Serial.begin(115200);
  while (true) { Serial.println(msg); delay(1000); }
}

void setup() {
  buttonInit(bWake);
  buttonInit(bCycle);
  buttonInit(bInteract);

  int sda = arduinoPinFor(GPIO_SDA);
  int scl = arduinoPinFor(GPIO_SCL);
  if (sda < 0 || scl < 0) halt("ERROR: SDA/SCL (P0.17/P0.20) not in this board variant's pin map");
  Wire.setPins(sda, scl);                      // must come before Wire.begin() (display.begin does that)

  if (!DISPLAY_INIT()) halt("ERROR: OLED not found at I2C address (try 0x3D, or check wiring/switch S1)");
  Wire.setClock(400000);
  display.setContrast(180);

  curAnim = 0;
  curFrame = 0;
  lastActivity = nextFrameAt = millis();
}

void loop() {
  uint32_t now = millis();

  if (buttonPressed(bCycle)) {
    curAnim = (curAnim + 1) % ANIM_COUNT;
    curFrame = 0;
    paused = false;
    nextFrameAt = now;
    lastActivity = now;
  }
  if (buttonPressed(bInteract)) { onInteract(); lastActivity = now; }
  if (buttonPressed(bWake))     { lastActivity = now; }

  if (!paused && (int32_t)(now - nextFrameAt) >= 0) {
    drawFrame();
    nextFrameAt = now + ANIMS[curAnim].frameMs;
    curFrame = (curFrame + 1) % ANIMS[curAnim].count;
  }

  if ((now - lastActivity) > IDLE_TIMEOUT_MS) goToSleep();

  delay(2);
}
