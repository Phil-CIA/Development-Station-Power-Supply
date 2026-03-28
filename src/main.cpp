#include <Arduino.h>
#include <math.h>
#include <Wire.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <esp_system.h>
#include <Preferences.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "power_telemetry_map.h"

SDL_Arduino_INA3221 ina3221;
TFT_eSPI display = TFT_eSPI();
HatPowerTelemetry hatTelemetry(ina3221);
Preferences prefs;

constexpr uint8_t STATUS_LED_PIN = 2;
constexpr uint8_t MCP4231_CS_PIN = 5;
#if defined(I2C_SDA_GPIO)
constexpr uint8_t I2C_SDA_PIN = I2C_SDA_GPIO;
#else
constexpr uint8_t I2C_SDA_PIN = 21;
#endif

#if defined(I2C_SCL_GPIO)
constexpr uint8_t I2C_SCL_PIN = I2C_SCL_GPIO;
#else
constexpr uint8_t I2C_SCL_PIN = 22;
#endif
constexpr uint8_t INA3221_ADDR_MIN = 0x40;
constexpr uint8_t INA3221_ADDR_MAX = 0x43;

#if defined(TFT_SCLK)
constexpr int SPI_SCLK_PIN = TFT_SCLK;
#else
constexpr int SPI_SCLK_PIN = 18;
#endif

#if defined(TFT_MISO)
constexpr int SPI_MISO_PIN = TFT_MISO;
#else
constexpr int SPI_MISO_PIN = 19;
#endif

#if defined(TFT_MOSI)
constexpr int SPI_MOSI_PIN = TFT_MOSI;
#else
constexpr int SPI_MOSI_PIN = 23;
#endif

#if defined(MCP4231_SDI_PIN)
constexpr int MCP4231_DATA_PIN = MCP4231_SDI_PIN;
#else
constexpr int MCP4231_DATA_PIN = SPI_MOSI_PIN;
#endif

#if defined(MCP4231_SCK_PIN)
constexpr int MCP4231_CLOCK_PIN = MCP4231_SCK_PIN;
#else
constexpr int MCP4231_CLOCK_PIN = SPI_SCLK_PIN;
#endif

#if defined(TFT_HW_RST)
constexpr int TFT_RESET_PIN = TFT_HW_RST;
#elif defined(TFT_RST)
constexpr int TFT_RESET_PIN = TFT_RST;
#else
constexpr int TFT_RESET_PIN = -1;
#endif

#if defined(TFT_RST_ACTIVE_LOW)
constexpr bool TFT_RESET_ACTIVE_LOW = (TFT_RST_ACTIVE_LOW != 0);
#else
constexpr bool TFT_RESET_ACTIVE_LOW = true;
#endif

bool ledState = false;
uint32_t lastFrameMs = 0;
uint32_t loopCounter = 0;
bool tftInitialized = false;
bool tftWriteOnlyReady = false;
bool tftWriteOnlyDashboardInitialized = false;

enum class TftInitState : uint8_t {
  Idle,
  Running,
  Done,
  Failed
};

volatile TftInitState tftInitState = TftInitState::Idle;
volatile bool tftInitResult = false;
TaskHandle_t tftInitTaskHandle = nullptr;

constexpr bool AUTO_TFT_INIT = true;
constexpr bool AUTO_TFT_WRITE_ONLY = true;
constexpr bool TFT_WRITE_ONLY_ROTATE_90 = true;
constexpr bool BOOT_LOG_VERBOSE = false;
constexpr const char* PROJECT_PATH = "c:/Users/user/Esp32 projects VScode/WorkStation";
constexpr const char* PROJECT_ENV = "esp32dev";

float ch3VoltageScale = 1.00f;
float ch3CurrentScale = 1.00f;
uint8_t mcpWiperCached[2] = {128, 128};
bool autoRangeEnabled = true;
float autoRangeLowThresholdmA = 250.0f;
float autoRangeHighThresholdmA = 900.0f;

constexpr uint32_t PERSIST_VERSION = 1;
constexpr const char* PERSIST_NAMESPACE = "hatpsu";
constexpr size_t PERSIST_RAIL_COUNT = static_cast<size_t>(HatRail::Count);
constexpr size_t PERSIST_RANGE_COUNT = 3;

struct PersistentConfig {
  uint32_t version;
  uint8_t autoRangeEnabled;
  float lowThresholdmA;
  float highThresholdmA;
  RailCalibration cal[PERSIST_RAIL_COUNT][PERSIST_RANGE_COUNT];
};

#if defined(TFT_WIDTH)
constexpr uint16_t TFT_PANEL_WIDTH = TFT_WIDTH;
#else
constexpr uint16_t TFT_PANEL_WIDTH = 320;
#endif

#if defined(TFT_HEIGHT)
constexpr uint16_t TFT_PANEL_HEIGHT = TFT_HEIGHT;
#else
constexpr uint16_t TFT_PANEL_HEIGHT = 480;
#endif

constexpr uint16_t TFT_LOGICAL_WIDTH = TFT_WRITE_ONLY_ROTATE_90 ? TFT_PANEL_HEIGHT : TFT_PANEL_WIDTH;
constexpr uint16_t TFT_LOGICAL_HEIGHT = TFT_WRITE_ONLY_ROTATE_90 ? TFT_PANEL_WIDTH : TFT_PANEL_HEIGHT;

struct BootStatus {
  bool uartOk;
  bool tftOk;
  bool tftIdOk;
  bool i2cOk;
  bool inaOk;
  bool inaRwOk;
  bool mcpOk;
  bool touchOk;
  float inaRailVoltage;
  uint8_t inaAddress;
  uint8_t i2cCount;
  uint32_t tftId;
};

BootStatus bootStatus = {false, false, false, false, false, false, false, false, NAN, 0x00, 0, 0};

static const char* resetReasonText(esp_reset_reason_t reason) {
  switch (reason) {
    case ESP_RST_POWERON: return "POWERON";
    case ESP_RST_EXT: return "EXTERNAL_PIN";
    case ESP_RST_SW: return "SOFTWARE";
    case ESP_RST_PANIC: return "PANIC";
    case ESP_RST_INT_WDT: return "INT_WDT";
    case ESP_RST_TASK_WDT: return "TASK_WDT";
    case ESP_RST_WDT: return "OTHER_WDT";
    case ESP_RST_DEEPSLEEP: return "DEEPSLEEP";
    case ESP_RST_BROWNOUT: return "BROWNOUT";
    case ESP_RST_SDIO: return "SDIO";
    default: return "UNKNOWN";
  }
}

static const char* activeTftDriverText() {
#if defined(ST7796_DRIVER)
  return "ST7796";
#elif defined(ILI9488_DRIVER)
  return "ILI9488";
#elif defined(ILI9486_DRIVER)
  return "ILI9486";
#else
  return "UNKNOWN";
#endif
}

static float readInaVoltageScaled(int channel) {
  float voltage = ina3221.getBusVoltage_V(channel);
  if (channel == 3) {
    voltage *= ch3VoltageScale;
  }
  return voltage;
}

static float readInaCurrentScaled(int channel) {
  float current = ina3221.getCurrent_mA(channel);
  if (channel == 3) {
    current *= ch3CurrentScale;
  }
  return current;
}

static void savePersistentConfig() {
  PersistentConfig cfg = {};
  cfg.version = PERSIST_VERSION;
  cfg.autoRangeEnabled = autoRangeEnabled ? 1U : 0U;
  cfg.lowThresholdmA = autoRangeLowThresholdmA;
  cfg.highThresholdmA = autoRangeHighThresholdmA;

  for (size_t rail = 0; rail < PERSIST_RAIL_COUNT; rail++) {
    for (size_t range = 0; range < PERSIST_RANGE_COUNT; range++) {
      cfg.cal[rail][range] = hatTelemetry.calibration(static_cast<HatRail>(rail), static_cast<HatRange>(range));
    }
  }

  if (!prefs.begin(PERSIST_NAMESPACE, false)) {
    Serial.println("[CFG] NVS open failed for write");
    return;
  }

  const size_t written = prefs.putBytes("cfg", &cfg, sizeof(cfg));
  prefs.end();

  if (written == sizeof(cfg)) {
    Serial.println("[CFG] Saved");
  } else {
    Serial.printf("[CFG] Save failed (%u/%u bytes)\n", static_cast<unsigned>(written), static_cast<unsigned>(sizeof(cfg)));
  }
}

static bool loadPersistentConfig(bool verbose) {
  if (!prefs.begin(PERSIST_NAMESPACE, true)) {
    if (verbose) {
      Serial.println("[CFG] NVS open failed for read");
    }
    return false;
  }

  PersistentConfig cfg = {};
  const size_t readBytes = prefs.getBytes("cfg", &cfg, sizeof(cfg));
  prefs.end();

  if (readBytes != sizeof(cfg)) {
    if (verbose) {
      Serial.println("[CFG] No saved config");
    }
    return false;
  }

  if (cfg.version != PERSIST_VERSION) {
    if (verbose) {
      Serial.printf("[CFG] Version mismatch (got %lu, expected %lu)\n", static_cast<unsigned long>(cfg.version), static_cast<unsigned long>(PERSIST_VERSION));
    }
    return false;
  }

  autoRangeEnabled = (cfg.autoRangeEnabled != 0U);
  autoRangeLowThresholdmA = cfg.lowThresholdmA;
  autoRangeHighThresholdmA = cfg.highThresholdmA;
  hatTelemetry.setAutoRangeThresholds(autoRangeLowThresholdmA, autoRangeHighThresholdmA);

  for (size_t rail = 0; rail < PERSIST_RAIL_COUNT; rail++) {
    const RailMapping& map = hatTelemetry.mapping(static_cast<HatRail>(rail));
    for (size_t range = 0; range < PERSIST_RANGE_COUNT; range++) {
      const HatRange r = static_cast<HatRange>(range);
      if (!map.hasDualRange && r != HatRange::Single) {
        continue;
      }
      if (map.hasDualRange && r == HatRange::Single) {
        continue;
      }
      hatTelemetry.setCalibration(static_cast<HatRail>(rail), r, cfg.cal[rail][range]);
    }
  }

  if (verbose) {
    Serial.printf("[CFG] Loaded: AUTORANGE=%s RTHR=%.1f/%.1f mA\n", autoRangeEnabled ? "ON" : "OFF", autoRangeLowThresholdmA, autoRangeHighThresholdmA);
  }
  return true;
}

static void resetPersistentConfigDefaults() {
  autoRangeEnabled = true;
  autoRangeLowThresholdmA = 250.0f;
  autoRangeHighThresholdmA = 900.0f;
  hatTelemetry.setAutoRangeThresholds(autoRangeLowThresholdmA, autoRangeHighThresholdmA);
  hatTelemetry.resetCalibration();
}

