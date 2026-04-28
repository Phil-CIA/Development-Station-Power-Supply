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
#include <SPI.h>

// ── Diagnostics ──────────────────────────────────────────────────────────────
// Polarity diagnostic flags (only one should be enabled):
// DIAG_INVERT_CS=1     → invert CS polarity (CS=HIGH for active, CS=LOW for idle)
// DIAG_INVERT_DC=1     → invert DC polarity (DC=LOW for data, DC=HIGH for command)
// DIAG_SWAP_BYTES=1    → swap byte order in 16-bit color writes (lo byte first, then hi)
#ifndef DIAG_INVERT_CS
#define DIAG_INVERT_CS 0
#endif
#ifndef DIAG_INVERT_DC
#define DIAG_INVERT_DC 0
#endif
#ifndef DIAG_SWAP_BYTES
#define DIAG_SWAP_BYTES 0
#endif

// ── Pins ──────────────────────────────────────────────────────────────────────
// Pin mapping can be selected at compile time:
// USE_DEVBOARD_PINS=1  → legacy dev-board mapping (CS=4, DC=16, RST=17, etc)
// USE_DEVBOARD_PINS=0  → PCB schematic mapping (CS=10, DC=18, RST=19, etc)
#ifndef USE_DEVBOARD_PINS
#define USE_DEVBOARD_PINS 0
#endif

constexpr uint8_t BL_PIN         = 20;
#if USE_DEVBOARD_PINS
// Legacy dev-board pins (known to work on direct ESP32 wiring)
constexpr uint8_t TFT_CS_PIN     = 4;
constexpr uint8_t TFT_DC_PIN     = 16;
constexpr uint8_t TFT_RST_PIN    = 17;
constexpr uint8_t TFT_SCLK_PIN   = 18;
constexpr uint8_t TFT_MOSI_PIN   = 23;
constexpr uint8_t TFT_MISO_PIN   = 19;
#else
// PCB schematic pins (from GPIO_PINOUT.md)
constexpr uint8_t TFT_CS_PIN     = 10;
constexpr uint8_t TFT_DC_PIN     = 18;
constexpr uint8_t TFT_RST_PIN    = 19;
constexpr uint8_t TFT_SCLK_PIN   = 12;
constexpr uint8_t TFT_MOSI_PIN   = 13;
constexpr uint8_t TFT_MISO_PIN   = 11;
#endif
constexpr uint8_t TOUCH_CS_PIN   = 22;
constexpr uint8_t SD_CS_PIN      = 15;
constexpr uint8_t HOST_REQ_PIN   = 0;
constexpr uint8_t DISP_READY_PIN = 1;

constexpr uint16_t COLOR_BLACK = 0x0000;
constexpr uint16_t COLOR_RED   = 0xF800;
constexpr uint16_t COLOR_GREEN = 0x07E0;
constexpr uint16_t COLOR_BLUE  = 0x001F;
constexpr uint16_t COLOR_CYAN  = 0x07FF;
constexpr uint16_t COLOR_WHITE = 0xFFFF;
constexpr int32_t TFT_SPI_HZ   = 4000000;
constexpr int32_t LEGACY_SPI_HZ = 1000000;
constexpr uint16_t TFT_RAW_WIDTH  = 480;
constexpr uint16_t TFT_RAW_HEIGHT = 320;

#define PANEL_CTRL_ST7796  1
#define PANEL_CTRL_ILI9488 2
#define PANEL_CTRL_ILI9486 3

#define TFT_BUS_SW 1
#define TFT_BUS_HW 2

#ifndef PANEL_CONTROLLER
#define PANEL_CONTROLLER PANEL_CTRL_ILI9488
#endif

#ifndef TFT_BUS_MODE
#define TFT_BUS_MODE TFT_BUS_HW
#endif

#ifndef USE_LEGACY_WRITE_ONLY_WAKE
#define USE_LEGACY_WRITE_ONLY_WAKE 1
#endif

