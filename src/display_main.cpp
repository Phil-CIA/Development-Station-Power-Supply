// display_main.cpp — Front display board bring-up
// Target: ESP32-C6 (front-display-board rev0)
// Pins: docs/GPIO_PINOUT.md
// Library: Arduino_GFX — TFT_eSPI cannot compile on ESP32-C6 (VSPI undefined)
//
// Init approach mirrors TFT_eSPI's verified working ST7796 sequence:
//   hardware reset → SWRESET (0x01) → SLPOUT (0x11) → full init → DISPON
//
// On first flash this runs a color cycle then shows a "Ready" banner.

#include <Arduino.h>
#include <Arduino_GFX_Library.h>

// ── Pins ──────────────────────────────────────────────────────────────────────
constexpr uint8_t BL_PIN         = 20;
constexpr uint8_t TFT_CS_PIN     = 10;
constexpr uint8_t TFT_DC_PIN     = 18;
constexpr uint8_t TFT_RST_PIN    = 19;
constexpr uint8_t TFT_SCLK_PIN   = 12;
constexpr uint8_t TFT_MOSI_PIN   = 13;
constexpr uint8_t TFT_MISO_PIN   = 11;
constexpr uint8_t HOST_REQ_PIN   = 0;
constexpr uint8_t DISP_READY_PIN = 1;

constexpr uint16_t COLOR_BLACK = 0x0000;
constexpr uint16_t COLOR_RED   = 0xF800;
constexpr uint16_t COLOR_GREEN = 0x07E0;
constexpr uint16_t COLOR_BLUE  = 0x001F;
constexpr uint16_t COLOR_CYAN  = 0x07FF;
constexpr uint16_t COLOR_WHITE = 0xFFFF;

#define PANEL_CTRL_ST7796  1
#define PANEL_CTRL_ILI9488 2
#define PANEL_CTRL_ILI9486 3

#ifndef PANEL_CONTROLLER
#define PANEL_CONTROLLER PANEL_CTRL_ST7796
#endif

// GFX_NOT_DEFINED for RST so begin() does NOT hardware-reset (we manage it)
static Arduino_DataBus* tftBus = new Arduino_SWSPI(
    TFT_DC_PIN, TFT_CS_PIN, TFT_SCLK_PIN, TFT_MOSI_PIN, TFT_MISO_PIN);

#if PANEL_CONTROLLER == PANEL_CTRL_ST7796
static const char* kPanelControllerName = "ST7796";
static Arduino_GFX* tft = new Arduino_ST7796(tftBus, GFX_NOT_DEFINED, 0, false);
#elif PANEL_CONTROLLER == PANEL_CTRL_ILI9488
static const char* kPanelControllerName = "ILI9488";
static Arduino_GFX* tft = new Arduino_ILI9488(tftBus, GFX_NOT_DEFINED, 0, false);
#elif PANEL_CONTROLLER == PANEL_CTRL_ILI9486
static const char* kPanelControllerName = "ILI9486";
static Arduino_GFX* tft = new Arduino_ILI9486(tftBus, GFX_NOT_DEFINED, 0, false);
#else
#error Unsupported PANEL_CONTROLLER value
#endif

// ── Helpers ───────────────────────────────────────────────────────────────────

static void setBacklight(bool on) {
    pinMode(BL_PIN, OUTPUT);
    digitalWrite(BL_PIN, on ? HIGH : LOW);
}

static void initHandshake() {
    pinMode(DISP_READY_PIN, OUTPUT);
    digitalWrite(DISP_READY_PIN, LOW);
    pinMode(HOST_REQ_PIN, INPUT_PULLDOWN);
}

// Hardware reset: mirrors TFT_eSPI's pre-init pulse
static void hardwareReset() {
    pinMode(TFT_RST_PIN, OUTPUT);
    digitalWrite(TFT_RST_PIN, HIGH); delay(20);
    digitalWrite(TFT_RST_PIN, LOW);  delay(120);
    digitalWrite(TFT_RST_PIN, HIGH); delay(120);
    Serial.println("[tft] hardware reset done");
}

// Send SWRESET via bus, matching TFT_eSPI's ST7796 init (it does SWRESET first)
static void sendSwReset() {
    tftBus->begin(1000000);
    tftBus->beginWrite();
    tftBus->writeCommand(0x01);  // SWRESET
    tftBus->endWrite();
    delay(150);
    Serial.println("[tft] SWRESET sent");
}

static void writeReg8(uint8_t cmd, uint8_t data) {
    tftBus->beginWrite();
    tftBus->writeCommand(cmd);
    tftBus->write(data);
    tftBus->endWrite();
}