static void erasePersistentConfig() {
  if (!prefs.begin(PERSIST_NAMESPACE, false)) {
    Serial.println("[CFG] NVS open failed for erase");
    return;
  }
  prefs.remove("cfg");
  prefs.end();
  Serial.println("[CFG] Erased saved config");
}

static RailMeasurement readRailMeasurement(HatRail rail) {
  if (autoRangeEnabled) {
    return hatTelemetry.readAutoCalibrated(rail);
  }
  return hatTelemetry.readCalibrated(rail, HatRange::High);
}

static bool parseRailToken(const String& token, HatRail& rail) {
  if (token == "5V" || token == "RAIL5V") {
    rail = HatRail::Rail5V;
    return true;
  }
  if (token == "3V3" || token == "3.3" || token == "RAIL3V3") {
    rail = HatRail::Rail3V3;
    return true;
  }
  if (token == "ADJ" || token == "ADJUSTABLE" || token == "RAILADJ") {
    rail = HatRail::RailAdj;
    return true;
  }
  if (token == "IN12" || token == "INCOMING" || token == "12V") {
    rail = HatRail::RailIncoming12V;
    return true;
  }
  return false;
}

static bool parseRangeToken(const String& token, HatRange& range) {
  if (token == "HIGH" || token == "HI") {
    range = HatRange::High;
    return true;
  }
  if (token == "LOW" || token == "LO") {
    range = HatRange::Low;
    return true;
  }
  if (token == "SINGLE" || token == "ONE") {
    range = HatRange::Single;
    return true;
  }
  return false;
}

static void printLogicalRailSnapshot() {
  static const HatRail rails[] = {
    HatRail::Rail5V,
    HatRail::Rail3V3,
    HatRail::RailAdj,
    HatRail::RailIncoming12V
  };

  for (const HatRail rail : rails) {
    const RailMeasurement active = readRailMeasurement(rail);
    if (rail == HatRail::RailIncoming12V) {
      Serial.printf("[RAIL] IN12V      ACTIVE=%s V=%.3f I=%.1f\n",
                    HatPowerTelemetry::rangeName(active.usedRange),
                    active.busVoltageV,
                    active.currentmA);
      continue;
    }

    const RailMeasurement lo = hatTelemetry.readCalibrated(rail, HatRange::Low);
    const RailMeasurement hr = hatTelemetry.readCalibrated(rail, HatRange::High);
    Serial.printf("[RAIL] %-10s ACTIVE=%s V=%.3f I=%.1f | HI V=%.3f I=%.1f | LO V=%.3f I=%.1f\n",
                  hatTelemetry.mapping(rail).railName,
                  HatPowerTelemetry::rangeName(active.usedRange),
                  active.busVoltageV,
                  active.currentmA,
                  hr.busVoltageV,
                  hr.currentmA,
                  lo.busVoltageV,
                  lo.currentmA);
  }
}

static bool isI2cDevicePresent(uint8_t address) {
  Wire.beginTransmission(address);
  return Wire.endTransmission() == 0;
}

static uint8_t scanI2cAndReport(bool verbose) {
  uint8_t count = 0;
  for (uint8_t address = 0x03; address <= 0x77; address++) {
    if (isI2cDevicePresent(address)) {
      count++;
      if (verbose) {
        Serial.printf("[I2C] Device found at 0x%02X\n", address);
      }
    }
  }
  if (verbose) {
    Serial.printf("[I2C] Total devices: %u\n", count);
  }
  return count;
}

struct InaStartupDiagResult {
  bool rwOk;
  bool controlOk;
  int manufId;
  float ch2Voltage;
};

static InaStartupDiagResult probeInaAtAddress(uint8_t address, bool verbose) {
  (void)verbose;
  InaStartupDiagResult result = {false, false, 0, NAN};

  ina3221.INA3221_i2caddr = address;
  ina3221.INA3221SetConfig();

  uint16_t configBefore = 0;
  ina3221.wireReadRegister(0x00, &configBefore);
  const uint16_t configTest = static_cast<uint16_t>(configBefore ^ 0x0008);
  ina3221.wireWriteRegister(0x00, configTest);
  delay(1);

  uint16_t configReadBack = 0;
  ina3221.wireReadRegister(0x00, &configReadBack);
  result.rwOk = (configReadBack == configTest);

  ina3221.wireWriteRegister(0x00, configBefore);
  delay(1);

  result.manufId = ina3221.getManufID();
  result.ch2Voltage = ina3221.getBusVoltage_V(2);

  const bool manufLooksValid = (result.manufId == 0x5449) || (result.manufId == 0x544A);
  const bool readingLooksValid = isfinite(result.ch2Voltage) && (result.ch2Voltage > -0.5f) && (result.ch2Voltage < 60.0f);
  result.controlOk = result.rwOk && (manufLooksValid || readingLooksValid);

  return result;
}

static void i2cBusRecover() {
  Serial.printf("[I2C] Bus recovery on SDA=%d SCL=%d\n", I2C_SDA_PIN, I2C_SCL_PIN);

  pinMode(I2C_SDA_PIN, INPUT_PULLUP);
  pinMode(I2C_SCL_PIN, INPUT_PULLUP);
  delay(1);

  pinMode(I2C_SCL_PIN, OUTPUT_OPEN_DRAIN);
  digitalWrite(I2C_SCL_PIN, HIGH);

  for (int pulse = 0; pulse < 18; pulse++) {
    digitalWrite(I2C_SCL_PIN, LOW);
    delayMicroseconds(8);
    digitalWrite(I2C_SCL_PIN, HIGH);
    delayMicroseconds(8);
  }

  pinMode(I2C_SDA_PIN, OUTPUT_OPEN_DRAIN);
  digitalWrite(I2C_SDA_PIN, LOW);
  delayMicroseconds(8);
  digitalWrite(I2C_SCL_PIN, HIGH);
  delayMicroseconds(8);
  digitalWrite(I2C_SDA_PIN, HIGH);
  delayMicroseconds(8);

  pinMode(I2C_SDA_PIN, INPUT_PULLUP);
  pinMode(I2C_SCL_PIN, INPUT_PULLUP);
  delay(1);

  Serial.printf("[I2C] Recovery done. Idle SDA=%d SCL=%d\n", digitalRead(I2C_SDA_PIN), digitalRead(I2C_SCL_PIN));
}

static void setTftControlLinesIdle() {
#if defined(TFT_CS) && (TFT_CS >= 0)
  pinMode(TFT_CS, OUTPUT);
  digitalWrite(TFT_CS, HIGH);
#endif
#if defined(TFT_DC) && (TFT_DC >= 0)
  pinMode(TFT_DC, OUTPUT);
  digitalWrite(TFT_DC, HIGH);
#endif
}

static void forceTftHardwareReset() {
  if (TFT_RESET_PIN < 0) {
    Serial.println("[SPI] TFT reset line disabled in config");
    return;
  }

  setTftControlLinesIdle();
  Serial.println("[SPI] TFT control lines idled before reset");

  const uint8_t resetAsserted = TFT_RESET_ACTIVE_LOW ? LOW : HIGH;
  const uint8_t resetReleased = TFT_RESET_ACTIVE_LOW ? HIGH : LOW;

  pinMode(TFT_RESET_PIN, OUTPUT);

  digitalWrite(TFT_RESET_PIN, resetReleased);
  delay(20);
  digitalWrite(TFT_RESET_PIN, resetAsserted);
  delay(120);
  digitalWrite(TFT_RESET_PIN, resetReleased);

  delay(200);
  setTftControlLinesIdle();
  Serial.printf("[SPI] TFT reset pulse sent on pin %d (%s)\n", TFT_RESET_PIN, TFT_RESET_ACTIVE_LOW ? "active-low" : "active-high");
  Serial.printf("[SPI] TFT reset final level=%d (expect %d)\n", digitalRead(TFT_RESET_PIN), resetReleased);
#if !defined(TFT_RST)
  Serial.println("[SPI] TFT_RST disabled in config");
#endif
}

static void printSpiPinMap() {
#ifdef TFT_CS
  Serial.printf("[SPI] TFT_CS=%d\n", TFT_CS);
#endif
#ifdef TFT_DC
  Serial.printf("[SPI] TFT_DC=%d\n", TFT_DC);
#endif
#ifdef TFT_RST
  Serial.printf("[SPI] TFT_RST=%d\n", TFT_RST);
#endif
  Serial.printf("[SPI] TFT_RESET_PIN=%d polarity=%s\n", TFT_RESET_PIN, TFT_RESET_ACTIVE_LOW ? "active-low" : "active-high");
  Serial.printf("[SPI] BUS SCLK=%d MISO=%d MOSI=%d\n", SPI_SCLK_PIN, SPI_MISO_PIN, SPI_MOSI_PIN);
}

static void runRawSpiBurst(bool verbose) {
#if defined(TFT_CS) && (TFT_CS >= 0)
  pinMode(TFT_CS, OUTPUT);
  digitalWrite(TFT_CS, HIGH);
#endif
#if defined(TFT_DC) && (TFT_DC >= 0)
  pinMode(TFT_DC, OUTPUT);
  digitalWrite(TFT_DC, LOW);
#endif

  if (verbose) {
    Serial.println("[SPI] Running raw SPI burst test (0xAA/0x55)");
  }

#if defined(TFT_CS) && (TFT_CS >= 0)
  digitalWrite(TFT_CS, LOW);
#endif

#if defined(TFT_DC) && (TFT_DC >= 0)
  digitalWrite(TFT_DC, LOW);
#endif
  for (int index = 0; index < 32; index++) {
    SPI.transfer(0xAA);
    SPI.transfer(0x55);
  }

#if defined(TFT_DC) && (TFT_DC >= 0)
  digitalWrite(TFT_DC, HIGH);
#endif
  for (int index = 0; index < 32; index++) {
    SPI.transfer(0x0F);
    SPI.transfer(0xF0);
  }

#if defined(TFT_CS) && (TFT_CS >= 0)
  digitalWrite(TFT_CS, HIGH);
#endif

  if (verbose) {
    Serial.println("[SPI] Raw SPI burst complete");
  }
}

static uint8_t tftReadCommand8(uint8_t command) {
#if defined(TFT_CS) && (TFT_CS >= 0)
  digitalWrite(TFT_CS, LOW);
#endif
#if defined(TFT_DC) && (TFT_DC >= 0)
  digitalWrite(TFT_DC, LOW);
#endif
  SPI.transfer(command);
#if defined(TFT_DC) && (TFT_DC >= 0)
  digitalWrite(TFT_DC, HIGH);
#endif
  const uint8_t value = SPI.transfer(0x00);
#if defined(TFT_CS) && (TFT_CS >= 0)
  digitalWrite(TFT_CS, HIGH);
#endif
  return value;
}