#ifndef ENABLE_AUTO_LEGACY_FALLBACK
#define ENABLE_AUTO_LEGACY_FALLBACK 0
#endif

#ifndef MINIMAL_TFT_ONLY
#define MINIMAL_TFT_ONLY 0
#endif

#define PROBE_PROFILE_ILI9488 1
#define PROBE_PROFILE_ST7796  2

#ifndef MINIMAL_PROBE_PROFILE
#define MINIMAL_PROBE_PROFILE PROBE_PROFILE_ST7796
#endif

// GFX_NOT_DEFINED for RST so begin() does NOT hardware-reset (we manage it)
#if TFT_BUS_MODE == TFT_BUS_HW
static Arduino_DataBus* tftBus = new Arduino_ESP32SPI(
    TFT_DC_PIN, TFT_CS_PIN, TFT_SCLK_PIN, TFT_MOSI_PIN, TFT_MISO_PIN);
#else
static Arduino_DataBus* tftBus = new Arduino_SWSPI(
    TFT_DC_PIN, TFT_CS_PIN, TFT_SCLK_PIN, TFT_MOSI_PIN, TFT_MISO_PIN);
#endif

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

static void setBacklightPWM(uint8_t brightness) {
    // brightness: 0 (off) to 255 (full)
    analogWrite(BL_PIN, brightness);
}

static void initHandshake() {
    pinMode(DISP_READY_PIN, OUTPUT);
    digitalWrite(DISP_READY_PIN, LOW);
    pinMode(HOST_REQ_PIN, INPUT_PULLDOWN);
}

static void isolateSharedSpiDevices() {
    pinMode(TOUCH_CS_PIN, OUTPUT);
    digitalWrite(TOUCH_CS_PIN, HIGH);
    pinMode(SD_CS_PIN, OUTPUT);
    digitalWrite(SD_CS_PIN, HIGH);
    Serial.println("[spi] touch+sd CS forced HIGH");
}

static void setTftControlLinesIdle() {
    pinMode(TFT_CS_PIN, OUTPUT);
    digitalWrite(TFT_CS_PIN, HIGH);
    pinMode(TFT_DC_PIN, OUTPUT);
    digitalWrite(TFT_DC_PIN, HIGH);
}

// Hardware reset: mirrors TFT_eSPI's pre-init pulse
static void hardwareReset() {
    setTftControlLinesIdle();
    pinMode(TFT_RST_PIN, OUTPUT);
    digitalWrite(TFT_RST_PIN, HIGH); delay(20);
    digitalWrite(TFT_RST_PIN, LOW);  delay(120);
    digitalWrite(TFT_RST_PIN, HIGH); delay(120);
    Serial.println("[tft] hardware reset done");
}

// Send SWRESET via bus, matching TFT_eSPI's ST7796 init (it does SWRESET first)
static void sendSwReset() {
    tftBus->begin(TFT_SPI_HZ, SPI_MODE3);
    tftBus->beginWrite();
    tftBus->writeCommand(0x01);  // SWRESET
    tftBus->endWrite();
    delay(150);
    Serial.println("[tft] SWRESET sent");
}