static void writeRegN(uint8_t cmd, const uint8_t* data, size_t len) {
    tftBus->beginWrite();
    tftBus->writeCommand(cmd);
    for (size_t i = 0; i < len; ++i) {
        tftBus->write(data[i]);
    }
    tftBus->endWrite();
}

// Reinforce panel drive settings after begin(). This is based on common ST7796S
// bring-up sequences where faint/washed output improves after explicit power,
// gamma, and inversion registers are rewritten.
static void reinforceSt7796Drive() {
    const uint8_t b6[] = {0x80, 0x22, 0x3B};
    const uint8_t e8[] = {0x40, 0x8A, 0x00, 0x00, 0x29, 0x19, 0xA5, 0x33};
    const uint8_t e0[] = {0xF0, 0x09, 0x0B, 0x06, 0x04, 0x15, 0x2F, 0x54, 0x42, 0x3C, 0x17, 0x14, 0x18, 0x1B};
    const uint8_t e1[] = {0xE0, 0x09, 0x0B, 0x06, 0x04, 0x03, 0x2B, 0x43, 0x42, 0x3B, 0x16, 0x14, 0x17, 0x1B};

    writeReg8(0xF0, 0xC3);
    writeReg8(0xF0, 0x96);
    writeReg8(0x3A, 0x55);
    writeReg8(0xB4, 0x01);
    writeRegN(0xB6, b6, sizeof(b6));
    writeRegN(0xE8, e8, sizeof(e8));
    writeReg8(0xC1, 0x06);
    writeReg8(0xC2, 0xA7);
    writeReg8(0xC5, 0x18);
    writeRegN(0xE0, e0, sizeof(e0));
    writeRegN(0xE1, e1, sizeof(e1));
    writeReg8(0x36, 0x48); // common ST7796 rotation/color order baseline
    writeReg8(0x21, 0x00); // INVON
    writeReg8(0xF0, 0x3C);
    writeReg8(0xF0, 0x69);
    writeReg8(0x29, 0x00); // DISPON
    delay(120);

    Serial.println("[tft] ST7796 drive/gamma reinforced");
}

// ── Color test ────────────────────────────────────────────────────────────────

static void runColorTest() {
    static const struct { uint16_t c; const char* n; } steps[] = {
        { COLOR_RED,   "RED"   },
        { COLOR_GREEN, "GREEN" },
        { COLOR_BLUE,  "BLUE"  },
        { COLOR_WHITE, "WHITE" },
        { COLOR_BLACK, "BLACK" },
    };
    Serial.println("[tft] color test");
    for (const auto& s : steps) {
        tft->fillScreen(s.c);
        Serial.printf("[tft] %s\n", s.n);
        delay(400);
    }
    Serial.println("[tft] color test done");
}

// ── Setup ─────────────────────────────────────────────────────────────────────

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.printf("[display] boot - Arduino_GFX %s probe\n", kPanelControllerName);

    initHandshake();
    setBacklight(false);
    Serial.println("[tft] backlight OFF during init");

    // Step 1: hardware reset (active-low pulse on GPIO19)
    hardwareReset();
    // Step 2: software reset — TFT_eSPI does this at the top of its ST7796 init
    sendSwReset();
    // Step 3: full init via Arduino_GFX (RST=GFX_NOT_DEFINED so it won't reset again)
    Serial.println("[tft] init...");
    tft->begin(1000000);
    tft->setRotation(1);
    tft->fillScreen(COLOR_BLACK);
    tft->setTextColor(COLOR_WHITE);
    tft->setTextSize(2);
    tft->setCursor(8, 8);
    tft->println("Display board OK");

#if PANEL_CONTROLLER == PANEL_CTRL_ST7796
    reinforceSt7796Drive();
#endif

    Serial.println("[tft] init done");

    setBacklight(true);
    Serial.println("[tft] backlight ON after init");

    runColorTest();

    tft->fillScreen(COLOR_BLACK);
    tft->setTextColor(COLOR_CYAN);
    tft->setCursor(8, 8);
    tft->println("Ready - waiting for host");

    digitalWrite(DISP_READY_PIN, HIGH);
    Serial.println("[display] DISP_READY HIGH — init complete");
}

// ── Loop ──────────────────────────────────────────────────────────────────────

void loop() {
    // Host board not built yet — nothing to do.
    // When HOST_REQ goes HIGH, de-assert DISP_READY, service transaction,
    // then re-assert.
    delay(1000);
}