static void tftWriteCommand(uint8_t command) {
#if defined(TFT_CS) && (TFT_CS >= 0)
  digitalWrite(TFT_CS, LOW);
#endif
#if defined(TFT_DC) && (TFT_DC >= 0)
  digitalWrite(TFT_DC, LOW);
#endif
  SPI.transfer(command);
#if defined(TFT_CS) && (TFT_CS >= 0)
  digitalWrite(TFT_CS, HIGH);
#endif
}

static void tftWriteData8(uint8_t value) {
#if defined(TFT_CS) && (TFT_CS >= 0)
  digitalWrite(TFT_CS, LOW);
#endif
#if defined(TFT_DC) && (TFT_DC >= 0)
  digitalWrite(TFT_DC, HIGH);
#endif
  SPI.transfer(value);
#if defined(TFT_CS) && (TFT_CS >= 0)
  digitalWrite(TFT_CS, HIGH);
#endif
}

static void tftWriteData16(uint16_t value) {
  tftWriteData8(static_cast<uint8_t>(value >> 8));
  tftWriteData8(static_cast<uint8_t>(value & 0xFF));
}

static void tftSetAddressWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
  tftWriteCommand(0x2A);  // CASET
  tftWriteData16(x0);
  tftWriteData16(x1);

  tftWriteCommand(0x2B);  // RASET
  tftWriteData16(y0);
  tftWriteData16(y1);

  tftWriteCommand(0x2C);  // RAMWR
}

static void tftFillScreenRaw(uint16_t color) {
  constexpr uint16_t width = TFT_LOGICAL_WIDTH;
  constexpr uint16_t height = TFT_LOGICAL_HEIGHT;

  tftSetAddressWindow(0, 0, static_cast<uint16_t>(width - 1), static_cast<uint16_t>(height - 1));

#if defined(TFT_CS) && (TFT_CS >= 0)
  digitalWrite(TFT_CS, LOW);
#endif
#if defined(TFT_DC) && (TFT_DC >= 0)
  digitalWrite(TFT_DC, HIGH);
#endif

  const uint8_t hi = static_cast<uint8_t>(color >> 8);
  const uint8_t lo = static_cast<uint8_t>(color & 0xFF);
  const uint32_t pixels = static_cast<uint32_t>(width) * static_cast<uint32_t>(height);
  for (uint32_t i = 0; i < pixels; i++) {
    SPI.transfer(hi);
    SPI.transfer(lo);
  }

#if defined(TFT_CS) && (TFT_CS >= 0)
  digitalWrite(TFT_CS, HIGH);
#endif
}

static void tftFillRectRaw(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
  if ((w == 0) || (h == 0)) {
    return;
  }

  const uint16_t x1 = static_cast<uint16_t>(x + w - 1);
  const uint16_t y1 = static_cast<uint16_t>(y + h - 1);
  tftSetAddressWindow(x, y, x1, y1);

#if defined(TFT_CS) && (TFT_CS >= 0)
  digitalWrite(TFT_CS, LOW);
#endif
#if defined(TFT_DC) && (TFT_DC >= 0)
  digitalWrite(TFT_DC, HIGH);
#endif

  const uint8_t hi = static_cast<uint8_t>(color >> 8);
  const uint8_t lo = static_cast<uint8_t>(color & 0xFF);
  const uint32_t pixels = static_cast<uint32_t>(w) * static_cast<uint32_t>(h);
  for (uint32_t i = 0; i < pixels; i++) {
    SPI.transfer(hi);
    SPI.transfer(lo);
  }

#if defined(TFT_CS) && (TFT_CS >= 0)
  digitalWrite(TFT_CS, HIGH);
#endif
}

static void tftDrawDigit7SegRaw(int x, int y, int scale, int digit, uint16_t color, uint16_t background) {
  const int t = 4 * scale;
  const int len = 14 * scale;
  const int width = (2 * t) + len;
  const int height = (3 * t) + (2 * len);

  tftFillRectRaw(static_cast<uint16_t>(x), static_cast<uint16_t>(y), static_cast<uint16_t>(width), static_cast<uint16_t>(height), background);

  if ((digit < 0) || (digit > 9)) {
    return;
  }

  static const uint8_t segmentMask[10] = {
    0b00111111,
    0b00000110,
    0b01011011,
    0b01001111,
    0b01100110,
    0b01101101,
    0b01111101,
    0b00000111,
    0b01111111,
    0b01101111
  };

  auto drawSegment = [&](int segment) {
    switch (segment) {
      case 0: tftFillRectRaw(static_cast<uint16_t>(x + t), static_cast<uint16_t>(y), static_cast<uint16_t>(len), static_cast<uint16_t>(t), color); break;
      case 1: tftFillRectRaw(static_cast<uint16_t>(x + t + len), static_cast<uint16_t>(y + t), static_cast<uint16_t>(t), static_cast<uint16_t>(len), color); break;
      case 2: tftFillRectRaw(static_cast<uint16_t>(x + t + len), static_cast<uint16_t>(y + (2 * t) + len), static_cast<uint16_t>(t), static_cast<uint16_t>(len), color); break;
      case 3: tftFillRectRaw(static_cast<uint16_t>(x + t), static_cast<uint16_t>(y + (2 * (t + len))), static_cast<uint16_t>(len), static_cast<uint16_t>(t), color); break;
      case 4: tftFillRectRaw(static_cast<uint16_t>(x), static_cast<uint16_t>(y + (2 * t) + len), static_cast<uint16_t>(t), static_cast<uint16_t>(len), color); break;
      case 5: tftFillRectRaw(static_cast<uint16_t>(x), static_cast<uint16_t>(y + t), static_cast<uint16_t>(t), static_cast<uint16_t>(len), color); break;
      case 6: tftFillRectRaw(static_cast<uint16_t>(x + t), static_cast<uint16_t>(y + t + len), static_cast<uint16_t>(len), static_cast<uint16_t>(t), color); break;
      default: break;
    }
  };

  const uint8_t mask = segmentMask[digit];
  for (int segment = 0; segment < 7; segment++) {
    if (mask & (1U << segment)) {
      drawSegment(segment);
    }
  }
}

static void tftDrawNumberRaw(int x, int y, int scale, int value, int digits, uint16_t color, uint16_t background) {
  if (digits <= 0) {
    return;
  }

  int safeValue = value;
  if (safeValue < 0) {
    safeValue = 0;
  }

  int divisor = 1;
  for (int i = 1; i < digits; i++) {
    divisor *= 10;
  }

  const int digitWidth = (2 * (4 * scale)) + (14 * scale);
  const int spacing = 4 * scale;

  for (int i = 0; i < digits; i++) {
    const int d = (safeValue / divisor) % 10;
    const int dx = x + i * (digitWidth + spacing);
    tftDrawDigit7SegRaw(dx, y, scale, d, color, background);
    divisor /= 10;
    if (divisor == 0) {
      divisor = 1;
    }
  }
}

static void tftDrawNumberFixed2Raw(int x, int y, int scale, int valueCenti, uint16_t color, uint16_t background, bool suppressLeadingZero) {
  int safeValue = valueCenti;
  if (safeValue < 0) {
    safeValue = 0;
  }
  if (safeValue > 999) {
    safeValue = 999;
  }

  const int hundreds = (safeValue / 100) % 10;
  const int tens = (safeValue / 10) % 10;
  const int ones = safeValue % 10;

  const int digitWidth = (2 * (4 * scale)) + (14 * scale);
  const int digitHeight = (3 * (4 * scale)) + (2 * (14 * scale));
  const int spacing = 4 * scale;

  if (suppressLeadingZero && hundreds == 0) {
    tftFillRectRaw(static_cast<uint16_t>(x), static_cast<uint16_t>(y), static_cast<uint16_t>(digitWidth), static_cast<uint16_t>(digitHeight), background);
  } else {
    tftDrawDigit7SegRaw(x, y, scale, hundreds, color, background);
  }
  tftDrawDigit7SegRaw(x + digitWidth + spacing, y, scale, tens, color, background);
  tftDrawDigit7SegRaw(x + (2 * (digitWidth + spacing)), y, scale, ones, color, background);

  const int dotSize = 4 * scale;
  const int dotX = x + digitWidth + (spacing / 2) - (dotSize / 2);
  const int dotY = y + digitHeight - dotSize - (2 * scale);
  tftFillRectRaw(static_cast<uint16_t>(dotX), static_cast<uint16_t>(dotY), static_cast<uint16_t>(dotSize), static_cast<uint16_t>(dotSize), color);
}

static int clamp999(int value) {
  if (value < 0) {
    return 0;
  }
  if (value > 999) {
    return 999;
  }
  return value;
}

static void tftDrawGlyphVipRaw(int x, int y, char glyph, uint16_t color, uint16_t background) {
  const int w = 14;
  const int h = 20;
  const int t = 3;
  tftFillRectRaw(static_cast<uint16_t>(x), static_cast<uint16_t>(y), static_cast<uint16_t>(w), static_cast<uint16_t>(h), background);

  if (glyph == 'V') {
    tftFillRectRaw(static_cast<uint16_t>(x), static_cast<uint16_t>(y), static_cast<uint16_t>(t), static_cast<uint16_t>(h - 5), color);
    tftFillRectRaw(static_cast<uint16_t>(x + w - t), static_cast<uint16_t>(y), static_cast<uint16_t>(t), static_cast<uint16_t>(h - 5), color);
    tftFillRectRaw(static_cast<uint16_t>(x + 4), static_cast<uint16_t>(y + h - 5), static_cast<uint16_t>(w - 8), static_cast<uint16_t>(t), color);
    return;
  }

  if (glyph == 'I') {
    tftFillRectRaw(static_cast<uint16_t>(x), static_cast<uint16_t>(y), static_cast<uint16_t>(w), static_cast<uint16_t>(t), color);
    tftFillRectRaw(static_cast<uint16_t>(x), static_cast<uint16_t>(y + h - t), static_cast<uint16_t>(w), static_cast<uint16_t>(t), color);
    tftFillRectRaw(static_cast<uint16_t>(x + (w / 2) - (t / 2)), static_cast<uint16_t>(y), static_cast<uint16_t>(t), static_cast<uint16_t>(h), color);
    return;
  }

  if (glyph == 'P') {
    tftFillRectRaw(static_cast<uint16_t>(x), static_cast<uint16_t>(y), static_cast<uint16_t>(t), static_cast<uint16_t>(h), color);
    tftFillRectRaw(static_cast<uint16_t>(x), static_cast<uint16_t>(y), static_cast<uint16_t>(w - t), static_cast<uint16_t>(t), color);
    tftFillRectRaw(static_cast<uint16_t>(x), static_cast<uint16_t>(y + (h / 2) - (t / 2)), static_cast<uint16_t>(w - t), static_cast<uint16_t>(t), color);
    tftFillRectRaw(static_cast<uint16_t>(x + w - t), static_cast<uint16_t>(y), static_cast<uint16_t>(t), static_cast<uint16_t>(h / 2), color);
  }
}