static void writeCmd(uint8_t cmd) {
    tftBus->beginWrite();
    tftBus->writeCommand(cmd);
    tftBus->endWrite();
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

static void writeReg4(uint8_t cmd, uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
    tftBus->beginWrite();
    tftBus->writeCommand(cmd);
    tftBus->write(a);
    tftBus->write(b);
    tftBus->write(c);
    tftBus->write(d);
    tftBus->endWrite();
}

static void tftSetAddressWindowRaw(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    tftBus->beginWrite();
    tftBus->writeCommand(0x2A);
    tftBus->write(static_cast<uint8_t>(x0 >> 8));
    tftBus->write(static_cast<uint8_t>(x0 & 0xFF));
    tftBus->write(static_cast<uint8_t>(x1 >> 8));
    tftBus->write(static_cast<uint8_t>(x1 & 0xFF));

    tftBus->writeCommand(0x2B);
    tftBus->write(static_cast<uint8_t>(y0 >> 8));
    tftBus->write(static_cast<uint8_t>(y0 & 0xFF));
    tftBus->write(static_cast<uint8_t>(y1 >> 8));
    tftBus->write(static_cast<uint8_t>(y1 & 0xFF));

    tftBus->writeCommand(0x2C);
    tftBus->endWrite();
}

// Diagnostic polarity wrappers
static inline void diagSetCS(bool active) {
#if DIAG_INVERT_CS
    digitalWrite(TFT_CS_PIN, active ? HIGH : LOW);
#else
    digitalWrite(TFT_CS_PIN, active ? LOW : HIGH);
#endif
}

static inline void diagSetDC(bool dataMode) {
#if DIAG_INVERT_DC
    digitalWrite(TFT_DC_PIN, dataMode ? LOW : HIGH);
#else
    digitalWrite(TFT_DC_PIN, dataMode ? HIGH : LOW);
#endif
}

static void legacySpiBegin() {
    SPI.begin(TFT_SCLK_PIN, TFT_MISO_PIN, TFT_MOSI_PIN);
    SPI.beginTransaction(SPISettings(LEGACY_SPI_HZ, MSBFIRST, SPI_MODE0));
    setTftControlLinesIdle();
}

static void legacyWriteCommand(uint8_t cmd) {
    diagSetCS(true);
    diagSetDC(false);
    SPI.transfer(cmd);
    diagSetCS(false);
}

static void legacyWriteData8(uint8_t data) {
    diagSetCS(true);
    diagSetDC(true);
    SPI.transfer(data);
    diagSetCS(false);
}

static void legacyWriteDataN(const uint8_t* data, size_t len) {
    diagSetCS(true);
    diagSetDC(true);
    for (size_t i = 0; i < len; ++i) {
        SPI.transfer(data[i]);
    }
    diagSetCS(false);
}

static void legacyWriteData16(uint16_t data) {
    legacyWriteData8(static_cast<uint8_t>(data >> 8));
    legacyWriteData8(static_cast<uint8_t>(data & 0xFF));
}

static void legacySetAddressWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    legacyWriteCommand(0x2A);
    legacyWriteData16(x0);
    legacyWriteData16(x1);
    legacyWriteCommand(0x2B);
    legacyWriteData16(y0);
    legacyWriteData16(y1);
    legacyWriteCommand(0x2C);
}

static void legacyFillScreenRaw(uint16_t color) {
    legacySetAddressWindow(0, 0, static_cast<uint16_t>(TFT_RAW_WIDTH - 1), static_cast<uint16_t>(TFT_RAW_HEIGHT - 1));

    digitalWrite(TFT_CS_PIN, LOW);
    digitalWrite(TFT_DC_PIN, HIGH);
    const uint8_t hi = static_cast<uint8_t>(color >> 8);
    const uint8_t lo = static_cast<uint8_t>(color & 0xFF);
    const uint32_t px = static_cast<uint32_t>(TFT_RAW_WIDTH) * TFT_RAW_HEIGHT;
    for (uint32_t i = 0; i < px; ++i) {
        SPI.transfer(hi);
        SPI.transfer(lo);
    }
    digitalWrite(TFT_CS_PIN, HIGH);
}

static void legacyRawRamwrSignature() {
    Serial.println("[spi] RAW test start");
    legacySetAddressWindow(0, 0, static_cast<uint16_t>(TFT_RAW_WIDTH - 1), static_cast<uint16_t>(TFT_RAW_HEIGHT - 1));
    digitalWrite(TFT_CS_PIN, LOW);
    digitalWrite(TFT_DC_PIN, HIGH);
    for (int i = 0; i < 256; ++i) {
        const uint16_t c = (i & 1) ? COLOR_RED : COLOR_BLUE;
        SPI.transfer(static_cast<uint8_t>(c >> 8));
        SPI.transfer(static_cast<uint8_t>(c & 0xFF));
    }
    digitalWrite(TFT_CS_PIN, HIGH);
    Serial.println("[spi] RAW test end");
}

