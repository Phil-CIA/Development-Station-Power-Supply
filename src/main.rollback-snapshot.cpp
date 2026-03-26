#include <Arduino.h>
#include <math.h>
#include <Wire.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <SDL_Arduino_INA3221.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

SDL_Arduino_INA3221 ina3221;
TFT_eSPI display = TFT_eSPI();

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

static int clamp999(int value) {
  if (value < 0) {
    return 0;
  }
  if (value > 999) {
    return 999;
  }
  return value;
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
    tftWriteOnlyDashboardInitialized = true;
  }

  for (int ch = 1; ch <= 3; ch++) {
    const float voltage = ina3221.getBusVoltage_V(ch);
    const float current = ina3221.getCurrent_mA(ch);
    const int deciVolts = clamp999(static_cast<int>(lroundf(voltage * 10.0f)));
    const int absCurrent = clamp999(static_cast<int>(lroundf(fabsf(current))));

    const int rowY = 24 + ((ch - 1) * 146);
    const uint16_t vColor = (ch == 1) ? 0x07FF : (ch == 2) ? 0xFFE0 : 0xF81F;
    const uint16_t iColor = 0x07E0;

    tftDrawDigit7SegRaw(8, rowY + 30, 1, ch, 0xFFFF, 0x0000);
    tftDrawNumberRaw(40, rowY, 2, deciVolts, 3, vColor, 0x0000);
    tftDrawNumberRaw(180, rowY, 2, absCurrent, 3, iColor, 0x0000);
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
  const uint8_t commandWrite = 0x00;
  const uint8_t address = (wiper & 0x01);

  digitalWrite(MCP4231_CS_PIN, LOW);
  SPI.transfer((commandWrite << 4) | address);
  SPI.transfer(value);
  digitalWrite(MCP4231_CS_PIN, HIGH);
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

  writeMcp4231Wiper(0, 128);
  writeMcp4231Wiper(1, 128);
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

  for (int ch = 1; ch <= 3; ch++) {
    const float voltage = ina3221.getBusVoltage_V(ch);
    const float current = ina3221.getCurrent_mA(ch);
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

  if (command == "HELP") {
    Serial.println("Commands: P0 <0-255>, P1 <0-255>, BOOTTEST, STATUS, TFTINIT, TFTTEST, TFTPROBE, TFTWAKE, TFTCLEAR, I2CSCAN, I2CRECOVER, SPIPINS, SPIRAW, PINTEST");
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
    Serial.printf("HB=%u loop=%lu CH2V=%.3f CH2I=%.1f\n", ledState ? 1 : 0, loopCounter, ina3221.getBusVoltage_V(2), ina3221.getCurrent_mA(2));
  }
}