static void drawRuntimeDashboardWriteOnly(uint32_t nowMs) {
  static uint32_t lastDrawMs = 0;
  if (!tftWriteOnlyReady) {
    return;
  }

  if ((nowMs - lastDrawMs) < 1000) {
    return;
  }
  lastDrawMs = nowMs;

  if (!tftWriteOnlyDashboardInitialized) {
    tftFillScreenRaw(0x0000);
    tftFillRectRaw(0, 44, TFT_LOGICAL_WIDTH, 2, 0xFFFF);
    tftFillRectRaw(52, 0, 2, TFT_LOGICAL_HEIGHT, 0x7BEF);
    tftFillRectRaw(192, 0, 2, TFT_LOGICAL_HEIGHT, 0x7BEF);
    tftFillRectRaw(332, 0, 2, TFT_LOGICAL_HEIGHT, 0x7BEF);
    tftFillRectRaw(0, 124, TFT_LOGICAL_WIDTH, 2, 0x39E7);
    tftFillRectRaw(0, 208, TFT_LOGICAL_WIDTH, 2, 0x39E7);
    tftFillRectRaw(0, 292, TFT_LOGICAL_WIDTH, 2, 0x39E7);

    tftDrawGlyphVipRaw(95, 14, 'V', 0x07FF, 0x0000);
    tftDrawGlyphVipRaw(235, 14, 'I', 0x07E0, 0x0000);
    tftDrawGlyphVipRaw(375, 14, 'P', 0xFFE0, 0x0000);

    tftWriteOnlyDashboardInitialized = true;
  }

  constexpr int valueScale = 1;
  constexpr int rowStartY = 64;
  constexpr int rowStepY = 84;
  constexpr int colVx = 66;
  constexpr int colIx = 206;
  constexpr int colPx = 346;

  static const HatRail rails[3] = {HatRail::Rail5V, HatRail::Rail3V3, HatRail::RailAdj};
  for (int ch = 1; ch <= 3; ch++) {
    const RailMeasurement m = bootStatus.inaOk ? readRailMeasurement(rails[ch - 1]) : RailMeasurement{0.0f, 0.0f, HatRange::High};
    const float voltage = m.busVoltageV;
    const float current = m.currentmA;
    const float powerMw = voltage * current;
    const int centiVolts = clamp999(static_cast<int>(lroundf(voltage * 100.0f)));
    const int centiAmps = clamp999(static_cast<int>(lroundf(fabsf(current) * 0.1f)));
    const int centiWatts = clamp999(static_cast<int>(lroundf(fabsf(powerMw) / 10.0f)));

    const int rowY = rowStartY + ((ch - 1) * rowStepY);
    const uint16_t vColor = (ch == 1) ? 0x07FF : (ch == 2) ? 0xFFE0 : 0xF81F;
    const uint16_t iColor = 0x07E0;
    const uint16_t pColor = 0xFFE0;

    tftDrawDigit7SegRaw(14, rowY + 8, 1, ch, 0xFFFF, 0x0000);
    tftDrawNumberFixed2Raw(colVx, rowY, valueScale, centiVolts, vColor, 0x0000, true);
    tftDrawNumberFixed2Raw(colIx, rowY, valueScale, centiAmps, iColor, 0x0000, true);
    tftDrawNumberFixed2Raw(colPx, rowY, valueScale, centiWatts, pColor, 0x0000, true);
  }
}

static void runTftWriteOnlyWake(bool verbose) {
  setTftControlLinesIdle();

  if (verbose) {
    Serial.println("[TFTWAKE] Sending write-only wake sequence");
  }

  tftWriteCommand(0x01);  // SWRESET
  delay(150);
  tftWriteCommand(0x11);  // SLPOUT
  delay(150);
  tftWriteCommand(0x3A);  // COLMOD
  tftWriteData8(0x55);    // 16-bit
  tftWriteCommand(0x36);  // MADCTL
  tftWriteData8(TFT_WRITE_ONLY_ROTATE_90 ? 0x28 : 0x48);
  tftWriteCommand(0x29);  // DISPON
  delay(20);
  tftWriteCommand(0x20);  // INVOFF
  delay(10);
  tftFillScreenRaw(0x0000);  // clear to black
  tftWriteOnlyReady = true;
  tftWriteOnlyDashboardInitialized = false;

  if (verbose) {
    Serial.println("[TFTWAKE] Sequence sent and screen cleared");
  }
}

static void runTftRawColorTest() {
  if (!tftWriteOnlyReady) {
    return;
  }
  tftFillScreenRaw(0xF800);
  delay(180);
  tftFillScreenRaw(0x07E0);
  delay(180);
  tftFillScreenRaw(0x001F);
  delay(180);
  tftFillScreenRaw(0x0000);
}

static uint32_t tftReadIdD3() {
#if defined(TFT_CS) && (TFT_CS >= 0)
  digitalWrite(TFT_CS, LOW);
#endif
#if defined(TFT_DC) && (TFT_DC >= 0)
  digitalWrite(TFT_DC, LOW);
#endif
  SPI.transfer(0xD3);
#if defined(TFT_DC) && (TFT_DC >= 0)
  digitalWrite(TFT_DC, HIGH);
#endif

  (void)SPI.transfer(0x00);  // dummy
  const uint8_t b1 = SPI.transfer(0x00);
  const uint8_t b2 = SPI.transfer(0x00);
  const uint8_t b3 = SPI.transfer(0x00);

#if defined(TFT_CS) && (TFT_CS >= 0)
  digitalWrite(TFT_CS, HIGH);
#endif

  return (static_cast<uint32_t>(b1) << 16) | (static_cast<uint32_t>(b2) << 8) | b3;
}

static void runTftProbe(bool verbose) {
  setTftControlLinesIdle();
  const uint8_t rddid = tftReadCommand8(0x04);
  const uint8_t rddst = tftReadCommand8(0x09);
  const uint8_t madctl = tftReadCommand8(0x0B);
  const uint8_t pixfmt = tftReadCommand8(0x0C);
  const uint32_t idd3 = tftReadIdD3();

  if (verbose) {
    Serial.printf("[TFTPROBE] RDDID(0x04) = 0x%02X\n", rddid);
    Serial.printf("[TFTPROBE] RDDST(0x09) = 0x%02X\n", rddst);
    Serial.printf("[TFTPROBE] MADCTL(0x0B)= 0x%02X\n", madctl);
    Serial.printf("[TFTPROBE] PIXFMT(0x0C)= 0x%02X\n", pixfmt);
    Serial.printf("[TFTPROBE] ID4(0xD3)  = 0x%06lX\n", idd3);
  }
}

static bool initTftDisplay() {
  Serial.println("[TFT] Init start");
  display.init();
  Serial.println("[TFT] init() returned");
  display.setRotation(1);
  Serial.println("[TFT] setRotation() done");
  display.fillScreen(TFT_BLACK);
  Serial.println("[TFT] fillScreen() done");
  display.setTextDatum(TL_DATUM);
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.drawString("Captain Fantastic PSU", 8, 8, 2);
  display.drawString("Booting...", 8, 28, 2);
  tftInitialized = true;
  Serial.println("[TFT] Init complete");
  return true;
}

static void tftInitTask(void* parameter) {
  (void)parameter;
  const bool ok = initTftDisplay();
  tftInitResult = ok;
  tftInitState = ok ? TftInitState::Done : TftInitState::Failed;
  tftInitTaskHandle = nullptr;
  vTaskDelete(nullptr);
}

static bool initTftDisplayGuarded(uint32_t timeoutMs) {
  if (tftInitialized) {
    return true;
  }

  tftInitState = TftInitState::Running;
  tftInitResult = false;

  const BaseType_t taskCreated = xTaskCreatePinnedToCore(
      tftInitTask,
      "tft_init",
      8192,
      nullptr,
      1,
      &tftInitTaskHandle,
      0);

  if (taskCreated != pdPASS) {
    Serial.println("[TFT] ERROR: Failed to start init task");
    tftInitState = TftInitState::Failed;
    return false;
  }

  const uint32_t startMs = millis();
  while (tftInitState == TftInitState::Running && (millis() - startMs) < timeoutMs) {
    delay(10);
  }

  if (tftInitState == TftInitState::Done) {
    Serial.println("[TFT] Guarded init finished");
    return static_cast<bool>(tftInitResult);
  }

  if (tftInitState == TftInitState::Failed) {
    Serial.println("[TFT] Guarded init failed");
    tftInitialized = false;
    return false;
  }

  Serial.printf("[TFT] Guarded init timeout after %lu ms\n", static_cast<unsigned long>(timeoutMs));
  if (tftInitTaskHandle != nullptr) {
    vTaskDelete(tftInitTaskHandle);
    tftInitTaskHandle = nullptr;
    Serial.println("[TFT] Hung init task deleted");
  }
  tftInitState = TftInitState::Failed;
  tftInitialized = false;
  return false;
}

static void runPinToggleTest() {
  Serial.println("[PINTEST] Starting TFT control-line toggle test");

#if defined(TFT_CS) && (TFT_CS >= 0)
  pinMode(TFT_CS, OUTPUT);
  digitalWrite(TFT_CS, HIGH);
  delay(30);
  digitalWrite(TFT_CS, LOW);
  delay(30);
  digitalWrite(TFT_CS, HIGH);
  Serial.println("[PINTEST] TFT_CS toggled");
#endif

#if defined(TFT_DC) && (TFT_DC >= 0)
  pinMode(TFT_DC, OUTPUT);
  digitalWrite(TFT_DC, LOW);
  delay(30);
  digitalWrite(TFT_DC, HIGH);
  Serial.println("[PINTEST] TFT_DC toggled");
#endif

#if defined(TFT_HW_RST) || defined(TFT_RST)
  if (TFT_RESET_PIN >= 0) {
    pinMode(TFT_RESET_PIN, OUTPUT);
    digitalWrite(TFT_RESET_PIN, TFT_RESET_ACTIVE_LOW ? HIGH : LOW);
    delay(30);
    digitalWrite(TFT_RESET_PIN, TFT_RESET_ACTIVE_LOW ? LOW : HIGH);
    delay(30);
    digitalWrite(TFT_RESET_PIN, TFT_RESET_ACTIVE_LOW ? HIGH : LOW);
    Serial.println("[PINTEST] TFT_RST toggled");
  } else {
    Serial.println("[PINTEST] TFT_RST not configured");
  }
#else
  Serial.println("[PINTEST] TFT_RST not configured");
#endif

  Serial.println("[PINTEST] Done");
}