static void tftFillScreenRaw(uint16_t color) {
    tftSetAddressWindowRaw(0, 0, static_cast<uint16_t>(TFT_RAW_WIDTH - 1), static_cast<uint16_t>(TFT_RAW_HEIGHT - 1));
    tftBus->beginWrite();
    for (uint32_t i = 0; i < static_cast<uint32_t>(TFT_RAW_WIDTH) * TFT_RAW_HEIGHT; ++i) {
        tftBus->write16(color);
    }
    tftBus->endWrite();
}

static void runLegacyWriteOnlyWake() {
    Serial.println("[legacy] write-only wake start");

    legacySpiBegin();

    legacyWriteCommand(0x01); // SWRESET
    delay(150);
    legacyWriteCommand(0x11); // SLPOUT
    delay(150);
    legacyWriteCommand(0x3A); // COLMOD
    legacyWriteData8(0x55);
    legacyWriteCommand(0x36); // MADCTL
    legacyWriteData8(0x28);   // old local path default for 480x320 orientation
    legacyWriteCommand(0x29); // DISPON
    delay(20);
    legacyWriteCommand(0x20); // INVOFF
    delay(10);

    legacyFillScreenRaw(COLOR_BLACK);
    Serial.println("[legacy] write-only wake done");
}

static void runLegacyWriteOnlyWake666() {
    Serial.println("[legacy666] write-only wake start");

    legacySpiBegin();

    legacyWriteCommand(0x01); // SWRESET
    delay(150);
    legacyWriteCommand(0x11); // SLPOUT
    delay(150);
    legacyWriteCommand(0x3A); // COLMOD
    legacyWriteData8(0x66);   // 18-bit color path
    legacyWriteCommand(0x36); // MADCTL
    legacyWriteData8(0x28);
    legacyWriteCommand(0x29); // DISPON
    delay(20);
    legacyWriteCommand(0x20); // INVOFF
    delay(10);

    Serial.println("[legacy666] write-only wake done");
}

static void legacyFillScreenRaw666(uint8_t r, uint8_t g, uint8_t b) {
    legacySetAddressWindow(0, 0, static_cast<uint16_t>(TFT_RAW_WIDTH - 1), static_cast<uint16_t>(TFT_RAW_HEIGHT - 1));

    digitalWrite(TFT_CS_PIN, LOW);
    digitalWrite(TFT_DC_PIN, HIGH);
    const uint32_t px = static_cast<uint32_t>(TFT_RAW_WIDTH) * TFT_RAW_HEIGHT;
    for (uint32_t i = 0; i < px; ++i) {
        SPI.transfer(r);
        SPI.transfer(g);
        SPI.transfer(b);
    }
    digitalWrite(TFT_CS_PIN, HIGH);
}

static void runLegacyRawColorTest() {
    Serial.println("[legacy] raw color test");
    legacyFillScreenRaw(COLOR_RED);
    Serial.println("[legacy] RED");
    delay(250);
    legacyFillScreenRaw(COLOR_GREEN);
    Serial.println("[legacy] GREEN");
    delay(250);
    legacyFillScreenRaw(COLOR_BLUE);
    Serial.println("[legacy] BLUE");
    delay(250);
    legacyFillScreenRaw(COLOR_WHITE);
    Serial.println("[legacy] WHITE");
    delay(250);
    legacyFillScreenRaw(COLOR_BLACK);
    Serial.println("[legacy] BLACK");
}