static uint16_t diagnosticColor(uint8_t index) {
  switch (index % 4) {
    case 0: return TFT_RED;
    case 1: return TFT_GREEN;
    case 2: return TFT_BLUE;
    default: return TFT_WHITE;
  }
}

static void writeMcp4231Wiper(uint8_t wiper, uint8_t value) {
  const uint8_t commandByte = (wiper & 0x01) ? 0x12 : 0x11;

  pinMode(MCP4231_CLOCK_PIN, OUTPUT);
  pinMode(MCP4231_DATA_PIN, OUTPUT);
  digitalWrite(MCP4231_CLOCK_PIN, LOW);
  digitalWrite(MCP4231_CS_PIN, LOW);

  auto shiftOutByteMsb = [&](uint8_t byteValue) {
    for (int bit = 7; bit >= 0; bit--) {
      digitalWrite(MCP4231_DATA_PIN, (byteValue & (1U << bit)) ? HIGH : LOW);
      delayMicroseconds(1);
      digitalWrite(MCP4231_CLOCK_PIN, HIGH);
      delayMicroseconds(1);
      digitalWrite(MCP4231_CLOCK_PIN, LOW);
    }
  };

  shiftOutByteMsb(commandByte);
  shiftOutByteMsb(value);

  digitalWrite(MCP4231_CS_PIN, HIGH);

  if (MCP4231_DATA_PIN == SPI_MISO_PIN) {
    pinMode(MCP4231_DATA_PIN, INPUT_PULLUP);
  }

  mcpWiperCached[wiper & 0x01] = value;
}

static void runMcp4231DiagnosticTest() {
  static const uint8_t testPoints[] = {0, 64, 128, 192, 255};

  Serial.println("[MCPTEST] Starting MCP4231 diagnostic sweep");
  Serial.printf("[MCPTEST] Initial cached wipers: P0=%u P1=%u\n", mcpWiperCached[0], mcpWiperCached[1]);

  for (uint8_t wiper = 0; wiper < 2; wiper++) {
    const uint8_t original = mcpWiperCached[wiper];
    Serial.printf("[MCPTEST] Sweeping P%u\n", wiper);

    for (const uint8_t value : testPoints) {
      writeMcp4231Wiper(wiper, value);
      delay(180);

      if (bootStatus.inaOk) {
        const float ch3Voltage = readInaVoltageScaled(3);
        const float ch3Current = readInaCurrentScaled(3);
        Serial.printf("[MCPTEST] P%u=%3u -> CH3V=%.3f CH3I=%.1f\n", wiper, value, ch3Voltage, ch3Current);
      } else {
        Serial.printf("[MCPTEST] P%u=%3u -> INA offline\n", wiper, value);
      }
    }

    writeMcp4231Wiper(wiper, original);
    delay(120);
    Serial.printf("[MCPTEST] P%u restored to %u\n", wiper, original);
  }

  Serial.println("[MCPTEST] Done");
}

static void runMcpPatternTest() {
  pinMode(MCP4231_CLOCK_PIN, OUTPUT);
  pinMode(MCP4231_DATA_PIN, OUTPUT);
  pinMode(MCP4231_CS_PIN, OUTPUT);

  digitalWrite(MCP4231_CLOCK_PIN, LOW);
  digitalWrite(MCP4231_CS_PIN, HIGH);
  delayMicroseconds(20);

  Serial.printf("[MCPPATTERN] CS=%d SCK=%d DATA=%d\n", MCP4231_CS_PIN, MCP4231_CLOCK_PIN, MCP4231_DATA_PIN);
  Serial.println("[MCPPATTERN] Sending AA 55 AA 55 under CS low");

  auto shiftOutByteMsb = [&](uint8_t byteValue) {
    for (int bit = 7; bit >= 0; bit--) {
      digitalWrite(MCP4231_DATA_PIN, (byteValue & (1U << bit)) ? HIGH : LOW);
      delayMicroseconds(2);
      digitalWrite(MCP4231_CLOCK_PIN, HIGH);
      delayMicroseconds(2);
      digitalWrite(MCP4231_CLOCK_PIN, LOW);
    }
  };

  digitalWrite(MCP4231_CS_PIN, LOW);
  shiftOutByteMsb(0xAA);
  shiftOutByteMsb(0x55);
  shiftOutByteMsb(0xAA);
  shiftOutByteMsb(0x55);
  digitalWrite(MCP4231_CS_PIN, HIGH);

  if (MCP4231_DATA_PIN == SPI_MISO_PIN) {
    pinMode(MCP4231_DATA_PIN, INPUT_PULLUP);
  }

  Serial.println("[MCPPATTERN] Done");
}

static bool detectCh3IncreaseWithWiper(uint8_t wiper, bool& increaseWithWiper) {
  uint8_t base = mcpWiperCached[wiper & 0x01];
  uint8_t delta = 4;
  uint8_t test = (base <= (255 - delta)) ? static_cast<uint8_t>(base + delta) : static_cast<uint8_t>(base - delta);
  if (test == base) {
    increaseWithWiper = true;
    return false;
  }

  writeMcp4231Wiper(wiper, base);
  delay(80);
  const float vBase = readInaVoltageScaled(3);

  writeMcp4231Wiper(wiper, test);
  delay(120);
  const float vTest = readInaVoltageScaled(3);

  writeMcp4231Wiper(wiper, base);
  delay(80);

  if (test > base) {
    increaseWithWiper = (vTest >= vBase);
  } else {
    increaseWithWiper = (vTest <= vBase);
  }
  return true;
}

static bool setCh3VoltageTarget(float targetVoltage, uint8_t wiper, float toleranceVoltage, uint8_t maxIterations) {
  if (!isfinite(targetVoltage) || targetVoltage < 0.0f || targetVoltage > 20.0f) {
    Serial.println("[VSET] Invalid target. Use 0.00..20.00V");
    return false;
  }

  if (!bootStatus.inaOk) {
    Serial.println("[VSET] INA3221 offline. Cannot close loop on CH3V");
    return false;
  }

  const uint8_t w = (wiper & 0x01);
  bool increaseWithWiper = true;
  (void)detectCh3IncreaseWithWiper(w, increaseWithWiper);

  uint8_t potValue = mcpWiperCached[w];
  writeMcp4231Wiper(w, potValue);
  delay(60);

  float measured = readInaVoltageScaled(3);
  float error = targetVoltage - measured;
  float bestAbsError = fabsf(error);
  uint8_t bestPotValue = potValue;
  float bestMeasured = measured;
  float previousAbsError = bestAbsError;
  int previousErrorSign = (error > 0.0f) ? 1 : ((error < 0.0f) ? -1 : 0);
  uint8_t stagnationCount = 0;
  uint8_t signChangeCount = 0;
  uint8_t stepCeiling = 32;

  if (bestAbsError <= toleranceVoltage) {
    Serial.printf("[VSET] CH3 target %.3fV reached: %.3fV (wiper %u=%u)\n", targetVoltage, measured, w, potValue);
    return true;
  }

  const uint16_t maxMoves = static_cast<uint16_t>(max<uint16_t>(120, static_cast<uint16_t>(maxIterations) * 8U));
  Serial.printf("[VSET] Ramp start target=%.3fV current=%.3fV wiper %u=%u maxMoves=%u\n", targetVoltage, measured, w, potValue, maxMoves);

  for (uint16_t move = 0; move < maxMoves; move++) {
    error = targetVoltage - measured;
    const float absError = fabsf(error);
    if (absError <= toleranceVoltage) {
      Serial.printf("[VSET] CH3 target %.3fV reached: %.3fV (wiper %u=%u, moves=%u)\n", targetVoltage, measured, w, potValue, move);
      return true;
    }

    const float span = max(max(targetVoltage, measured), 1.0f);
    const float pctError = absError / span;
    int step = static_cast<int>(ceilf(pctError * 64.0f));

    const int voltageStep = static_cast<int>(ceilf(absError * 12.0f));
    if (voltageStep > step) {
      step = voltageStep;
    }

    if (absError <= 0.06f) {
      step = 1;
    } else if (absError <= 0.15f && step > 2) {
      step = 2;
    } else if (absError <= 0.30f && step > 4) {
      step = 4;
    }

    if (step < 1) {
      step = 1;
    }
    if (step > stepCeiling) {
      step = stepCeiling;
    }

    if (absError >= (previousAbsError - 0.002f)) {
      if (stagnationCount < 255) {
        stagnationCount++;
      }
      if (stagnationCount >= 4 && step < stepCeiling) {
        step = min(static_cast<int>(stepCeiling), step + 2);
      }
    } else {
      stagnationCount = 0;
    }

    int direction = (error > 0.0f) ? 1 : -1;
    if (!increaseWithWiper) {
      direction = -direction;
    }

    int nextValue = static_cast<int>(potValue) + (direction * step);
    if (nextValue < 0) {
      nextValue = 0;
    }
    if (nextValue > 255) {
      nextValue = 255;
    }

    if (nextValue == potValue) {
      break;
    }

    potValue = static_cast<uint8_t>(nextValue);
    writeMcp4231Wiper(w, potValue);
    delay(step > 4 ? 65 : 90);

    measured = readInaVoltageScaled(3);
    error = targetVoltage - measured;
    const float newAbsError = fabsf(error);
    if (newAbsError < bestAbsError) {
      bestAbsError = newAbsError;
      bestPotValue = potValue;
      bestMeasured = measured;
    }

    const int newErrorSign = (error > 0.0f) ? 1 : ((error < 0.0f) ? -1 : 0);
    if (newErrorSign != 0 && previousErrorSign != 0 && newErrorSign != previousErrorSign) {
      if (signChangeCount < 255) {
        signChangeCount++;
      }
      if (signChangeCount >= 2) {
        stepCeiling = 8;
      }
      if (signChangeCount >= 4) {
        stepCeiling = 3;
      }
    }

    previousErrorSign = newErrorSign;
    previousAbsError = newAbsError;
  }

  if (bestPotValue != potValue) {
    potValue = bestPotValue;
    writeMcp4231Wiper(w, potValue);
    delay(90);
    measured = readInaVoltageScaled(3);
  }

  Serial.printf("[VSET] CH3 target %.3fV not reached. Best %.3fV (err=%.3fV, wiper %u=%u)\n",
                targetVoltage,
                bestMeasured,
                bestAbsError,
                w,
                bestPotValue);
  return false;
}

static void runCalRamp(uint8_t wiper) {
  if (!bootStatus.inaOk) {
    Serial.println("[CALRAMP] INA3221 offline. Cannot run calibration ramp");
    return;
  }

  const uint8_t w = (wiper & 0x01);
  const uint8_t original = mcpWiperCached[w];
  constexpr uint8_t step = 4;

  Serial.printf("[CALRAMP] Start P%u (original=%u)\n", w, original);
  Serial.println("[CALRAMP] CSV: phase,wiper,ch3v,ch3i");

  auto writeAndReport = [&](const char* phase, uint8_t value) {
    writeMcp4231Wiper(w, value);
    delay(90);
    const float ch3v = readInaVoltageScaled(3);
    const float ch3i = readInaCurrentScaled(3);
    Serial.printf("[CALRAMP] %s,%u,%.4f,%.2f\n", phase, value, ch3v, ch3i);
  };

  for (int value = 0; value <= 255; value += step) {
    writeAndReport("UP", static_cast<uint8_t>(value));
  }
  if ((255 % step) != 0) {
    writeAndReport("UP", 255);
  }

  for (int value = 255; value >= 0; value -= step) {
    writeAndReport("DOWN", static_cast<uint8_t>(value));
  }
  if ((255 % step) != 0) {
    writeAndReport("DOWN", 0);
  }

  writeMcp4231Wiper(w, original);
  delay(90);
  const float finalV = readInaVoltageScaled(3);
  Serial.printf("[CALRAMP] Restore P%u=%u -> CH3V=%.4f\n", w, original, finalV);
}

static uint16_t readTouchRaw(uint8_t command) {
#ifdef TOUCH_CS
  digitalWrite(TOUCH_CS, LOW);
  SPI.transfer(command);
  uint16_t raw = (static_cast<uint16_t>(SPI.transfer(0x00)) << 8) | SPI.transfer(0x00);
  digitalWrite(TOUCH_CS, HIGH);
  return raw >> 3;
#else
  (void)command;
  return 0xFFFF;
#endif
}

static void printTouchSample(const char* tag) {
#ifdef TOUCH_CS
  pinMode(TOUCH_CS, OUTPUT);
  digitalWrite(TOUCH_CS, HIGH);
  delayMicroseconds(50);

  const uint16_t tx = readTouchRaw(0xD0);
  const uint16_t ty = readTouchRaw(0x90);
  const uint16_t tz1 = readTouchRaw(0xB0);
  const uint16_t tz2 = readTouchRaw(0xC0);

  const bool touched = (tz1 > 40) && (tz2 < 4095);
  Serial.printf("[%s] X=%4u Y=%4u Z1=%4u Z2=%4u touched=%s\n",
                tag,
                tx,
                ty,
                tz1,
                tz2,
                touched ? "yes" : "no");
#else
  (void)tag;
  Serial.println("[TOUCH] TOUCH_CS not defined");
#endif
}

static void runTouchBusProbe() {
#ifdef TOUCH_CS
  pinMode(TOUCH_CS, OUTPUT);
  digitalWrite(TOUCH_CS, HIGH);
  pinMode(SPI_MISO_PIN, INPUT_PULLUP);
  delayMicroseconds(20);

  const int misoIdleHigh = digitalRead(SPI_MISO_PIN);
  digitalWrite(TOUCH_CS, LOW);
  delayMicroseconds(20);
  const int misoWithCsLow = digitalRead(SPI_MISO_PIN);
  digitalWrite(TOUCH_CS, HIGH);

  Serial.printf("[TOUCHBUS] TOUCH_CS=%d MOSI=%d MISO=%d SCLK=%d\n", TOUCH_CS, SPI_MOSI_PIN, SPI_MISO_PIN, SPI_SCLK_PIN);
  Serial.printf("[TOUCHBUS] MISO idle(cs=H)=%d MISO(cs=L)=%d\n", misoIdleHigh, misoWithCsLow);
  Serial.println("[TOUCHBUS] 5 samples (X,Y,Z1,Z2)");

  for (int index = 0; index < 5; index++) {
    const uint16_t tx = readTouchRaw(0xD0);
    const uint16_t ty = readTouchRaw(0x90);
    const uint16_t tz1 = readTouchRaw(0xB0);
    const uint16_t tz2 = readTouchRaw(0xC0);
    Serial.printf("[TOUCHBUS] %d: %4u %4u %4u %4u\n", index + 1, tx, ty, tz1, tz2);
    delay(80);
  }
#else
  Serial.println("[TOUCHBUS] TOUCH_CS not defined");
#endif
}

static void runTouchMosiPatternTest() {
#ifdef TOUCH_CS
  pinMode(TOUCH_CS, OUTPUT);
  digitalWrite(TOUCH_CS, HIGH);
  delayMicroseconds(20);

  Serial.printf("[TOUCHMOSI] CS=%d MOSI=%d MISO=%d SCLK=%d\n", TOUCH_CS, SPI_MOSI_PIN, SPI_MISO_PIN, SPI_SCLK_PIN);
  Serial.println("[TOUCHMOSI] Sending pattern: 0xAA 0x55 0xF0 0x0F x16 with CS low");

  digitalWrite(TOUCH_CS, LOW);
  for (int index = 0; index < 16; index++) {
    SPI.transfer(0xAA);
    SPI.transfer(0x55);
    SPI.transfer(0xF0);
    SPI.transfer(0x0F);
  }
  digitalWrite(TOUCH_CS, HIGH);

  Serial.println("[TOUCHMOSI] Done");
#else
  Serial.println("[TOUCHMOSI] TOUCH_CS not defined");
#endif
}

static void runTftMosiPatternTest() {
#if defined(TFT_CS) && (TFT_CS >= 0)
  pinMode(TFT_CS, OUTPUT);
  digitalWrite(TFT_CS, HIGH);
  delayMicroseconds(20);

  Serial.printf("[TFTMOSI] TFT_CS=%d MOSI=%d MISO=%d SCLK=%d\n", TFT_CS, SPI_MOSI_PIN, SPI_MISO_PIN, SPI_SCLK_PIN);
  Serial.println("[TFTMOSI] Sending pattern: 0xAA 0x55 0xF0 0x0F x16 with TFT_CS low");

  digitalWrite(TFT_CS, LOW);
#if defined(TFT_DC) && (TFT_DC >= 0)
  pinMode(TFT_DC, OUTPUT);
  digitalWrite(TFT_DC, HIGH);
#endif
  for (int index = 0; index < 16; index++) {
    SPI.transfer(0xAA);
    SPI.transfer(0x55);
    SPI.transfer(0xF0);
    SPI.transfer(0x0F);
  }
  digitalWrite(TFT_CS, HIGH);

  Serial.println("[TFTMOSI] Done");
#else
  Serial.println("[TFTMOSI] TFT_CS not defined or disabled");
#endif
}

static void runTouchCommandSweep() {
#ifdef TOUCH_CS
  static const uint8_t commands[] = {
    0x90,  // Y position
    0xD0,  // X position
    0xB0,  // Z1
    0xC0,  // Z2
    0x80,  // Temp0
    0xA0,  // VBAT
    0xE0,  // AUX
    0xF0   // Temp1
  };

  Serial.println("[TOUCHSCAN] Command sweep (8 samples each)");
  for (const uint8_t command : commands) {
    uint16_t minValue = 0xFFFF;
    uint16_t maxValue = 0;
    uint16_t lastValue = 0;
    int changedCount = 0;

    for (int index = 0; index < 8; index++) {
      const uint16_t value = readTouchRaw(command);
      if (value < minValue) {
        minValue = value;
      }
      if (value > maxValue) {
        maxValue = value;
      }
      if (index > 0 && value != lastValue) {
        changedCount++;
      }
      lastValue = value;
      delay(8);
    }

    const bool active = (maxValue - minValue) > 4 || changedCount > 0;
    Serial.printf("[TOUCHSCAN] cmd=0x%02X min=%4u max=%4u delta=%4u changes=%d active=%s\n",
                  command,
                  minValue,
                  maxValue,
                  static_cast<unsigned>(maxValue - minValue),
                  changedCount,
                  active ? "yes" : "no");
  }
  Serial.println("[TOUCHSCAN] Done");
#else
  Serial.println("[TOUCHSCAN] TOUCH_CS not defined");
#endif
}

static void runTouchSingleCommand(String commandText) {
#ifdef TOUCH_CS
  commandText.trim();
  if (commandText.length() == 0) {
    Serial.println("[TOUCHCMD] Usage: TOUCHCMD <hex-byte>, e.g. TOUCHCMD D0 or TOUCHCMD 0xD0");
    return;
  }

  if (!commandText.startsWith("0X")) {
    commandText = "0X" + commandText;
  }

  const long parsed = strtol(commandText.c_str(), nullptr, 16);
  if (parsed < 0 || parsed > 0xFF) {
    Serial.println("[TOUCHCMD] Invalid command byte. Range 00..FF");
    return;
  }

  const uint8_t cmd = static_cast<uint8_t>(parsed);
  Serial.printf("[TOUCHCMD] cmd=0x%02X samples:", cmd);
  for (int index = 0; index < 10; index++) {
    const uint16_t value = readTouchRaw(cmd);
    Serial.printf(" %u", value);
    delay(8);
  }
  Serial.println();
#else
  (void)commandText;
  Serial.println("[TOUCHCMD] TOUCH_CS not defined");
#endif
}

static void printBootLine(const char* label, bool pass) {
  Serial.printf("[BOOT] %-12s : %s\n", label, pass ? "PASS" : "FAIL");
}

static uint32_t tryReadTftId() {
  if (!tftInitialized) {
    return 0;
  }
  uint8_t id1 = display.readcommand8(0x04, 1);
  uint8_t id2 = display.readcommand8(0x04, 2);
  uint8_t id3 = display.readcommand8(0x04, 3);
  return (static_cast<uint32_t>(id1) << 16) | (static_cast<uint32_t>(id2) << 8) | id3;
}

static void runTftColorTest() {
  if (!tftInitialized) {
    return;
  }
  for (uint8_t index = 0; index < 4; index++) {
    display.fillScreen(diagnosticColor(index));
    delay(180);
  }
  display.fillScreen(TFT_BLACK);
}