static void legacyFillScreenRaw565(uint16_t width, uint16_t height, uint16_t color) {
    legacySetAddressWindow(0, 0,
        static_cast<uint16_t>(width - 1),
        static_cast<uint16_t>(height - 1));

    digitalWrite(TFT_CS_PIN, LOW);
    digitalWrite(TFT_DC_PIN, HIGH);
    const uint8_t hi = static_cast<uint8_t>(color >> 8);
    const uint8_t lo = static_cast<uint8_t>(color & 0xFF);
    const uint32_t px = static_cast<uint32_t>(width) * height;
    for (uint32_t i = 0; i < px; ++i) {
#if DIAG_SWAP_BYTES
        SPI.transfer(lo);
        SPI.transfer(hi);
#else
        SPI.transfer(hi);
        SPI.transfer(lo);
#endif
    }
    digitalWrite(TFT_CS_PIN, HIGH);
}

static void legacyFillScreenRaw666(uint16_t width, uint16_t height, uint8_t r, uint8_t g, uint8_t b) {
    legacySetAddressWindow(0, 0,
        static_cast<uint16_t>(width - 1),
        static_cast<uint16_t>(height - 1));

    digitalWrite(TFT_CS_PIN, LOW);
    digitalWrite(TFT_DC_PIN, HIGH);
    const uint32_t px = static_cast<uint32_t>(width) * height;
    for (uint32_t i = 0; i < px; ++i) {
        SPI.transfer(r);
        SPI.transfer(g);
        SPI.transfer(b);
    }
    digitalWrite(TFT_CS_PIN, HIGH);
}

static void runProbeInitIli9488() {
    static const uint8_t e0[] = {
        0x00, 0x03, 0x09, 0x08, 0x16, 0x0A, 0x3F, 0x78,
        0x4C, 0x09, 0x0A, 0x08, 0x16, 0x1A, 0x0F
    };
    static const uint8_t e1[] = {
        0x00, 0x16, 0x19, 0x03, 0x0F, 0x05, 0x32, 0x45,
        0x46, 0x04, 0x0E, 0x0D, 0x35, 0x37, 0x0F
    };
    static const uint8_t f7[] = {0xA9, 0x51, 0x2C, 0x82};

    Serial.println("[probe/ili9488] SWRESET");
    legacyWriteCommand(0x01);
    delay(150);

    Serial.println("[probe/ili9488] gamma + power table");
    legacyWriteCommand(0xE0); legacyWriteDataN(e0, sizeof(e0));
    legacyWriteCommand(0xE1); legacyWriteDataN(e1, sizeof(e1));
    legacyWriteCommand(0xC0); legacyWriteData8(0x17); legacyWriteData8(0x15);
    legacyWriteCommand(0xC1); legacyWriteData8(0x41);
    legacyWriteCommand(0xC5); legacyWriteData8(0x00); legacyWriteData8(0x12); legacyWriteData8(0x80);

    // MX + BGR, rotation baseline used by common ILI9488 setups.
    legacyWriteCommand(0x36); legacyWriteData8(0x48);

    // SPI path for ILI9488 uses 18-bit pixel transfer.
    legacyWriteCommand(0x3A); legacyWriteData8(0x66);
    legacyWriteCommand(0xB0); legacyWriteData8(0x00);
    legacyWriteCommand(0xB1); legacyWriteData8(0xA0);
    legacyWriteCommand(0xB4); legacyWriteData8(0x02);
    legacyWriteCommand(0xB6); legacyWriteData8(0x02); legacyWriteData8(0x02);
    legacyWriteCommand(0xB7); legacyWriteData8(0xC6);
    legacyWriteCommand(0xF7); legacyWriteDataN(f7, sizeof(f7));

    Serial.println("[probe/ili9488] SLPOUT");
    legacyWriteCommand(0x11);
    delay(120);

    legacyWriteCommand(0x20); // INVOFF
    legacyWriteCommand(0x29); // DISPON
    delay(100);
    Serial.println("[probe/ili9488] DISPON");
}