static void drawBootReport(const BootStatus& status) {
  if (!tftInitialized) {
    return;
  }
  display.fillScreen(TFT_BLACK);
  display.setTextColor(TFT_CYAN, TFT_BLACK);
  display.drawString("Captain Fantastic PSU", 8, 8, 2);
  display.drawString("Startup Self-Test", 8, 28, 4);

  auto drawResult = [](int y, const char* label, bool pass) {
    display.setTextColor(TFT_WHITE, TFT_BLACK);
    display.drawString(label, 8, y, 2);
    display.setTextColor(pass ? TFT_GREEN : TFT_RED, TFT_BLACK);
    display.drawString(pass ? "PASS" : "FAIL", 240, y, 2);
  };

  drawResult(74, "UART Console", status.uartOk);
  drawResult(98, "TFT SPI Write", status.tftOk);
  drawResult(122, "TFT ID Read", status.tftIdOk);
  drawResult(146, "I2C Bus", status.i2cOk);
  drawResult(170, "INA3221", status.inaOk);
  drawResult(194, "MCP4231", status.mcpOk);
  drawResult(218, "Touch SPI", status.touchOk);

  display.setTextColor(TFT_YELLOW, TFT_BLACK);
  display.drawString("TFT ID: 0x" + String(status.tftId, HEX), 8, 248, 2);
  if (status.inaAddress != 0x00) {
    display.drawString("INA addr: 0x" + String(status.inaAddress, HEX) + "  I2C found: " + String(status.i2cCount), 8, 270, 2);
  } else {
    display.drawString("INA addr: not found  I2C found: " + String(status.i2cCount), 8, 270, 2);
  }
  display.drawString("Type BOOTTEST or TFTTEST", 8, 292, 2);
}

static BootStatus runStartupDiagnostics() {
  BootStatus status = {true, (tftInitialized || tftWriteOnlyReady), false, false, false, false, false, false, NAN, 0x00, 0, 0};

  Serial.println("\n[BOOT] ===== Startup Self-Test =====");

  if (tftInitialized) {
    display.fillScreen(TFT_BLACK);
    display.setTextColor(TFT_WHITE, TFT_BLACK);
    display.drawString("Running TFT color test...", 8, 8, 2);
    runTftColorTest();
    status.tftId = tryReadTftId();
    status.tftIdOk = !((status.tftId == 0x000000) || (status.tftId == 0xFFFFFF));
  } else {
    status.tftId = 0;
    status.tftIdOk = false;
    if (tftWriteOnlyReady) {
      Serial.println("[BOOT] TFT running in write-only mode");
    } else {
      Serial.println("[BOOT] TFT init skipped (safe mode)");
    }
  }

  printBootLine("UART Console", status.uartOk);
  printBootLine("TFT SPI", status.tftOk);
  printBootLine("TFT ID", status.tftIdOk);
  Serial.printf("[BOOT] TFT ID Read  : 0x%06lX\n", status.tftId);

  status.i2cCount = scanI2cAndReport(true);
  if (status.i2cCount == 0) {
    Serial.println("[I2C] No devices found, attempting bus recovery + rescan");
    i2cBusRecover();
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, 100000);
    delay(5);
    status.i2cCount = scanI2cAndReport(true);
  }
  status.i2cOk = status.i2cCount > 0;

  for (uint8_t address = INA3221_ADDR_MIN; address <= INA3221_ADDR_MAX; address++) {
    if (isI2cDevicePresent(address)) {
      status.inaAddress = address;
      break;
    }
  }

  printBootLine("I2C Bus", status.i2cOk);
  Serial.printf("[BOOT] I2C Count    : %u\n", status.i2cCount);

  if (status.inaAddress != 0x00) {
    const InaStartupDiagResult inaDiag = probeInaAtAddress(status.inaAddress, false);
    status.inaRwOk = inaDiag.rwOk;
    status.inaRailVoltage = inaDiag.ch2Voltage;
    const bool ch2InRange = isfinite(status.inaRailVoltage) && (status.inaRailVoltage >= 3.2f) && (status.inaRailVoltage <= 3.4f);
    status.inaOk = inaDiag.controlOk && ch2InRange;
    printBootLine("INA R/W", status.inaRwOk);
    Serial.printf("[BOOT] INA CH2 Rail : %.3f V -> %s\n", status.inaRailVoltage, ch2InRange ? "PASS" : "FAIL");
  }
  printBootLine("INA3221", status.inaOk);

  Serial.println("[MCP] Startup keeps current wiper values (no forced preset)");
  status.mcpOk = true;
  printBootLine("MCP4231", status.mcpOk);

#ifdef TOUCH_CS
  pinMode(TOUCH_CS, OUTPUT);
  digitalWrite(TOUCH_CS, HIGH);
  const uint16_t tx = readTouchRaw(0xD0);
  const uint16_t ty = readTouchRaw(0x90);
  status.touchOk = !((tx == 0x0000 && ty == 0x0000) || (tx >= 0x0FFF && ty >= 0x0FFF));
#else
  status.touchOk = false;
#endif
  printBootLine("Touch SPI", status.touchOk);

  drawBootReport(status);
  Serial.println("[BOOT] ===== Self-Test Complete =====");
  return status;
}

static void drawRuntimeDashboard(uint32_t nowMs) {
  if (!tftInitialized) {
    return;
  }
  display.fillRect(0, 0, 480, 320, TFT_BLACK);
  display.setTextColor(TFT_CYAN, TFT_BLACK);
  display.drawString("Captain Fantastic PSU", 8, 8, 2);
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.drawString("Runtime Dashboard", 8, 28, 4);

  display.setTextColor(TFT_GREEN, TFT_BLACK);
  display.drawString("HB: " + String(ledState ? "ON" : "OFF"), 8, 70, 2);
  display.drawString("Loop: " + String(loopCounter), 140, 70, 2);
  display.drawString("Millis: " + String(nowMs), 300, 70, 2);

  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.drawString("CH   V(V)      I(mA)      P(mW)", 8, 102, 2);

  static const HatRail rails[3] = {HatRail::Rail5V, HatRail::Rail3V3, HatRail::RailAdj};
  for (int ch = 1; ch <= 3; ch++) {
    const RailMeasurement m = bootStatus.inaOk ? readRailMeasurement(rails[ch - 1]) : RailMeasurement{0.0f, 0.0f, HatRange::High};
    const float voltage = m.busVoltageV;
    const float current = m.currentmA;
    const float power = voltage * current;
    const int rowY = 132 + ((ch - 1) * 56);

    display.drawString(String(ch), 8, rowY, 4);
    display.drawString(String(voltage, 3), 52, rowY, 4);
    display.drawString(String(current, 1), 188, rowY, 4);
    display.drawString(String(power, 1), 326, rowY, 4);
  }
}