static void runProbeInitSt7796() {
    static const uint8_t b6[] = {0x80, 0x22, 0x3B};
    static const uint8_t e8[] = {0x40, 0x8A, 0x00, 0x00, 0x29, 0x19, 0xA5, 0x33};
    static const uint8_t e0[] = {0xF0, 0x09, 0x0B, 0x06, 0x04, 0x15, 0x2F, 0x54, 0x42, 0x3C, 0x17, 0x14, 0x18, 0x1B};
    static const uint8_t e1[] = {0xE0, 0x09, 0x0B, 0x06, 0x04, 0x03, 0x2B, 0x43, 0x42, 0x3B, 0x16, 0x14, 0x17, 0x1B};

    Serial.println("[probe/st7796] SWRESET");
    legacyWriteCommand(0x01);
    delay(150);

    Serial.println("[probe/st7796] extension + drive table");
    legacyWriteCommand(0x11); // SLPOUT early in ST7796 path
    delay(120);
    legacyWriteCommand(0xF0); legacyWriteData8(0xC3);
    legacyWriteCommand(0xF0); legacyWriteData8(0x96);
    legacyWriteCommand(0x36); legacyWriteData8(0x48);
    legacyWriteCommand(0x3A); legacyWriteData8(0x55);
    legacyWriteCommand(0xB4); legacyWriteData8(0x01);
    legacyWriteCommand(0xB6); legacyWriteDataN(b6, sizeof(b6));
    legacyWriteCommand(0xE8); legacyWriteDataN(e8, sizeof(e8));
    legacyWriteCommand(0xC1); legacyWriteData8(0x06);
    legacyWriteCommand(0xC2); legacyWriteData8(0xA7);
    legacyWriteCommand(0xC5); legacyWriteData8(0x18);
    legacyWriteCommand(0xE0); legacyWriteDataN(e0, sizeof(e0));
    legacyWriteCommand(0xE1); legacyWriteDataN(e1, sizeof(e1));
    legacyWriteCommand(0xF0); legacyWriteData8(0x3C);
    legacyWriteCommand(0xF0); legacyWriteData8(0x69);

    legacyWriteCommand(0x20); // INVOFF
    legacyWriteCommand(0x29); // DISPON
    delay(100);
    Serial.println("[probe/st7796] DISPON");
}

// Emits an unmistakable SPI signature for analyzer correlation:
// 0x2A, 0x2B, 0x2C followed by a short RGB565 burst.
static void rawRamwrSignature() {
    Serial.println("[spi] RAW test start");

    // Full 320x480 window
    writeReg4(0x2A, 0x00, 0x00, 0x01, 0x3F);
    writeReg4(0x2B, 0x00, 0x00, 0x01, 0xDF);

    tftBus->beginWrite();
    tftBus->writeCommand(0x2C);
    for (int i = 0; i < 256; ++i) {
        tftBus->write16((i & 1) ? COLOR_RED : COLOR_BLUE);
    }
    tftBus->endWrite();

    Serial.println("[spi] RAW test end");
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
    writeReg8(0x36, 0x68); // rotation=1 + BGR (match setRotation(1))
    writeCmd(0x21); // INVON
    writeReg8(0xF0, 0x3C);
    writeReg8(0xF0, 0x69);
    writeCmd(0x29); // DISPON
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
    delay(500);  // let power rails settle after cold plug-in
    Serial.printf("[display] boot - Arduino_GFX %s probe\n", kPanelControllerName);
#if TFT_BUS_MODE == TFT_BUS_HW
    Serial.println("[tft] bus = HW SPI");
#else
    Serial.println("[tft] bus = SW SPI");
#endif

    initHandshake();
    isolateSharedSpiDevices();
    setBacklight(false);
    Serial.println("[tft] backlight OFF during init");

#if USE_DEVBOARD_PINS
    Serial.println("[pins] using legacy dev-board mapping (CS=4, DC=16, RST=17)");
#else
    Serial.println("[pins] using PCB schematic mapping (CS=10, DC=18, RST=19)");
#endif

#if USE_LEGACY_WRITE_ONLY_WAKE
    Serial.println("[mode] force legacy write-only wake");
#else
    Serial.println("[mode] full init first; legacy fallback enabled");
#endif

#if MINIMAL_TFT_ONLY
    Serial.println("[mode] MINIMAL_TFT_ONLY active");
#endif

    // Step 1: hardware reset (active-low pulse on GPIO19)
    hardwareReset();
    // Keep original SWRESET behavior for non-minimal paths.
#if !MINIMAL_TFT_ONLY
    // Step 2: software reset — TFT_eSPI does this at the top of its ST7796 init
    sendSwReset();
#endif

#if MINIMAL_TFT_ONLY
    Serial.println("[minimal] deterministic dual-profile probe mode");
    Serial.println("[minimal] RULE: power-cycle before every test (unplug 5s, replug)");

#if DIAG_INVERT_DC
    Serial.println("[DIAGNOSTIC] DC polarity INVERTED (dataMode=LOW for data, HIGH for command)");
#endif
#if DIAG_INVERT_CS
    Serial.println("[DIAGNOSTIC] CS polarity INVERTED (HIGH for active, LOW for idle)");
#endif
#if DIAG_SWAP_BYTES
    Serial.println("[DIAGNOSTIC] 16-bit color byte order SWAPPED (lo byte first, then hi)");
#endif

    // Use one SPI mode for all probe commands/data (MODE0) for reproducibility.
    legacySpiBegin();

#if MINIMAL_PROBE_PROFILE == PROBE_PROFILE_ST7796
    Serial.println("[minimal] profile = ST7796 (full table)");
    runProbeInitSt7796();
#else
    Serial.println("[minimal] profile = ILI9488 (full table)");
    runProbeInitIli9488();
#endif

    Serial.println("[minimal] ramping backlight 0->255 over 5 seconds...");
    for (uint8_t bri = 0; bri <= 255; bri++) {
        setBacklightPWM(bri);
        delay(20);
    }
    Serial.println("[minimal] backlight at full brightness");

#if MINIMAL_PROBE_PROFILE == PROBE_PROFILE_ST7796
    Serial.println("[minimal] fill RED using RGB565 over 320x480");
    legacyFillScreenRaw565(320, 480, COLOR_RED);
#else
    Serial.println("[minimal] fill RED using RGB666 over 320x480");
    legacyFillScreenRaw666(320, 480, 0xFF, 0x00, 0x00);
#endif

    Serial.println("[done] probe complete; screen should be solid RED");
    Serial.println("[diag] entering brightness diagnostic loop — observe at which level RED appears");
    
    // Diagnostic: hold at each brightness level for 3 seconds so user can observe
    while (true) {
        for (uint8_t level : {0, 50, 100, 150, 200, 255}) {
            Serial.printf("[diag] brightness = %u (hold 3s)\n", level);
            setBacklightPWM(level);
            delay(3000);
        }
    }
#endif

#if USE_LEGACY_WRITE_ONLY_WAKE
    runLegacyWriteOnlyWake();
    setBacklight(true);
    Serial.println("[tft] backlight ON after legacy wake");
    legacyRawRamwrSignature();
    runLegacyRawColorTest();
    digitalWrite(DISP_READY_PIN, HIGH);
    Serial.println("[display] DISP_READY HIGH - legacy path complete");
    return;
#endif

    // Step 3: full init via Arduino_GFX (RST=GFX_NOT_DEFINED so it won't reset again)
    Serial.println("[tft] init...");
    const bool tftInitOk = tft->begin(TFT_SPI_HZ);
    if (!tftInitOk) {
        Serial.println("[tft] init failed");
#if ENABLE_AUTO_LEGACY_FALLBACK
        Serial.println("[tft] falling back to legacy write-only wake");
        runLegacyWriteOnlyWake();
        setBacklight(true);
        Serial.println("[tft] backlight ON after legacy fallback");
        legacyRawRamwrSignature();
        runLegacyRawColorTest();
        digitalWrite(DISP_READY_PIN, HIGH);
        Serial.println("[display] DISP_READY HIGH - legacy fallback complete");
        return;
#else
        Serial.println("[tft] legacy fallback disabled");
        return;
#endif
    }
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

    rawRamwrSignature();

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