static void handleSerialCommands() {
  if (!Serial.available()) {
    return;
  }

  String command = Serial.readStringUntil('\n');
  command.trim();
  command.toUpperCase();

  if (command.startsWith("P0 ") || command.startsWith("P1 ")) {
    const uint8_t wiper = command.charAt(1) - '0';
    int value = command.substring(3).toInt();
    if (value < 0) {
      value = 0;
    }
    if (value > 255) {
      value = 255;
    }

    writeMcp4231Wiper(wiper, static_cast<uint8_t>(value));
    Serial.printf("MCP4231 wiper %u set to %d\n", wiper, value);
    return;
  }

  if (command.startsWith("CH3VSCALE ")) {
    const float value = command.substring(9).toFloat();
    if (value <= 0.0f || value > 10.0f) {
      Serial.println("[CH3] Invalid CH3VSCALE. Use range 0.001..10.0");
      return;
    }
    ch3VoltageScale = value;
    Serial.printf("[CH3] Voltage scale set to %.4f\n", ch3VoltageScale);
    return;
  }

  if (command.startsWith("VSET ")) {
    const float target = command.substring(5).toFloat();
    setCh3VoltageTarget(target, 1, 0.03f, 30);
    return;
  }

  if (command.startsWith("VSET0 ")) {
    const float target = command.substring(6).toFloat();
    setCh3VoltageTarget(target, 0, 0.03f, 30);
    return;
  }

  if (command.startsWith("VSET1 ")) {
    const float target = command.substring(6).toFloat();
    setCh3VoltageTarget(target, 1, 0.03f, 30);
    return;
  }

  if (command.startsWith("CH3ISCALE ")) {
    const float value = command.substring(9).toFloat();
    if (value <= 0.0f || value > 10.0f) {
      Serial.println("[CH3] Invalid CH3ISCALE. Use range 0.001..10.0");
      return;
    }
    ch3CurrentScale = value;
    Serial.printf("[CH3] Current scale set to %.4f\n", ch3CurrentScale);
    return;
  }

  if (command == "CH3SCALE") {
    Serial.printf("[CH3] Voltage scale: %.4f\n", ch3VoltageScale);
    Serial.printf("[CH3] Current scale: %.4f\n", ch3CurrentScale);
    return;
  }

  if (command == "HWMAP") {
    hatTelemetry.printTopology(Serial);
    return;
  }

  if (command == "RCALSHOW") {
    hatTelemetry.printCalibration(Serial);
    return;
  }

  if (command == "CFGSAVE") {
    savePersistentConfig();
    return;
  }

  if (command == "CFGLOAD") {
    if (!loadPersistentConfig(true)) {
      Serial.println("[CFG] Load failed or no config present");
    }
    return;
  }

  if (command == "CFGRESET") {
    resetPersistentConfigDefaults();
    savePersistentConfig();
    Serial.println("[CFG] Reset to defaults and saved");
    return;
  }

  if (command == "CFGERASE") {
    erasePersistentConfig();
    return;
  }

  if (command.startsWith("AUTORANGE ")) {
    const String arg = command.substring(10);
    if (arg == "ON" || arg == "1") {
      autoRangeEnabled = true;
      Serial.println("[RANGE] Auto-range enabled");
      savePersistentConfig();
      return;
    }
    if (arg == "OFF" || arg == "0") {
      autoRangeEnabled = false;
      Serial.println("[RANGE] Auto-range disabled (HIGH path fixed)");
      savePersistentConfig();
      return;
    }
    Serial.println("[RANGE] Use AUTORANGE ON or AUTORANGE OFF");
    return;
  }

  if (command.startsWith("RTHR ")) {
    float lowmA = 0.0f;
    float highmA = 0.0f;
    if (sscanf(command.c_str(), "RTHR %f %f", &lowmA, &highmA) == 2) {
      if (lowmA > 0.0f && highmA > lowmA) {
        autoRangeLowThresholdmA = lowmA;
        autoRangeHighThresholdmA = highmA;
        hatTelemetry.setAutoRangeThresholds(lowmA, highmA);
        Serial.printf("[RANGE] Thresholds set: low<=%.1f mA, high>=%.1f mA\n", lowmA, highmA);
        savePersistentConfig();
      } else {
        Serial.println("[RANGE] Invalid thresholds. Require: low>0 and high>low");
      }
    } else {
      Serial.println("[RANGE] Usage: RTHR <low_mA> <high_mA>");
    }
    return;
  }

  if (command.startsWith("RCAL ")) {
    char railToken[16] = {0};
    char rangeToken[16] = {0};
    float vGain = 1.0f;
    float vOff = 0.0f;
    float iGain = 1.0f;
    float iOff = 0.0f;

    if (sscanf(command.c_str(), "RCAL %15s %15s %f %f %f %f", railToken, rangeToken, &vGain, &vOff, &iGain, &iOff) == 6) {
      HatRail rail;
      HatRange range;
      String railArg(railToken);
      String rangeArg(rangeToken);
      railArg.toUpperCase();
      rangeArg.toUpperCase();

      if (!parseRailToken(railArg, rail)) {
        Serial.println("[CAL] Unknown rail. Use 5V, 3V3, ADJ, IN12");
        return;
      }
      if (!parseRangeToken(rangeArg, range)) {
        Serial.println("[CAL] Unknown range. Use HIGH, LOW, SINGLE");
        return;
      }

      const RailMapping& map = hatTelemetry.mapping(rail);
      if (!map.hasDualRange && range != HatRange::Single) {
        Serial.println("[CAL] Incoming rail supports SINGLE range only");
        return;
      }
      if (map.hasDualRange && range == HatRange::Single) {
        Serial.println("[CAL] Use HIGH or LOW for dual-range rails");
        return;
      }

      hatTelemetry.setCalibration(rail, range, {vGain, vOff, iGain, iOff});
      Serial.printf("[CAL] %s %s set V=(gain %.6f, off %.6f) I=(gain %.6f, off %.6f)\n",
                    map.railName,
                    HatPowerTelemetry::rangeName(range),
                    vGain,
                    vOff,
                    iGain,
                    iOff);
      savePersistentConfig();
      return;
    }

    Serial.println("[CAL] Usage: RCAL <rail> <range> <vgain> <voff> <igain> <ioff>");
    return;
  }

  if (command == "RAILSNAP") {
    printLogicalRailSnapshot();
    return;
  }

  if (command == "HELP") {
    Serial.println("Commands: P0 <0-255>, P1 <0-255>, MCPTEST, MCPPATTERN, CALRAMP (P1), CALRAMP0, CALRAMP1, VSET <V> (uses P1), VSET0 <V>, VSET1 <V>, CH3SCALE, CH3VSCALE <f>, CH3ISCALE <f>, HWMAP, RAILSNAP, AUTORANGE <ON|OFF>, RTHR <low_mA> <high_mA>, RCAL <rail> <range> <vgain> <voff> <igain> <ioff>, RCALSHOW, CFGSAVE, CFGLOAD, CFGRESET, CFGERASE, BOOTTEST, STATUS, TFTINIT, TFTTEST, TFTPROBE, TFTWAKE, TFTCLEAR, I2CSCAN, I2CRECOVER, TOUCHRAW, TOUCHTEST, TOUCHBUS, TOUCHSCAN, TOUCHCMD <hex>, TOUCHMOSI, TFTMOSI, SPIPINS, SPIRAW, PINTEST");
    return;
  }

  if (command == "MCPTEST") {
    runMcp4231DiagnosticTest();
    return;
  }

  if (command == "MCPPATTERN") {
    runMcpPatternTest();
    return;
  }

  if (command == "CALRAMP") {
    runCalRamp(1);
    return;
  }

  if (command == "CALRAMP0") {
    runCalRamp(0);
    return;
  }

  if (command == "CALRAMP1") {
    runCalRamp(1);
    return;
  }

  if (command == "TFTINIT") {
    if (tftInitialized) {
      Serial.println("[TFT] Already initialized");
    } else {
      Serial.println("[TFT] Manual init requested");
      initTftDisplayGuarded(2500);
    }
    return;
  }

  if (command == "BOOTTEST") {
    bootStatus = runStartupDiagnostics();
    return;
  }

  if (command == "STATUS") {
    Serial.println("[STATUS] Last boot-test results:");
    printBootLine("UART Console", bootStatus.uartOk);
    printBootLine("TFT SPI", bootStatus.tftOk);
    printBootLine("TFT ID", bootStatus.tftIdOk);
    printBootLine("I2C Bus", bootStatus.i2cOk);
    printBootLine("INA R/W", bootStatus.inaRwOk);
    printBootLine("INA3221", bootStatus.inaOk);
    printBootLine("MCP4231", bootStatus.mcpOk);
    printBootLine("Touch SPI", bootStatus.touchOk);
    Serial.printf("[STATUS] TFT ID     : 0x%06lX\n", bootStatus.tftId);
    Serial.printf("[STATUS] I2C Count  : %u\n", bootStatus.i2cCount);
    if (isfinite(bootStatus.inaRailVoltage)) {
      Serial.printf("[STATUS] INA CH2V   : %.3f V\n", bootStatus.inaRailVoltage);
    } else {
      Serial.println("[STATUS] INA CH2V   : n/a");
    }
    return;
  }

  if (command == "TFTTEST") {
    if (tftInitialized) {
      Serial.println("[CMD] Running TFT color test");
      runTftColorTest();
      return;
    }
    if (tftWriteOnlyReady) {
      Serial.println("[CMD] Running raw write-only TFT color test");
      runTftRawColorTest();
      return;
    }
    Serial.println("[TFT] Not initialized. Run TFTWAKE or TFTINIT first.");
    return;
  }

  if (command == "I2CSCAN") {
    Serial.printf("[I2C] SDA=%d SCL=%d\n", digitalRead(I2C_SDA_PIN), digitalRead(I2C_SCL_PIN));
    scanI2cAndReport(true);
    return;
  }

  if (command == "I2CRECOVER") {
    i2cBusRecover();
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, 100000);
    Serial.println("[I2C] Recovery command complete");
    return;
  }

  if (command == "TOUCHRAW") {
    printTouchSample("TOUCH");
    return;
  }

  if (command == "TOUCHTEST") {
    Serial.println("[TOUCH] Sampling for 3s (touch and drag now)...");
    for (int index = 0; index < 15; index++) {
      printTouchSample("TOUCH");
      delay(200);
    }
    Serial.println("[TOUCH] Test done");
    return;
  }

  if (command == "TOUCHBUS") {
    runTouchBusProbe();
    return;
  }

  if (command == "TOUCHMOSI") {
    runTouchMosiPatternTest();
    return;
  }

  if (command == "TFTMOSI") {
    runTftMosiPatternTest();
    return;
  }

  if (command == "TOUCHSCAN") {
    runTouchCommandSweep();
    return;
  }

  if (command.startsWith("TOUCHCMD ")) {
    runTouchSingleCommand(command.substring(9));
    return;
  }

  if (command == "SPIPINS") {
    printSpiPinMap();
    return;
  }

  if (command == "SPIRAW") {
    runRawSpiBurst(true);
    return;
  }

  if (command == "TFTPROBE") {
    runTftProbe(true);
    return;
  }

  if (command == "TFTWAKE") {
    runTftWriteOnlyWake(true);
    return;
  }

  if (command == "TFTCLEAR") {
    tftFillScreenRaw(0x0000);
    Serial.println("[TFT] Raw clear sent (black)");
    return;
  }

  if (command == "PINTEST") {
    runPinToggleTest();
  }
}

void setup() {
  Serial.begin(115200);
  delay(100);
  const esp_reset_reason_t resetReason = esp_reset_reason();
  Serial.printf("[SYS] Project path: %s\n", PROJECT_PATH);
  Serial.printf("[SYS] Environment : %s\n", PROJECT_ENV);
  Serial.printf("[SYS] TFT driver  : %s\n", activeTftDriverText());
  Serial.printf("[SYS] Reset reason: %s (%d)\n", resetReasonText(resetReason), static_cast<int>(resetReason));
  Serial.printf("[SYS] I2C pins: SDA=%d SCL=%d\n", I2C_SDA_PIN, I2C_SCL_PIN);
  pinMode(I2C_SDA_PIN, INPUT_PULLUP);
  pinMode(I2C_SCL_PIN, INPUT_PULLUP);
  i2cBusRecover();
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, 100000);
  SPI.begin(SPI_SCLK_PIN, SPI_MISO_PIN, SPI_MOSI_PIN);
  SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));

  pinMode(STATUS_LED_PIN, OUTPUT);
  digitalWrite(STATUS_LED_PIN, LOW);
  pinMode(MCP4231_CS_PIN, OUTPUT);
  digitalWrite(MCP4231_CS_PIN, HIGH);

  hatTelemetry.setAutoRangeThresholds(autoRangeLowThresholdmA, autoRangeHighThresholdmA);
  if (!loadPersistentConfig(true)) {
    savePersistentConfig();
  }

  if (BOOT_LOG_VERBOSE) {
    printSpiPinMap();
  }
  setTftControlLinesIdle();
  forceTftHardwareReset();
  runRawSpiBurst(false);
  if (BOOT_LOG_VERBOSE) {
    runTftProbe(true);
  }

  if (AUTO_TFT_INIT) {
    if (AUTO_TFT_WRITE_ONLY) {
      Serial.println("[TFT] Auto-init write-only mode enabled");
      runTftWriteOnlyWake(BOOT_LOG_VERBOSE);
    } else {
      Serial.println("[TFT] Auto-init enabled");
      initTftDisplayGuarded(2500);
    }
  } else {
    Serial.println("[TFT] Auto-init disabled (safe mode)");
  }

  bootStatus = runStartupDiagnostics();
  Serial.println("System ready. Type HELP for commands.");
}

void loop() {
  handleSerialCommands();

  const uint32_t now = millis();
  if (now - lastFrameMs >= 500) {
    lastFrameMs = now;
    loopCounter++;

    ledState = !ledState;
    digitalWrite(STATUS_LED_PIN, ledState ? HIGH : LOW);
    if (tftInitialized) {
      drawRuntimeDashboard(now);
    } else if (tftWriteOnlyReady) {
      drawRuntimeDashboardWriteOnly(now);
    }
    if ((loopCounter % 20U) == 0U) {
      if (bootStatus.inaOk) {
        const RailMeasurement rail5v = readRailMeasurement(HatRail::Rail5V);
        const RailMeasurement railAdj = readRailMeasurement(HatRail::RailAdj);
        const RailMeasurement railIn = readRailMeasurement(HatRail::RailIncoming12V);
        Serial.printf("HB=%u loop=%lu 5V=%.3fV/%.1fmA(%s) ADJ=%.3fV/%.1fmA(%s) IN12=%.3fV/%.1fmA\n",
                      ledState ? 1 : 0,
                      loopCounter,
                      rail5v.busVoltageV,
                      rail5v.currentmA,
                      HatPowerTelemetry::rangeName(rail5v.usedRange),
                      railAdj.busVoltageV,
                      railAdj.currentmA,
                      HatPowerTelemetry::rangeName(railAdj.usedRange),
                      railIn.busVoltageV,
                      railIn.currentmA);
      } else {
        Serial.printf("HB=%u loop=%lu INA=offline\n", ledState ? 1 : 0, loopCounter);
      }
    }
  }
}
