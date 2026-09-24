#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <string.h>
#include <Adafruit_AW9523.h>

// NOTE: STM32duino's concrete serial class is `Uart`, not `HardwareSerial`.
// `HardwareSerial` here is only the abstract ArduinoCore-API base class and
// has no (rx, tx) pin-pair constructor, which is what broke this build
// (see issue #3).
Uart SerialDbg(PA10, PA9); // RX, TX (USART1 via CH340 on HAT)
Uart SerialU3(PB11, PB10); // RX, TX (USART3)

// Project bring-up signals from docs/STM32_BLUEPILL_PIN_TABLE.md (Draft A)
static const uint8_t PIN_ISET_5V = PA0;
static const uint8_t PIN_ISET_3V3 = PA1;
static const uint8_t PIN_ISET_CH3 = PA2;
static const uint8_t PIN_FAULT_CRITICAL_SUM = PA3;
static const uint8_t PIN_STATUS_LED = PC13; // Blue Pill onboard LED (active-low on most boards)
static const uint8_t PIN_FLASH_CS = PA8;
static const uint8_t PIN_SR_LATCH = PA4;
// U4 shift-register bits controlling 3.3V rail path (PMOS: 0=ON, 1=OFF)
static const uint8_t SR_BIT_3V3_HI = 3;
static const uint8_t SR_BIT_3V3_LO = 4;
static const uint8_t SR_BIT_ADJ_LO = 5;

static const uint8_t I2C_SCL_PIN = PB8;
static const uint8_t I2C_SDA_PIN = PB9;
static const uint8_t AHT20_ADDRESS = 0x38;
static const uint8_t INA3221_ADDR_5V = 0x40;
static const uint8_t INA3221_ADDR_3V3 = 0x41;
static const uint8_t AW95XX_ADDR_MIN = 0x58;
static const uint8_t AW95XX_ADDR_MAX = 0x5B;
static const uint8_t AW95XX_ADDR_ACTIVE = 0x58;
static const uint8_t AW95XX_REG_OUTPUT_P0 = 0x02;
static const uint8_t AW95XX_REG_OUTPUT_P1 = 0x03;
static const uint8_t AW95XX_REG_INPUT_P0 = 0x00;
static const uint8_t AW95XX_REG_CONFIG_P0 = 0x04;
static const uint8_t AW95XX_REG_CONFIG_P1 = 0x05;
static const uint8_t AW95XX_REG_GCR = 0x11;      // Global control register
static const uint8_t AW95XX_REG_LED_MODE_P0 = 0x12; // 1=GPIO, 0=LED current mode (per bit)
static const uint8_t AW95XX_REG_LED_MODE_P1 = 0x13; // 1=GPIO, 0=LED current mode (per bit)
static const uint8_t AW95XX_GCR_PORT_MODE_BIT = 0x10; // 1=push-pull, 0=open-drain
static const uint8_t AW95XX_CFG_P0_POLICY = 0xC0; // P0.0..P0.5 output, P0.6..P0.7 input
static const uint8_t AW95XX_P10_MASK = 0x01;
static const uint8_t AW95XX_P01_MASK = 0x02; // P0.1: ESP- GPIO 5V Hi
static const uint8_t AW95XX_P02_MASK = 0x04; // P0.2: ESP- GPIO 5V Low (Q1/Q7 path)
static const uint8_t AW95XX_P03_MASK = 0x08; // P0.3: Channel 3 Hi-Range (Q5/Q11 path)
static const uint8_t AW95XX_P04_MASK = 0x10; // P0.4: ESP- GPIO 3V3 Low (Q4/Q10 path)
static const uint8_t AW95XX_P00_MASK = 0x01; // P0.0: ISET_MPU_5V -> Q3 gate path
static const uint8_t AW95XX_P05_MASK = 0x20; // P0.5: ISET_MPU_3V3 -> Q9 gate path
static const uint8_t AW95XX_PIN_P0_0 = 0;
static const uint8_t AW95XX_PIN_P0_1 = 1;
static const uint8_t AW95XX_PIN_P0_2 = 2;
static const uint8_t AW95XX_PIN_P0_3 = 3;
static const uint8_t AW95XX_PIN_P0_4 = 4;
static const uint8_t AW95XX_PIN_P0_5 = 5;
static const uint8_t AW95XX_PIN_P0_6 = 6;
static const uint8_t AW95XX_PIN_P0_7 = 7;
static const uint8_t AW95XX_PIN_P1_0 = 8;
static const uint8_t INA3221_REG_CONFIG = 0x00;
static const uint8_t INA3221_REG_SHUNTVOLTAGE_1 = 0x01;
static const uint8_t INA3221_REG_BUSVOLTAGE_1 = 0x02;
static const uint16_t INA3221_CONFIG_CONTINUOUS = 0x7127;
static const uint8_t AHT20_CMD_SOFT_RESET = 0xBA;
static const uint8_t AHT20_CMD_INIT = 0xBE;
static const uint8_t AHT20_CMD_MEASURE = 0xAC;
static const uint8_t AHT20_STATUS_BUSY = 0x80;
static const uint8_t AHT20_STATUS_CALIBRATED = 0x08;

// W25Q128 commands (U11 external SPI flash)
static const uint8_t CMD_RDID = 0x9F;
static const uint8_t CMD_RDSR1 = 0x05;
static const uint8_t CMD_WREN = 0x06;
static const uint8_t CMD_SECTOR_ERASE_4K = 0x20;
static const uint8_t CMD_PAGE_PROGRAM = 0x02;
static const uint8_t CMD_READ_DATA = 0x03;

static const uint32_t FLASH_TEST_ADDR = 0x001000; // dedicated bring-up test sector
static const size_t FLASH_TEST_LEN = 32;
static const uint32_t FLASH_CFG_ADDR = 0x002000; // dedicated persistent-config sector
static const uint32_t W25Q_PAGE_SIZE = 256;
static const uint32_t FLASH_CFG_MAGIC = 0x43464731; // "CFG1"
static const uint16_t FLASH_CFG_VERSION = 1;

// ── Telemetry frame constants ──────────────────────────────────────────────
static const uint8_t FRAME_SOF1 = 0xAA;
static const uint8_t FRAME_SOF2 = 0x55;
static const uint8_t FRAME_TAG = 'T';
static const uint8_t FRAME_LEN = 13;
static const size_t FRAME_SIZE = 17;
static const uint16_t CH1_ENABLED_MIN_MV = 1000;
static const uint16_t CH2_ENABLED_MIN_MV = 1000;
static const uint16_t CH1_OVP_THRESHOLD_MV = 5500;
static const uint16_t CH2_OVP_THRESHOLD_MV = 3600;
static const uint8_t OTP_THRESHOLD_C = 75;
static const uint8_t THERMAL_WARN_THRESHOLD_C = 70;
static const uint16_t UDI_CH1_LIMIT_MIN_MA = 0;
static const uint16_t UDI_CH1_LIMIT_MAX_MA = 3000;
static const uint16_t UDI_CH2_LIMIT_MIN_MA = 0;
static const uint16_t UDI_CH2_LIMIT_MAX_MA = 2000;
static const size_t UDI_MAX_LINE = 120;
static uint8_t frame_seq = 0;
static bool flash_test_passed = false;
static uint32_t flash_test_runs = 0;
static uint16_t g_sr_state = 0x0000;
static bool g_output_enabled = false;
static uint16_t g_current_limit_ch1_mA = 2500;
static uint16_t g_current_limit_ch2_mA = 1500;
static char g_udi_line_buf[UDI_MAX_LINE + 1] = {};
static size_t g_udi_line_len = 0;

struct RailCalibrationConfig {
  float voltage_gain;
  float voltage_offset_mV;
  float current_gain;
  float current_offset_mA;
};

struct PersistentConfigPayload {
  uint8_t d9_path_enabled;
  uint8_t reserved0;
  uint8_t reserved1;
  uint8_t reserved2;
  RailCalibrationConfig rail_5v;
  RailCalibrationConfig rail_3v3;
};

struct PersistentConfigRecord {
  uint32_t magic;
  uint16_t version;
  uint16_t payload_len;
  PersistentConfigPayload payload;
  uint32_t crc32;
};
static bool g_hb_print_enabled = false;

enum AwBootInitState : uint8_t {
  AW_BOOT_UNKNOWN = 0,
  AW_BOOT_SKIP = 1,
  AW_BOOT_PASS = 2,
  AW_BOOT_HOLD = 3
};

static AwBootInitState g_aw_boot_state = AW_BOOT_UNKNOWN;

static Adafruit_AW9523 g_aw;
static bool g_aw_inited = false;

struct Aht20Sample {
  bool valid;
  float temp_C;
  float humidity_pct;
  uint8_t status;
};

struct Ina3221ChannelReading {
  float bus_V;
  float shunt_mV;
  float current_mA;
};

struct Ina3221Reading {
  uint8_t address;
  bool present;
  Ina3221ChannelReading channel[3];
};

struct IncomingRailSample {
  bool valid;
  float bus_V;
  float current_mA;
};

Aht20Sample g_aht20 = {false, 0.0f, 0.0f, 0};
IncomingRailSample g_incoming_rail = {false, 0.0f, 0.0f};
PersistentConfigPayload g_config = {};

bool runFlashBringupTest();
bool readAht20Now(Aht20Sample& out);
bool readIna3221(uint8_t address, Ina3221Reading& out);
bool refreshIncomingRailSample();
void runShiftRegisterSelfTest();
void logHealthSummary();
void flashD9Led(uint8_t blinks, uint16_t on_ms, uint16_t off_ms);
void setD9PathEnabled(bool enabled);
void resetPersistentConfigDefaults();
bool savePersistentConfig(bool verbose);
bool loadPersistentConfig(bool verbose);
bool erasePersistentConfig(bool verbose);
void printPersistentConfig();
void pollUdiCommands();
void setRangePairEnabled(uint8_t bit, const char* label, bool enabled);
void setQ3OnlyEnabled(bool enabled);
void setQ9OnlyEnabled(bool enabled);
void setAwP0GatePathEnabled(uint8_t mask, uint8_t pin, const char* label, bool enabled);
void setQ1PathEnabled(bool enabled);
void setQ2PathEnabled(bool enabled);
void setQ4PathEnabled(bool enabled);
void setQ5PathEnabled(bool enabled);
void runQ9ElectricalDiagnostic();
void printRangePairStates();
void runRangePairSequence();
void captureRangeSequenceSnapshot(const char* step);
void runAwP10Heartbeat(uint8_t blinks, uint16_t on_ms, uint16_t off_ms);
bool aw95xxSetP0MaskOutputMode(uint8_t mask);
bool aw95xxWriteP0Mask(uint8_t mask, bool high, uint8_t& out_after);
bool aw95xxEnsureGpioPushPull();
void aw95xxPrintModeRegs();
bool aw95xxEnsureDriver();
void logAwP0MaskElectricalState(const char* tag, uint8_t mask, bool expected_high);
void aw95xxBootInit();
const char* awBootStateString();

void logBoth(const char* msg) {
  Serial.println(msg);
  SerialDbg.println(msg);
}

void formatAhtValues(const Aht20Sample& sample,
                     int32_t& t_whole,
                     int32_t& t_frac,
                     int32_t& h_whole,
                     int32_t& h_frac) {
  const int32_t t_centi = static_cast<int32_t>(sample.temp_C * 100.0f);
  const int32_t h_centi = static_cast<int32_t>(sample.humidity_pct * 100.0f);
  t_whole = t_centi / 100;
  t_frac = t_centi >= 0 ? (t_centi % 100) : -(t_centi % 100);
  h_whole = h_centi / 100;
  h_frac = h_centi >= 0 ? (h_centi % 100) : -(h_centi % 100);
}

void formatFixedValue(float value, uint32_t scale, uint8_t digits, char* out, size_t out_len) {
  const bool negative = value < 0.0f;
  const float abs_value = negative ? -value : value;
  const uint32_t scaled = static_cast<uint32_t>(abs_value * static_cast<float>(scale) + 0.5f);
  const uint32_t whole = scaled / scale;
  const uint32_t frac = scaled % scale;

  if (digits == 3) {
    snprintf(out, out_len, "%s%lu.%03lu", negative ? "-" : "", static_cast<unsigned long>(whole), static_cast<unsigned long>(frac));
  } else {
    snprintf(out, out_len, "%s%lu.%02lu", negative ? "-" : "", static_cast<unsigned long>(whole), static_cast<unsigned long>(frac));
  }
}

void formatVoltageValue(float value, char* out, size_t out_len) {
  formatFixedValue(value, 1000, 3, out, out_len);
}

void formatCurrentValue(float value, char* out, size_t out_len) {
  formatFixedValue(value, 100, 2, out, out_len);
}

float applyVoltageCalibration(float raw_bus_v, const RailCalibrationConfig& cal) {
  const float raw_mV = raw_bus_v * 1000.0f;
  const float corrected_mV = (raw_mV * cal.voltage_gain) + cal.voltage_offset_mV;
  return corrected_mV / 1000.0f;
}

float applyCurrentCalibration(float raw_mA, const RailCalibrationConfig& cal) {
  return (raw_mA * cal.current_gain) + cal.current_offset_mA;
}

const RailCalibrationConfig& calibrationForRail(uint8_t address) {
  if (address == INA3221_ADDR_5V) {
    return g_config.rail_5v;
  }
  return g_config.rail_3v3;
}

uint32_t crc32(const uint8_t* data, size_t len) {
  uint32_t crc = 0xFFFFFFFFu;
  for (size_t i = 0; i < len; ++i) {
    crc ^= static_cast<uint32_t>(data[i]);
    for (uint8_t bit = 0; bit < 8; ++bit) {
      const uint32_t mask = 0u - (crc & 1u);
      crc = (crc >> 1) ^ (0xEDB88320u & mask);
    }
  }
  return ~crc;
}

void printCommandHelp() {
  logBoth("cmd: HELP | FTEST | AHTNOW | AHTRESET | SRTEST | D9FLASH | D9ON | D9OFF | INAPROBE | INANOW | INARAILS | CALSHOW | CALSET <5V|3V3> <vGain> <vOff_mV> <iGain> <iOff_mA> | CFGSHOW | CFGSAVE | CFGLOAD | CFGRESET | CFGERASE | AWPROBE | AWHB | AWP10ON | AWP10OFF");
  logBoth("udi: CMD:OUTPUT <ON|OFF> | CMD:ILIM <CH1|CH2> <mA> | CMD:GET OUTPUT | CMD:GET ILIM <CH1|CH2>");
}

bool i2cPing(uint8_t address) {
  Wire.beginTransmission(address);
  return Wire.endTransmission() == 0;
}

bool i2cWriteReg16(uint8_t address, uint8_t reg, uint16_t value) {
  Wire.beginTransmission(address);
  Wire.write(reg);
  Wire.write(static_cast<uint8_t>((value >> 8) & 0xFF));
  Wire.write(static_cast<uint8_t>(value & 0xFF));
  return Wire.endTransmission() == 0;
}

bool i2cWriteReg8(uint8_t address, uint8_t reg, uint8_t value) {
  Wire.beginTransmission(address);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

bool i2cReadReg8(uint8_t address, uint8_t reg, uint8_t& value) {
  Wire.beginTransmission(address);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) {
    return false;
  }
  if (Wire.requestFrom(static_cast<int>(address), 1) != 1) {
    return false;
  }
  value = static_cast<uint8_t>(Wire.read());
  return true;
}

bool i2cReadReg16(uint8_t address, uint8_t reg, uint16_t& value) {
  Wire.beginTransmission(address);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) {
    return false;
  }
  if (Wire.requestFrom(static_cast<int>(address), 2) != 2) {
    return false;
  }
  value = (static_cast<uint16_t>(Wire.read()) << 8) | static_cast<uint16_t>(Wire.read());
  return true;
}

float inaBusVoltageV(uint16_t raw) {
  const int16_t signed_value = static_cast<int16_t>(raw) >> 3;
  return static_cast<float>(signed_value) * 0.008f;
}

float inaShuntMillivolts(uint16_t raw) {
  const int16_t signed_value = static_cast<int16_t>(raw) >> 3;
  return static_cast<float>(signed_value) * 0.04f;
}

float inaShuntOhmsFor(uint8_t address, int channel_index) {
  if (address == INA3221_ADDR_3V3 && channel_index == 2) {
    return 0.018f;
  }
  return 0.200f;
}

bool initIna3221(uint8_t address) {
  return i2cWriteReg16(address, INA3221_REG_CONFIG, INA3221_CONFIG_CONTINUOUS);
}

bool readIna3221(uint8_t address, Ina3221Reading& out) {
  out.address = address;
  out.present = false;

  if (!i2cPing(address)) {
    return false;
  }
  if (!initIna3221(address)) {
    return false;
  }

  for (int channel = 0; channel < 3; channel++) {
    const uint8_t shunt_reg = INA3221_REG_SHUNTVOLTAGE_1 + static_cast<uint8_t>(channel * 2);
    const uint8_t bus_reg = INA3221_REG_BUSVOLTAGE_1 + static_cast<uint8_t>(channel * 2);
    uint16_t raw_shunt = 0;
    uint16_t raw_bus = 0;
    if (!i2cReadReg16(address, shunt_reg, raw_shunt)) {
      return false;
    }
    if (!i2cReadReg16(address, bus_reg, raw_bus)) {
      return false;
    }

    out.channel[channel].bus_V = inaBusVoltageV(raw_bus);
    out.channel[channel].shunt_mV = inaShuntMillivolts(raw_shunt);
    out.channel[channel].current_mA = out.channel[channel].shunt_mV / inaShuntOhmsFor(address, channel);
  }

  out.present = true;
  return true;
}

bool refreshIncomingRailSample() {
  Ina3221Reading ina_3v3 = {};
  if (!readIna3221(INA3221_ADDR_3V3, ina_3v3)) {
    g_incoming_rail.valid = false;
    return false;
  }

  g_incoming_rail.valid = true;
  g_incoming_rail.bus_V = ina_3v3.channel[2].bus_V;
  g_incoming_rail.current_mA = ina_3v3.channel[2].current_mA;
  return true;
}

void printInaProbeSummary() {
  char msg[96];
  const bool found_5v = i2cPing(INA3221_ADDR_5V);
  const bool found_3v3 = i2cPing(INA3221_ADDR_3V3);
  snprintf(msg,
           sizeof(msg),
           "ina: expected 0x40=%s 0x41=%s (0x43 not used on this rev)",
           found_5v ? "ACK" : "MISS",
           found_3v3 ? "ACK" : "MISS");
  logBoth(msg);
}

void printAw95xxProbeSummary() {
  bool found_any = false;
  char msg[96];

  for (uint8_t addr = AW95XX_ADDR_MIN; addr <= AW95XX_ADDR_MAX; addr++) {
    if (i2cPing(addr)) {
      snprintf(msg,
               sizeof(msg),
               "aw95xx: candidate ACK at 0x%02X",
               addr);
      logBoth(msg);
      found_any = true;
    }
  }

  if (!found_any) {
    logBoth("aw95xx: no ACK in 0x58-0x5B");
  }
}

bool aw95xxSetP10OutputMode(uint8_t& saved_cfg, uint8_t& saved_out) {
  if (!aw95xxEnsureGpioPushPull()) {
    return false;
  }

  g_aw.pinMode(AW95XX_PIN_P1_0, OUTPUT);

  if (!i2cReadReg8(AW95XX_ADDR_ACTIVE, AW95XX_REG_CONFIG_P1, saved_cfg)) {
    return false;
  }
  if (!i2cReadReg8(AW95XX_ADDR_ACTIVE, AW95XX_REG_OUTPUT_P1, saved_out)) {
    return false;
  }

  const uint8_t new_cfg = static_cast<uint8_t>(saved_cfg & ~AW95XX_P10_MASK); // 0=output
  return i2cWriteReg8(AW95XX_ADDR_ACTIVE, AW95XX_REG_CONFIG_P1, new_cfg);
}

bool aw95xxSetP0MaskOutputMode(uint8_t mask) {
  if (!aw95xxEnsureGpioPushPull()) {
    return false;
  }

  if (mask & AW95XX_P00_MASK) g_aw.pinMode(AW95XX_PIN_P0_0, OUTPUT);
  if (mask & AW95XX_P01_MASK) g_aw.pinMode(AW95XX_PIN_P0_1, OUTPUT);
  if (mask & AW95XX_P02_MASK) g_aw.pinMode(AW95XX_PIN_P0_2, OUTPUT);
  if (mask & AW95XX_P03_MASK) g_aw.pinMode(AW95XX_PIN_P0_3, OUTPUT);
  if (mask & AW95XX_P04_MASK) g_aw.pinMode(AW95XX_PIN_P0_4, OUTPUT);
  if (mask & AW95XX_P05_MASK) g_aw.pinMode(AW95XX_PIN_P0_5, OUTPUT);

  uint8_t cfg_reg = 0;
  if (!i2cReadReg8(AW95XX_ADDR_ACTIVE, AW95XX_REG_CONFIG_P0, cfg_reg)) {
    return false;
  }

  const uint8_t new_cfg = static_cast<uint8_t>(cfg_reg & ~mask); // 0=output
  return i2cWriteReg8(AW95XX_ADDR_ACTIVE, AW95XX_REG_CONFIG_P0, new_cfg);
}

bool aw95xxWriteP0Mask(uint8_t mask, bool high, uint8_t& out_after) {
  if (!aw95xxEnsureGpioPushPull()) {
    return false;
  }

  const uint8_t level = high ? HIGH : LOW;
  if (mask & AW95XX_P00_MASK) g_aw.digitalWrite(AW95XX_PIN_P0_0, level);
  if (mask & AW95XX_P01_MASK) g_aw.digitalWrite(AW95XX_PIN_P0_1, level);
  if (mask & AW95XX_P02_MASK) g_aw.digitalWrite(AW95XX_PIN_P0_2, level);
  if (mask & AW95XX_P03_MASK) g_aw.digitalWrite(AW95XX_PIN_P0_3, level);
  if (mask & AW95XX_P04_MASK) g_aw.digitalWrite(AW95XX_PIN_P0_4, level);
  if (mask & AW95XX_P05_MASK) g_aw.digitalWrite(AW95XX_PIN_P0_5, level);

  uint8_t out_reg = 0;
  if (!i2cReadReg8(AW95XX_ADDR_ACTIVE, AW95XX_REG_OUTPUT_P0, out_reg)) {
    return false;
  }
  out_after = out_reg;
  return true;
}

void logAwP0MaskElectricalState(const char* tag, uint8_t mask, bool expected_high) {
  uint8_t p0_out = 0;
  uint8_t p0_in = 0;
  uint8_t p0_cfg = 0;

  if (!i2cReadReg8(AW95XX_ADDR_ACTIVE, AW95XX_REG_OUTPUT_P0, p0_out) ||
      !i2cReadReg8(AW95XX_ADDR_ACTIVE, AW95XX_REG_INPUT_P0, p0_in) ||
      !i2cReadReg8(AW95XX_ADDR_ACTIVE, AW95XX_REG_CONFIG_P0, p0_cfg)) {
    logBoth("aw95xx diag: failed reading P0 output/input/config registers");
    return;
  }

  const unsigned want = expected_high ? 1u : 0u;
  const unsigned out_bit = (p0_out & mask) ? 1u : 0u;
  const unsigned in_bit = (p0_in & mask) ? 1u : 0u;
  const unsigned is_out = (p0_cfg & mask) ? 0u : 1u;

  char msg[176];
  snprintf(msg,
           sizeof(msg),
           "aw95xx diag %s: want=%u out=%u in=%u dir=%s (p0_out=0x%02X p0_in=0x%02X cfg0=0x%02X)",
           tag,
           want,
           out_bit,
           in_bit,
           is_out ? "OUT" : "IN",
           static_cast<unsigned>(p0_out),
           static_cast<unsigned>(p0_in),
           static_cast<unsigned>(p0_cfg));
  logBoth(msg);

  if (is_out == 0u) {
    logBoth("aw95xx diag: pin not configured as output");
    return;
  }

  if ((out_bit == want) && (in_bit != want)) {
    logBoth("aw95xx diag: output register matches command, but input latch disagrees (possible external pull/load)");
  }
}

bool aw95xxEnsureDriver() {
  if (g_aw_inited) {
    return true;
  }

  if (!g_aw.begin(AW95XX_ADDR_ACTIVE, &Wire)) {
    return false;
  }

  g_aw_inited = true;
  return true;
}

bool aw95xxEnsureGpioPushPull() {
  if (!aw95xxEnsureDriver()) {
    return false;
  }

  // Adafruit driver API: false = push-pull for port0.
  g_aw.openDrainPort0(false);
  g_aw.pinMode(AW95XX_PIN_P0_0, OUTPUT);
  g_aw.pinMode(AW95XX_PIN_P0_1, OUTPUT);
  g_aw.pinMode(AW95XX_PIN_P0_2, OUTPUT);
  g_aw.pinMode(AW95XX_PIN_P0_3, OUTPUT);
  g_aw.pinMode(AW95XX_PIN_P0_4, OUTPUT);
  g_aw.pinMode(AW95XX_PIN_P0_5, OUTPUT);
  g_aw.pinMode(AW95XX_PIN_P0_6, INPUT);
  g_aw.pinMode(AW95XX_PIN_P0_7, INPUT);
  g_aw.pinMode(AW95XX_PIN_P1_0, OUTPUT);

  uint8_t gcr = 0;
  if (!i2cReadReg8(AW95XX_ADDR_ACTIVE, AW95XX_REG_GCR, gcr)) {
    return false;
  }

  // Datasheet + Adafruit driver behavior: bit set = push-pull, bit clear = open-drain.
  const uint8_t gcr_push_pull = static_cast<uint8_t>(gcr | AW95XX_GCR_PORT_MODE_BIT);
  if (!i2cWriteReg8(AW95XX_ADDR_ACTIVE, AW95XX_REG_GCR, gcr_push_pull)) {
    return false;
  }

  uint8_t gcr_after = 0;
  if (!i2cReadReg8(AW95XX_ADDR_ACTIVE, AW95XX_REG_GCR, gcr_after)) {
    return false;
  }
  if ((gcr_after & AW95XX_GCR_PORT_MODE_BIT) == 0) {
    return false;
  }

  // Force both ports into GPIO mode (set LED mode bits high).
  if (!i2cWriteReg8(AW95XX_ADDR_ACTIVE, AW95XX_REG_LED_MODE_P0, 0xFF)) {
    return false;
  }
  if (!i2cWriteReg8(AW95XX_ADDR_ACTIVE, AW95XX_REG_LED_MODE_P1, 0xFF)) {
    return false;
  }

  // Enforce board policy from bench findings:
  // P0.0..P0.5 are control outputs, P0.6..P0.7 are input lines.
  if (!i2cWriteReg8(AW95XX_ADDR_ACTIVE, AW95XX_REG_CONFIG_P0, AW95XX_CFG_P0_POLICY)) {
    return false;
  }

  return true;
}

void aw95xxPrintModeRegs() {
  if (!i2cPing(AW95XX_ADDR_ACTIVE)) {
    logBoth("aw95xx: 0x58 not responding");
    return;
  }

  uint8_t gcr = 0;
  uint8_t cfg_p0 = 0;
  uint8_t cfg_p1 = 0;
  uint8_t mode_p0 = 0;
  uint8_t mode_p1 = 0;
  if (!i2cReadReg8(AW95XX_ADDR_ACTIVE, AW95XX_REG_GCR, gcr) ||
      !i2cReadReg8(AW95XX_ADDR_ACTIVE, AW95XX_REG_CONFIG_P0, cfg_p0) ||
      !i2cReadReg8(AW95XX_ADDR_ACTIVE, AW95XX_REG_CONFIG_P1, cfg_p1) ||
      !i2cReadReg8(AW95XX_ADDR_ACTIVE, AW95XX_REG_LED_MODE_P0, mode_p0) ||
      !i2cReadReg8(AW95XX_ADDR_ACTIVE, AW95XX_REG_LED_MODE_P1, mode_p1)) {
    logBoth("aw95xx: failed reading mode registers");
    return;
  }

  char msg[192];
  snprintf(msg,
           sizeof(msg),
           "aw95xx mode: GCR=0x%02X (port=%s) CFG_P0=0x%02X CFG_P1=0x%02X LEDMODE_P0=0x%02X LEDMODE_P1=0x%02X (1=GPIO)",
           static_cast<unsigned>(gcr),
           (gcr & AW95XX_GCR_PORT_MODE_BIT) ? "push-pull" : "open-drain",
           static_cast<unsigned>(cfg_p0),
           static_cast<unsigned>(cfg_p1),
           static_cast<unsigned>(mode_p0),
           static_cast<unsigned>(mode_p1));
  logBoth(msg);

  if (cfg_p0 != AW95XX_CFG_P0_POLICY) {
    char warn[96];
    snprintf(warn,
             sizeof(warn),
             "aw95xx warn: CFG_P0 policy mismatch (expected 0x%02X)",
             static_cast<unsigned>(AW95XX_CFG_P0_POLICY));
    logBoth(warn);
  }
}

bool aw95xxWriteP10(bool high) {
  if (!aw95xxEnsureGpioPushPull()) {
    return false;
  }

  g_aw.pinMode(AW95XX_PIN_P1_0, OUTPUT);
  g_aw.digitalWrite(AW95XX_PIN_P1_0, high ? HIGH : LOW);

  uint8_t out_reg = 0;
  if (!i2cReadReg8(AW95XX_ADDR_ACTIVE, AW95XX_REG_OUTPUT_P1, out_reg)) {
    return false;
  }

  const bool bit_set = (out_reg & AW95XX_P10_MASK) != 0;
  return bit_set == high;
}

void runAwP10Heartbeat(uint8_t blinks, uint16_t on_ms, uint16_t off_ms) {
  if (!i2cPing(AW95XX_ADDR_ACTIVE)) {
    logBoth("aw95xx: 0x58 not responding");
    return;
  }

  uint8_t saved_cfg = 0;
  uint8_t saved_out = 0;
  if (!aw95xxSetP10OutputMode(saved_cfg, saved_out)) {
    logBoth("aw95xx: failed to set P1.0 output mode");
    return;
  }

  for (uint8_t i = 0; i < blinks; i++) {
    if (!aw95xxWriteP10(true)) {
      logBoth("aw95xx: write fail during heartbeat ON");
      break;
    }
    delay(on_ms);
    if (!aw95xxWriteP10(false)) {
      logBoth("aw95xx: write fail during heartbeat OFF");
      break;
    }
    delay(off_ms);
  }

  // Restore pre-test state.
  i2cWriteReg8(AW95XX_ADDR_ACTIVE, AW95XX_REG_OUTPUT_P1, saved_out);
  i2cWriteReg8(AW95XX_ADDR_ACTIVE, AW95XX_REG_CONFIG_P1, saved_cfg);
  logBoth("aw95xx: P1.0 heartbeat complete");
}

void printInaReading(const Ina3221Reading& reading, const char* label) {
  char msg[128];
  char ch1_v[16], ch1_i[16], ch2_v[16], ch2_i[16], ch3_v[16], ch3_i[16];
  formatVoltageValue(reading.channel[0].bus_V, ch1_v, sizeof(ch1_v));
  formatCurrentValue(reading.channel[0].current_mA, ch1_i, sizeof(ch1_i));
  formatVoltageValue(reading.channel[1].bus_V, ch2_v, sizeof(ch2_v));
  formatCurrentValue(reading.channel[1].current_mA, ch2_i, sizeof(ch2_i));
  formatVoltageValue(reading.channel[2].bus_V, ch3_v, sizeof(ch3_v));
  formatCurrentValue(reading.channel[2].current_mA, ch3_i, sizeof(ch3_i));
  snprintf(msg,
           sizeof(msg),
           "ina %s 0x%02X: CH1 %sV %smA | CH2 %sV %smA | CH3 %sV %smA",
           label,
           reading.address,
           ch1_v,
           ch1_i,
           ch2_v,
           ch2_i,
           ch3_v,
           ch3_i);
  logBoth(msg);
}

void printInaRailsSummary(const Ina3221Reading* ina_5v, const Ina3221Reading* ina_3v3) {
  if (ina_5v != nullptr) {
    char msg_5v[128];
    char hi_v[16], hi_i[16], lo_v[16], lo_i[16];
    formatVoltageValue(ina_5v->channel[0].bus_V, hi_v, sizeof(hi_v));
    formatCurrentValue(ina_5v->channel[0].current_mA, hi_i, sizeof(hi_i));
    formatVoltageValue(ina_5v->channel[1].bus_V, lo_v, sizeof(lo_v));
    formatCurrentValue(ina_5v->channel[1].current_mA, lo_i, sizeof(lo_i));
    snprintf(msg_5v,
             sizeof(msg_5v),
             "rail 5V: hi %sV %smA | lo %sV %smA",
             hi_v,
             hi_i,
             lo_v,
             lo_i);
    logBoth(msg_5v);
  } else {
    logBoth("rail 5V: INA 0x40 unavailable");
  }

  if (ina_3v3 != nullptr) {
    char msg_3v3[144];
    char hi_v[16], hi_i[16], lo_v[16], lo_i[16], in_v[16], in_i[16];
    formatVoltageValue(ina_3v3->channel[0].bus_V, hi_v, sizeof(hi_v));
    formatCurrentValue(ina_3v3->channel[0].current_mA, hi_i, sizeof(hi_i));
    formatVoltageValue(ina_3v3->channel[1].bus_V, lo_v, sizeof(lo_v));
    formatCurrentValue(ina_3v3->channel[1].current_mA, lo_i, sizeof(lo_i));
    formatVoltageValue(ina_3v3->channel[2].bus_V, in_v, sizeof(in_v));
    formatCurrentValue(ina_3v3->channel[2].current_mA, in_i, sizeof(in_i));
    snprintf(msg_3v3,
             sizeof(msg_3v3),
             "rail 3V3: hi %sV %smA | lo %sV %smA | in %sV %smA",
             hi_v,
             hi_i,
             lo_v,
             lo_i,
             in_v,
             in_i);
    logBoth(msg_3v3);
    logBoth("rail note: 0x41 CH3 is the incoming rail monitor; switched shunt currents remain hardware-limited on this rev.");
  } else {
    logBoth("rail 3V3: INA 0x41 unavailable");
  }
}

void sendUdiAck(const char* payload) {
  SerialU3.print("ACK:");
  SerialU3.println(payload);
}

void sendUdiErr(const char* payload) {
  SerialU3.print("ERR:");
  SerialU3.println(payload);
}

void sendUdiEvt(const char* payload) {
  SerialU3.print("EVT:");
  SerialU3.println(payload);
}

void handleUdiCommandLine(const String& line_in) {
  String line = line_in;
  line.trim();
  if (line.length() == 0) return;

  if (!line.startsWith("CMD:")) {
    sendUdiErr("FORMAT expected CMD:<command>");
    return;
  }

  String cmd = line.substring(4);
  cmd.trim();
  cmd.toUpperCase();
  if (cmd.length() == 0) {
    sendUdiErr("FORMAT empty command");
    return;
  }

  if (cmd == "OUTPUT ON") {
    g_output_enabled = true;
    setD9PathEnabled(true);
    g_config.d9_path_enabled = 1;
    savePersistentConfig(false);
    sendUdiAck("OUTPUT ON");
    sendUdiEvt("OUTPUT ON");
    return;
  }

  if (cmd == "OUTPUT OFF") {
    g_output_enabled = false;
    setD9PathEnabled(false);
    g_config.d9_path_enabled = 0;
    savePersistentConfig(false);
    sendUdiAck("OUTPUT OFF");
    sendUdiEvt("OUTPUT OFF");
    return;
  }

  if (cmd == "GET OUTPUT") {
    sendUdiAck(g_output_enabled ? "OUTPUT ON" : "OUTPUT OFF");
    return;
  }

  if (cmd == "GET ILIM CH1") {
    char ack_msg[40];
    snprintf(ack_msg,
             sizeof(ack_msg),
             "ILIM CH1 %u",
             static_cast<unsigned>(g_current_limit_ch1_mA));
    sendUdiAck(ack_msg);
    return;
  }

  if (cmd == "GET ILIM CH2") {
    char ack_msg[40];
    snprintf(ack_msg,
             sizeof(ack_msg),
             "ILIM CH2 %u",
             static_cast<unsigned>(g_current_limit_ch2_mA));
    sendUdiAck(ack_msg);
    return;
  }

  char channel_token[8] = {0};
  int limit_mA = -1;
  if (sscanf(cmd.c_str(), "ILIM %7s %d", channel_token, &limit_mA) == 2) {
    if (limit_mA < 0) {
      sendUdiErr("ILIM mA must be >= 0");
      return;
    }

    uint16_t* target_limit = nullptr;
    uint16_t min_mA = 0;
    uint16_t max_mA = 0;
    if (strcmp(channel_token, "CH1") == 0) {
      target_limit = &g_current_limit_ch1_mA;
      min_mA = UDI_CH1_LIMIT_MIN_MA;
      max_mA = UDI_CH1_LIMIT_MAX_MA;
    } else if (strcmp(channel_token, "CH2") == 0) {
      target_limit = &g_current_limit_ch2_mA;
      min_mA = UDI_CH2_LIMIT_MIN_MA;
      max_mA = UDI_CH2_LIMIT_MAX_MA;
    } else {
      sendUdiErr("ILIM channel must be CH1 or CH2");
      return;
    }

    if (limit_mA < static_cast<int>(min_mA) || limit_mA > static_cast<int>(max_mA)) {
      char msg[64];
      snprintf(msg,
               sizeof(msg),
               "ILIM %s range %u..%u mA",
               channel_token,
               static_cast<unsigned>(min_mA),
               static_cast<unsigned>(max_mA));
      sendUdiErr(msg);
      return;
    }

    *target_limit = static_cast<uint16_t>(limit_mA);

    char ack_msg[40];
    snprintf(ack_msg,
             sizeof(ack_msg),
             "ILIM %s %u",
             channel_token,
             static_cast<unsigned>(*target_limit));
    sendUdiAck(ack_msg);

    char evt_msg[48];
    snprintf(evt_msg,
             sizeof(evt_msg),
             "ILIM %s %u mA",
             channel_token,
             static_cast<unsigned>(*target_limit));
    sendUdiEvt(evt_msg);
    return;
  }

  sendUdiErr("UNKNOWN unsupported CMD");
}

void handleCommand(const String& cmd_in) {
  String cmd = cmd_in;
  cmd.trim();
  cmd.toUpperCase();
  if (cmd.length() == 0) return;

  if (cmd == "HELP") {
    printCommandHelp();
    return;
  }

  if (cmd == "FTEST") {
    flash_test_runs++;
    char run_msg[48];
    snprintf(run_msg, sizeof(run_msg), "flash: manual test run #%lu", static_cast<unsigned long>(flash_test_runs));
    logBoth(run_msg);
    flash_test_passed = runFlashBringupTest();
    return;
  }

  if (cmd == "AHTNOW") {
    Aht20Sample sample;
    if (readAht20Now(sample)) {
      g_aht20 = sample;
      char msg[96];
      int32_t t_whole = 0, t_frac = 0, h_whole = 0, h_frac = 0;
      formatAhtValues(g_aht20, t_whole, t_frac, h_whole, h_frac);
      snprintf(msg,
               sizeof(msg),
               "aht20: T=%ld.%02ldC RH=%ld.%02ld%% status=0x%02X",
               static_cast<long>(t_whole),
               static_cast<long>(t_frac),
               static_cast<long>(h_whole),
               static_cast<long>(h_frac),
               g_aht20.status);
      logBoth(msg);
    } else {
      logBoth("aht20: read failed");
    }
    return;
  }

  if (cmd == "AHTRESET") {
    Wire.beginTransmission(AHT20_ADDRESS);
    Wire.write(AHT20_CMD_SOFT_RESET);
    const uint8_t tx_ok = Wire.endTransmission();
    delay(25);
    if (tx_ok != 0) {
      logBoth("aht20: soft reset tx failed");
      return;
    }
    Aht20Sample sample;
    const bool ok = readAht20Now(sample);
    logBoth(ok ? "aht20: reset + probe OK" : "aht20: reset done, read still failing");
    return;
  }

  if (cmd == "SRTEST") {
    runShiftRegisterSelfTest();
    return;
  }

  if (cmd == "D9FLASH") {
    flashD9Led(8, 1000, 1000);
    return;
  }

  if (cmd == "D9ON") {
    g_output_enabled = true;
    setD9PathEnabled(true);
    g_config.d9_path_enabled = 1;
    savePersistentConfig(false);
    return;
  }

  if (cmd == "D9OFF") {
    g_output_enabled = false;
    setD9PathEnabled(false);
    g_config.d9_path_enabled = 0;
    savePersistentConfig(false);
    return;
  }

  if (cmd == "HBON") {
    g_hb_print_enabled = true;
    logBoth("hb: periodic serial output ENABLED");
    return;
  }

  if (cmd == "HBOFF") {
    g_hb_print_enabled = false;
    logBoth("hb: periodic serial output DISABLED");
    return;
  }

  if (cmd == "Q3ON") {
    setQ3OnlyEnabled(true);
    return;
  }

  if (cmd == "Q3OFF") {
    setQ3OnlyEnabled(false);
    return;
  }

  if (cmd == "Q1ON") {
    setQ1PathEnabled(true);
    return;
  }

  if (cmd == "Q1OFF") {
    setQ1PathEnabled(false);
    return;
  }

  if (cmd == "Q2ON") {
    setQ2PathEnabled(true);
    return;
  }

  if (cmd == "Q2OFF") {
    setQ2PathEnabled(false);
    return;
  }

  if (cmd == "Q4ON") {
    setQ4PathEnabled(true);
    return;
  }

  if (cmd == "Q4OFF") {
    setQ4PathEnabled(false);
    return;
  }

  if (cmd == "Q5ON") {
    setQ5PathEnabled(true);
    return;
  }

  if (cmd == "Q5OFF") {
    setQ5PathEnabled(false);
    return;
  }

  if (cmd == "Q9ON") {
    setQ9OnlyEnabled(true);
    return;
  }

  if (cmd == "Q9OFF") {
    setQ9OnlyEnabled(false);
    return;
  }

  if (cmd == "Q9DIAG") {
    runQ9ElectricalDiagnostic();
    return;
  }

  if (cmd == "Q39ON") {
    setRangePairEnabled(SR_BIT_3V3_HI, "Q3/Q9", true);
    return;
  }

  if (cmd == "Q39OFF") {
    setRangePairEnabled(SR_BIT_3V3_HI, "Q3/Q9", false);
    return;
  }

  if (cmd == "Q612ON") {
    setRangePairEnabled(SR_BIT_ADJ_LO, "Q6/Q12", true);
    return;
  }

  if (cmd == "Q612OFF") {
    setRangePairEnabled(SR_BIT_ADJ_LO, "Q6/Q12", false);
    return;
  }

  if (cmd == "QSTATE") {
    printRangePairStates();
    return;
  }

  if (cmd == "QSEQ") {
    runRangePairSequence();
    return;
  }

  if (cmd == "INAPROBE") {
    printInaProbeSummary();
    return;
  }

  if (cmd == "INANOW") {
    Ina3221Reading ina_5v = {};
    Ina3221Reading ina_3v3 = {};
    const bool ok_5v = readIna3221(INA3221_ADDR_5V, ina_5v);
    const bool ok_3v3 = readIna3221(INA3221_ADDR_3V3, ina_3v3);
    if (!ok_5v && !ok_3v3) {
      logBoth("ina: no expected devices responded");
      return;
    }
    if (ok_5v) {
      printInaReading(ina_5v, "5V");
    } else {
      logBoth("ina 5V 0x40: read failed");
    }
    if (ok_3v3) {
      printInaReading(ina_3v3, "3V3");
    } else {
      logBoth("ina 3V3 0x41: read failed");
    }
    return;
  }

  if (cmd == "INARAILS") {
    Ina3221Reading ina_5v = {};
    Ina3221Reading ina_3v3 = {};
    const bool ok_5v = readIna3221(INA3221_ADDR_5V, ina_5v);
    const bool ok_3v3 = readIna3221(INA3221_ADDR_3V3, ina_3v3);
    if (!ok_5v && !ok_3v3) {
      logBoth("ina rails: no expected devices responded");
      return;
    }
    printInaRailsSummary(ok_5v ? &ina_5v : nullptr, ok_3v3 ? &ina_3v3 : nullptr);
    return;
  }

  if (cmd == "CALSHOW" || cmd == "CFGSHOW") {
    printPersistentConfig();
    return;
  }

  if (cmd.startsWith("CALSET ")) {
    char rail_token[8] = {0};
    float v_gain = 1.0f;
    float v_off_mV = 0.0f;
    float i_gain = 1.0f;
    float i_off_mA = 0.0f;
    if (sscanf(cmd.c_str(), "CALSET %7s %f %f %f %f", rail_token, &v_gain, &v_off_mV, &i_gain, &i_off_mA) != 5) {
      logBoth("[CFG] Usage: CALSET <5V|3V3> <vGain> <vOff_mV> <iGain> <iOff_mA>");
      return;
    }

    if (v_gain <= 0.0f || i_gain <= 0.0f) {
      logBoth("[CFG] CALSET rejected: gains must be > 0");
      return;
    }
    if (v_off_mV < -10000.0f || v_off_mV > 10000.0f || i_off_mA < -50000.0f || i_off_mA > 50000.0f) {
      logBoth("[CFG] CALSET rejected: offsets out of safe range");
      return;
    }

    RailCalibrationConfig* target = nullptr;
    if (strcmp(rail_token, "5V") == 0) {
      target = &g_config.rail_5v;
    } else if (strcmp(rail_token, "3V3") == 0) {
      target = &g_config.rail_3v3;
    } else {
      logBoth("[CFG] CALSET rejected: rail must be 5V or 3V3");
      return;
    }

    target->voltage_gain = v_gain;
    target->voltage_offset_mV = v_off_mV;
    target->current_gain = i_gain;
    target->current_offset_mA = i_off_mA;
    savePersistentConfig(false);
    printPersistentConfig();
    return;
  }

  if (cmd == "CFGSAVE") {
    savePersistentConfig(true);
    return;
  }

  if (cmd == "CFGLOAD") {
    if (loadPersistentConfig(true)) {
      setD9PathEnabled(g_config.d9_path_enabled != 0);
    }
    return;
  }

  if (cmd == "CFGRESET") {
    resetPersistentConfigDefaults();
    setD9PathEnabled(g_config.d9_path_enabled != 0);
    savePersistentConfig(true);
    return;
  }

  if (cmd == "CFGERASE") {
    if (erasePersistentConfig(true)) {
      resetPersistentConfigDefaults();
      setD9PathEnabled(g_config.d9_path_enabled != 0);
    }
    return;
  }

  if (cmd == "AWPROBE") {
    printAw95xxProbeSummary();
    return;
  }

  if (cmd == "AWMODE") {
    if (aw95xxEnsureGpioPushPull()) {
      logBoth("aw95xx: forced GPIO + push-pull mode");
    } else {
      logBoth("aw95xx: failed forcing GPIO + push-pull mode");
    }
    aw95xxPrintModeRegs();
    return;
  }

  if (cmd == "AWHB") {
    runAwP10Heartbeat(8, 200, 200);
    return;
  }

  if (cmd == "AWP10ON") {
    uint8_t saved_cfg = 0;
    uint8_t saved_out = 0;
    (void)saved_out;
    if (!aw95xxSetP10OutputMode(saved_cfg, saved_out)) {
      logBoth("aw95xx: failed to set P1.0 output mode");
      return;
    }
    if (aw95xxWriteP10(true)) {
      logBoth("aw95xx: P1.0 forced HIGH");
    } else {
      logBoth("aw95xx: failed to write P1.0 HIGH");
    }
    return;
  }

  if (cmd == "AWP10OFF") {
    uint8_t saved_cfg = 0;
    uint8_t saved_out = 0;
    (void)saved_out;
    if (!aw95xxSetP10OutputMode(saved_cfg, saved_out)) {
      logBoth("aw95xx: failed to set P1.0 output mode");
      return;
    }
    if (aw95xxWriteP10(false)) {
      logBoth("aw95xx: P1.0 forced LOW");
    } else {
      logBoth("aw95xx: failed to write P1.0 LOW");
    }
    return;
  }

  char unknown_msg[80];
  snprintf(unknown_msg, sizeof(unknown_msg), "cmd: unknown '%s'", cmd.c_str());
  logBoth(unknown_msg);
  printCommandHelp();
}

void pollCommands() {
  if (Serial.available()) {
    const String cmd = Serial.readStringUntil('\n');
    handleCommand(cmd);
  }
  if (SerialDbg.available()) {
    const String cmd = SerialDbg.readStringUntil('\n');
    handleCommand(cmd);
  }
  pollUdiCommands();
}

void pollUdiCommands() {
  while (SerialU3.available() > 0) {
    const char ch = static_cast<char>(SerialU3.read());
    if (ch == '\r') {
      continue;
    }
    if (ch == '\n') {
      if (g_udi_line_len > 0) {
        g_udi_line_buf[g_udi_line_len] = '\0';
        handleUdiCommandLine(String(g_udi_line_buf));
      }
      g_udi_line_len = 0;
      continue;
    }

    if (ch < 0x20 || ch > 0x7E) {
      g_udi_line_len = 0;
      continue;
    }

    if (g_udi_line_len < UDI_MAX_LINE) {
      g_udi_line_buf[g_udi_line_len++] = ch;
    } else {
      g_udi_line_len = 0;
      sendUdiErr("FORMAT line too long");
    }
  }
}

bool readAhtStatus(uint8_t& status) {
  if (Wire.requestFrom(static_cast<int>(AHT20_ADDRESS), 1) != 1) {
    return false;
  }
  status = static_cast<uint8_t>(Wire.read());
  return true;
}

bool initAhtIfNeeded() {
  uint8_t status = 0;
  if (!readAhtStatus(status)) return false;
  if ((status & AHT20_STATUS_CALIBRATED) != 0) return true;

  Wire.beginTransmission(AHT20_ADDRESS);
  Wire.write(AHT20_CMD_INIT);
  Wire.write(0x08);
  Wire.write(0x00);
  if (Wire.endTransmission() != 0) return false;
  delay(12);
  if (!readAhtStatus(status)) return false;
  return (status & AHT20_STATUS_CALIBRATED) != 0;
}

bool readAht20Now(Aht20Sample& out) {
  out.valid = false;

  if (!initAhtIfNeeded()) {
    return false;
  }

  Wire.beginTransmission(AHT20_ADDRESS);
  Wire.write(AHT20_CMD_MEASURE);
  Wire.write(0x33);
  Wire.write(0x00);
  if (Wire.endTransmission() != 0) return false;

  delay(80);

  uint8_t status = 0;
  const uint32_t deadline = millis() + 120;
  while (millis() < deadline) {
    if (!readAhtStatus(status)) return false;
    if ((status & AHT20_STATUS_BUSY) == 0) break;
    delay(10);
  }
  if ((status & AHT20_STATUS_BUSY) != 0) return false;

  uint8_t frame[7] = {0};
  if (Wire.requestFrom(static_cast<int>(AHT20_ADDRESS), 7) != 7) {
    return false;
  }
  for (size_t i = 0; i < 7; i++) {
    frame[i] = static_cast<uint8_t>(Wire.read());
  }

  const uint32_t raw_h =
      (static_cast<uint32_t>(frame[1]) << 12) |
      (static_cast<uint32_t>(frame[2]) << 4) |
      ((static_cast<uint32_t>(frame[3]) & 0xF0) >> 4);
  const uint32_t raw_t =
      ((static_cast<uint32_t>(frame[3]) & 0x0F) << 16) |
      (static_cast<uint32_t>(frame[4]) << 8) |
      static_cast<uint32_t>(frame[5]);

  out.humidity_pct = (static_cast<float>(raw_h) * 100.0f) / 1048576.0f;
  out.temp_C = (static_cast<float>(raw_t) * 200.0f) / 1048576.0f - 50.0f;
  out.status = frame[0];
  out.valid = true;
  return true;
}

void srShiftOut16(uint16_t value) {
  g_sr_state = value;
  digitalWrite(PIN_SR_LATCH, LOW);
  SPI.transfer(static_cast<uint8_t>((value >> 8) & 0xFF));
  SPI.transfer(static_cast<uint8_t>(value & 0xFF));
  digitalWrite(PIN_SR_LATCH, HIGH);
  delayMicroseconds(2);
  digitalWrite(PIN_SR_LATCH, LOW);
}

void flashD9Led(uint8_t blinks, uint16_t on_ms, uint16_t off_ms) {
  if (i2cPing(AW95XX_ADDR_ACTIVE)) {
    logBoth("d9: using aw95xx P1.0 backend");
    runAwP10Heartbeat(blinks, on_ms, off_ms);
    return;
  }

  logBoth("d9: using SR fallback backend");
  const uint16_t mask = static_cast<uint16_t>((1u << SR_BIT_3V3_HI) | (1u << SR_BIT_3V3_LO));
  const uint16_t saved = g_sr_state;
  // PMOS high-side behavior on this path: gate-low enables, gate-high disables.
  const uint16_t ch2_on = static_cast<uint16_t>(saved & ~mask);
  const uint16_t ch2_off = static_cast<uint16_t>(saved | mask);

  char msg[96];
  snprintf(msg,
           sizeof(msg),
           "d9: flashing %u cycles (on=%u ms off=%u ms)",
           static_cast<unsigned>(blinks),
           static_cast<unsigned>(on_ms),
           static_cast<unsigned>(off_ms));
  logBoth(msg);

  for (uint8_t i = 0; i < blinks; ++i) {
    srShiftOut16(ch2_on);
    delay(on_ms);
    srShiftOut16(ch2_off);
    delay(off_ms);
  }

  srShiftOut16(saved);
  logBoth("d9: flash sequence complete");
}

void setD9PathEnabled(bool enabled) {
  if (i2cPing(AW95XX_ADDR_ACTIVE)) {
    uint8_t saved_cfg = 0;
    uint8_t saved_out = 0;
    (void)saved_out;
    if (!aw95xxSetP10OutputMode(saved_cfg, saved_out)) {
      logBoth("aw95xx: failed to set P1.0 output mode");
      return;
    }
    if (aw95xxWriteP10(enabled)) {
      logBoth(enabled ? "d9: aw95xx P1.0 forced ON" : "d9: aw95xx P1.0 forced OFF");
    } else {
      logBoth(enabled ? "d9: aw95xx P1.0 ON write failed" : "d9: aw95xx P1.0 OFF write failed");
    }
    return;
  }

  logBoth("d9: aw95xx unavailable, using SR fallback backend");
  const uint16_t mask = static_cast<uint16_t>((1u << SR_BIT_3V3_HI) | (1u << SR_BIT_3V3_LO));
  // PMOS high-side behavior on this path: gate-low enables, gate-high disables.
  const uint16_t on_state = static_cast<uint16_t>(g_sr_state & ~mask);
  const uint16_t off_state = static_cast<uint16_t>(g_sr_state | mask);

  if (enabled) {
    // Force a visible transition on the gate even if the path was already on.
    srShiftOut16(off_state);
    delay(20);
    srShiftOut16(on_state);
  } else {
    srShiftOut16(off_state);
  }

  logBoth(enabled ? "d9: path forced ON" : "d9: path forced OFF");
}

void setRangePairEnabled(uint8_t bit, const char* label, bool enabled) {
  uint8_t p0_mask = 0;
  const char* p0_desc = "";
  if (bit == SR_BIT_3V3_HI) {
    // From Rev-C netlist provided by operator:
    // Q3 gate path is tied to ISET_MPU_5V (U5 P0.0),
    // Q9 gate path is tied to ISET_MPU_3V3 (U5 P0.5).
    p0_mask = static_cast<uint8_t>(AW95XX_P00_MASK | AW95XX_P05_MASK);
    p0_desc = "P0.0+P0.5";
  } else if (bit == SR_BIT_ADJ_LO) {
    p0_mask = AW95XX_P01_MASK; // P0.1 = ESP- GPIO 5V Hi (Q6/Q12 path)
    p0_desc = "P0.1";
  }

  if (p0_mask != 0 && i2cPing(AW95XX_ADDR_ACTIVE)) {
    if (!aw95xxSetP0MaskOutputMode(p0_mask)) {
      logBoth("aw95xx: failed to set P0 pin output mode for range control");
      return;
    }

    // EN nets are active-high on this Rev-C path.
    uint8_t p0_out_after = 0;
    if (!aw95xxWriteP0Mask(p0_mask, enabled, p0_out_after)) {
      logBoth("aw95xx: failed to write P0 pin for range control");
      return;
    }

    char msg[128];
    snprintf(msg,
             sizeof(msg),
             "range: %s forced %s via aw95xx %s (p0=0x%02X)",
             label,
             enabled ? "ON" : "OFF",
             p0_desc,
             static_cast<unsigned>(p0_out_after));
    logBoth(msg);
    return;
  }

  // Fallback for legacy hardware paths that still use the shift-register chain.
  const uint16_t mask = static_cast<uint16_t>(1u << bit);
  const uint16_t on_state = static_cast<uint16_t>(g_sr_state & ~mask);
  const uint16_t off_state = static_cast<uint16_t>(g_sr_state | mask);
  srShiftOut16(enabled ? on_state : off_state);

  char msg[112];
  snprintf(msg,
           sizeof(msg),
           "range: %s forced %s via SR fallback (bit%u=%u, sr=0x%04X)",
           label,
           enabled ? "ON" : "OFF",
           static_cast<unsigned>(bit),
           static_cast<unsigned>((g_sr_state >> bit) & 0x1u),
           static_cast<unsigned>(g_sr_state));
  logBoth(msg);
}

void setQ3OnlyEnabled(bool enabled) {
  if (i2cPing(AW95XX_ADDR_ACTIVE)) {
    if (!aw95xxSetP0MaskOutputMode(AW95XX_P00_MASK)) {
      logBoth("aw95xx: failed to set P0.0 output mode for Q3-only control");
      return;
    }

    uint8_t p0_out_after = 0;
    if (!aw95xxWriteP0Mask(AW95XX_P00_MASK, enabled, p0_out_after)) {
      logBoth("aw95xx: failed to write P0.0 for Q3-only control");
      return;
    }

    char msg[120];
    snprintf(msg,
             sizeof(msg),
             "range: Q3-only forced %s via aw95xx P0.0 (p0=0x%02X)",
             enabled ? "ON" : "OFF",
             static_cast<unsigned>(p0_out_after));
    logBoth(msg);
    logAwP0MaskElectricalState("Q3/P0.0", AW95XX_P00_MASK, enabled);
    return;
  }

  logBoth("range: Q3-only requested but aw95xx unavailable; using combined Q3/Q9 fallback");
  setRangePairEnabled(SR_BIT_3V3_HI, "Q3/Q9", enabled);
}

void setQ9OnlyEnabled(bool enabled) {
  if (i2cPing(AW95XX_ADDR_ACTIVE)) {
    if (!aw95xxSetP0MaskOutputMode(AW95XX_P05_MASK)) {
      logBoth("aw95xx: failed to set P0.5 output mode for Q9-only control");
      return;
    }

    uint8_t p0_out_after = 0;
    if (!aw95xxWriteP0Mask(AW95XX_P05_MASK, enabled, p0_out_after)) {
      logBoth("aw95xx: failed to write P0.5 for Q9-only control");
      return;
    }

    char msg[120];
    snprintf(msg,
             sizeof(msg),
             "range: Q9-only forced %s via aw95xx P0.5 (p0=0x%02X)",
             enabled ? "ON" : "OFF",
             static_cast<unsigned>(p0_out_after));
    logBoth(msg);
    logAwP0MaskElectricalState("Q9/P0.5", AW95XX_P05_MASK, enabled);
    return;
  }

  logBoth("range: Q9-only requested but aw95xx unavailable; using combined Q3/Q9 fallback");
  setRangePairEnabled(SR_BIT_3V3_HI, "Q3/Q9", enabled);
}

void setAwP0GatePathEnabled(uint8_t mask, uint8_t pin, const char* label, bool enabled) {
  if (!i2cPing(AW95XX_ADDR_ACTIVE)) {
    logBoth("range: aw95xx unavailable for direct gate-path control");
    return;
  }

  if (!aw95xxSetP0MaskOutputMode(mask)) {
    logBoth("aw95xx: failed to set P0 output mode for direct gate-path control");
    return;
  }

  uint8_t p0_out_after = 0;
  if (!aw95xxWriteP0Mask(mask, enabled, p0_out_after)) {
    logBoth("aw95xx: failed to write P0 output for direct gate-path control");
    return;
  }

  char msg[136];
  snprintf(msg,
           sizeof(msg),
           "range: %s forced %s via aw95xx P0.%u (p0=0x%02X)",
           label,
           enabled ? "ON" : "OFF",
           static_cast<unsigned>(pin),
           static_cast<unsigned>(p0_out_after));
  logBoth(msg);
  logAwP0MaskElectricalState(label, mask, enabled);
}

void setQ1PathEnabled(bool enabled) {
  setAwP0GatePathEnabled(AW95XX_P02_MASK, AW95XX_PIN_P0_2, "Q1/Q7", enabled);
}

void setQ2PathEnabled(bool enabled) {
  setAwP0GatePathEnabled(AW95XX_P01_MASK, AW95XX_PIN_P0_1, "Q2/Q8", enabled);
}

void setQ4PathEnabled(bool enabled) {
  setAwP0GatePathEnabled(AW95XX_P04_MASK, AW95XX_PIN_P0_4, "Q4/Q10", enabled);
}

void setQ5PathEnabled(bool enabled) {
  setAwP0GatePathEnabled(AW95XX_P03_MASK, AW95XX_PIN_P0_3, "Q5/Q11", enabled);
}

void runQ9ElectricalDiagnostic() {
  if (!i2cPing(AW95XX_ADDR_ACTIVE)) {
    logBoth("q9 diag: aw95xx unavailable");
    return;
  }

  if (!aw95xxEnsureGpioPushPull()) {
    logBoth("q9 diag: failed to enforce push-pull mode");
    return;
  }

  logBoth("q9 diag: start (P0.5 low -> high -> input)");

  g_aw.pinMode(AW95XX_PIN_P0_5, OUTPUT);
  g_aw.digitalWrite(AW95XX_PIN_P0_5, LOW);
  delay(5);
  logAwP0MaskElectricalState("Q9/P0.5 LOW", AW95XX_P05_MASK, false);

  g_aw.pinMode(AW95XX_PIN_P0_5, OUTPUT);
  g_aw.digitalWrite(AW95XX_PIN_P0_5, HIGH);
  delay(5);
  logAwP0MaskElectricalState("Q9/P0.5 HIGH", AW95XX_P05_MASK, true);

  g_aw.pinMode(AW95XX_PIN_P0_5, INPUT);
  delay(5);

  uint8_t p0_in = 0;
  uint8_t p0_cfg = 0;
  if (!i2cReadReg8(AW95XX_ADDR_ACTIVE, AW95XX_REG_INPUT_P0, p0_in) ||
      !i2cReadReg8(AW95XX_ADDR_ACTIVE, AW95XX_REG_CONFIG_P0, p0_cfg)) {
    logBoth("q9 diag: failed reading input/config after Hi-Z");
  } else {
    const unsigned in_bit = (p0_in & AW95XX_P05_MASK) ? 1u : 0u;
    const unsigned is_input = (p0_cfg & AW95XX_P05_MASK) ? 1u : 0u;
    char msg[144];
    snprintf(msg,
             sizeof(msg),
             "q9 diag: Hi-Z sample in=%u dir=%s (p0_in=0x%02X cfg0=0x%02X)",
             in_bit,
             is_input ? "IN" : "OUT",
             static_cast<unsigned>(p0_in),
             static_cast<unsigned>(p0_cfg));
    logBoth(msg);
  }

  // Restore default board policy after test.
  if (aw95xxEnsureGpioPushPull()) {
    logBoth("q9 diag: end (policy restored)");
  } else {
    logBoth("q9 diag: end (failed restoring policy)");
  }
}

void aw95xxBootInit() {
  if (!i2cPing(AW95XX_ADDR_ACTIVE)) {
    g_aw_boot_state = AW_BOOT_SKIP;
    logBoth("aw95xx boot: 0x58 not detected, skipping init");
    return;
  }

  if (!aw95xxEnsureGpioPushPull()) {
    g_aw_boot_state = AW_BOOT_HOLD;
    logBoth("aw95xx boot: failed to enforce GPIO + push-pull policy");
    return;
  }

  // Default control outputs low at boot for deterministic inactive state.
  uint8_t p0_out_after = 0;
  const uint8_t boot_mask = static_cast<uint8_t>(AW95XX_P00_MASK | AW95XX_P01_MASK | AW95XX_P02_MASK |
                                                 AW95XX_P03_MASK | AW95XX_P04_MASK | AW95XX_P05_MASK);
  if (!aw95xxWriteP0Mask(boot_mask, false, p0_out_after)) {
    g_aw_boot_state = AW_BOOT_HOLD;
    logBoth("aw95xx boot: failed forcing P0.0/P0.1/P0.5 low");
  } else {
    g_aw_boot_state = AW_BOOT_PASS;
    char msg[96];
    snprintf(msg,
             sizeof(msg),
             "aw95xx boot: policy latched, P0 outputs forced low (p0=0x%02X)",
             static_cast<unsigned>(p0_out_after));
    logBoth(msg);
  }

  // Keep P1.0 low by default as well.
  (void)aw95xxWriteP10(false);
  aw95xxPrintModeRegs();
}

const char* awBootStateString() {
  switch (g_aw_boot_state) {
    case AW_BOOT_SKIP:
      return "SKIP";
    case AW_BOOT_PASS:
      return "PASS";
    case AW_BOOT_HOLD:
      return "HOLD";
    case AW_BOOT_UNKNOWN:
    default:
      return "UNK";
  }
}

void printRangePairStates() {
  uint8_t p0_cfg = 0;
  uint8_t p0_out = 0;
  if (i2cPing(AW95XX_ADDR_ACTIVE) &&
      i2cReadReg8(AW95XX_ADDR_ACTIVE, AW95XX_REG_CONFIG_P0, p0_cfg) &&
      i2cReadReg8(AW95XX_ADDR_ACTIVE, AW95XX_REG_OUTPUT_P0, p0_out)) {
    const unsigned q39_q3 = static_cast<unsigned>((p0_out & AW95XX_P00_MASK) != 0);
    const unsigned q2_q8 = static_cast<unsigned>((p0_out & AW95XX_P01_MASK) != 0);
    const unsigned q1_q7 = static_cast<unsigned>((p0_out & AW95XX_P02_MASK) != 0);
    const unsigned q5_q11 = static_cast<unsigned>((p0_out & AW95XX_P03_MASK) != 0);
    const unsigned q4_q10 = static_cast<unsigned>((p0_out & AW95XX_P04_MASK) != 0);
    const unsigned q39_q9 = static_cast<unsigned>((p0_out & AW95XX_P05_MASK) != 0);
    const unsigned q612 = static_cast<unsigned>((p0_out & AW95XX_P01_MASK) != 0);
    const unsigned q39_q3_is_out = static_cast<unsigned>((p0_cfg & AW95XX_P00_MASK) == 0);
    const unsigned q2_q8_is_out = static_cast<unsigned>((p0_cfg & AW95XX_P01_MASK) == 0);
    const unsigned q1_q7_is_out = static_cast<unsigned>((p0_cfg & AW95XX_P02_MASK) == 0);
    const unsigned q5_q11_is_out = static_cast<unsigned>((p0_cfg & AW95XX_P03_MASK) == 0);
    const unsigned q4_q10_is_out = static_cast<unsigned>((p0_cfg & AW95XX_P04_MASK) == 0);
    const unsigned q39_q9_is_out = static_cast<unsigned>((p0_cfg & AW95XX_P05_MASK) == 0);
    const unsigned q612_is_out = static_cast<unsigned>((p0_cfg & AW95XX_P01_MASK) == 0);

    char msg[320];
    snprintf(msg,
             sizeof(msg),
       "range: aw95xx p0=0x%02X cfg0=0x%02X | Q1/Q7 P0.2=%u(%s,%s) | Q2/Q8 P0.1=%u(%s,%s) | Q3 P0.0=%u(%s,%s) | Q4/Q10 P0.4=%u(%s,%s) | Q5/Q11 P0.3=%u(%s,%s) | Q9 P0.5=%u(%s,%s) | Q6/Q12 P0.1=%u(%s,%s)",
             static_cast<unsigned>(p0_out),
             static_cast<unsigned>(p0_cfg),
       q1_q7,
       q1_q7 ? "ON" : "OFF",
       q1_q7_is_out ? "OUT" : "IN",
       q2_q8,
       q2_q8 ? "ON" : "OFF",
       q2_q8_is_out ? "OUT" : "IN",
         q39_q3,
         q39_q3 ? "ON" : "OFF",
         q39_q3_is_out ? "OUT" : "IN",
       q4_q10,
       q4_q10 ? "ON" : "OFF",
       q4_q10_is_out ? "OUT" : "IN",
       q5_q11,
       q5_q11 ? "ON" : "OFF",
       q5_q11_is_out ? "OUT" : "IN",
         q39_q9,
         q39_q9 ? "ON" : "OFF",
         q39_q9_is_out ? "OUT" : "IN",
             q612,
             q612 ? "ON" : "OFF",
             q612_is_out ? "OUT" : "IN");
    logBoth(msg);
    return;
  }

  char msg[120];
  const unsigned q39_bit = static_cast<unsigned>((g_sr_state >> SR_BIT_3V3_HI) & 0x1u);
  const unsigned q612_bit = static_cast<unsigned>((g_sr_state >> SR_BIT_ADJ_LO) & 0x1u);
  snprintf(msg,
           sizeof(msg),
           "range: aw95xx unavailable, SR fallback sr=0x%04X | Q3/Q9 bit%u=%u(%s) | Q6/Q12 bit%u=%u(%s)",
           static_cast<unsigned>(g_sr_state),
           static_cast<unsigned>(SR_BIT_3V3_HI),
           q39_bit,
           q39_bit == 0 ? "ON" : "OFF",
           static_cast<unsigned>(SR_BIT_ADJ_LO),
           q612_bit,
           q612_bit == 0 ? "ON" : "OFF");
  logBoth(msg);
}

void captureRangeSequenceSnapshot(const char* step) {
  char tag[64];
  snprintf(tag, sizeof(tag), "range seq: %s", step);
  logBoth(tag);
  printRangePairStates();

  Ina3221Reading ina_5v = {};
  Ina3221Reading ina_3v3 = {};
  const bool ok_5v = readIna3221(INA3221_ADDR_5V, ina_5v);
  const bool ok_3v3 = readIna3221(INA3221_ADDR_3V3, ina_3v3);
  if (!ok_5v && !ok_3v3) {
    logBoth("ina rails: no expected devices responded");
    return;
  }
  printInaRailsSummary(ok_5v ? &ina_5v : nullptr, ok_3v3 ? &ina_3v3 : nullptr);
}

void runRangePairSequence() {
  logBoth("range seq: begin (Q3/Q9 then Q6/Q12)");
  captureRangeSequenceSnapshot("B0 baseline");

  setRangePairEnabled(SR_BIT_3V3_HI, "Q3/Q9", false);
  delay(40);
  captureRangeSequenceSnapshot("S1 Q39OFF");

  setRangePairEnabled(SR_BIT_3V3_HI, "Q3/Q9", true);
  delay(40);
  captureRangeSequenceSnapshot("S2 Q39ON");

  setRangePairEnabled(SR_BIT_3V3_HI, "Q3/Q9", false);
  delay(40);
  captureRangeSequenceSnapshot("S3 Q39OFF");

  setRangePairEnabled(SR_BIT_ADJ_LO, "Q6/Q12", false);
  delay(40);
  captureRangeSequenceSnapshot("S4 Q612OFF");

  setRangePairEnabled(SR_BIT_ADJ_LO, "Q6/Q12", true);
  delay(40);
  captureRangeSequenceSnapshot("S5 Q612ON");

  setRangePairEnabled(SR_BIT_ADJ_LO, "Q6/Q12", false);
  delay(40);
  captureRangeSequenceSnapshot("S6 Q612OFF");

  logBoth("range seq: complete");
}

void runShiftRegisterSelfTest() {
  static const uint16_t patterns[] = {
      0x0000,
      0xFFFF,
      0xAAAA,
      0x5555,
      0x00F0,
      0x0F00,
  };

  logBoth("sr: self-test begin (interface only)");
  for (size_t i = 0; i < sizeof(patterns) / sizeof(patterns[0]); i++) {
    srShiftOut16(patterns[i]);
    char msg[64];
    snprintf(msg,
             sizeof(msg),
             "sr: pattern[%lu]=0x%04X",
             static_cast<unsigned long>(i),
             static_cast<unsigned>(patterns[i]));
    logBoth(msg);
    delay(40);
  }
  srShiftOut16(0x0000);
  logBoth("sr: self-test end");
}

void logHealthSummary() {
  char msg[160];
  if (g_aht20.valid && g_incoming_rail.valid) {
    int32_t t_whole = 0, t_frac = 0, h_whole = 0, h_frac = 0;
    char vin[16], iin[16];
    formatAhtValues(g_aht20, t_whole, t_frac, h_whole, h_frac);
    formatVoltageValue(g_incoming_rail.bus_V, vin, sizeof(vin));
    formatCurrentValue(g_incoming_rail.current_mA, iin, sizeof(iin));
    snprintf(msg,
             sizeof(msg),
             "status: flash=%s aw=%s aht=%s Vin=%sV Iin=%smA T=%ld.%02ldC RH=%ld.%02ld%% runs=%lu",
             flash_test_passed ? "PASS" : "HOLD",
             awBootStateString(),
             "PASS",
             vin,
             iin,
             static_cast<long>(t_whole),
             static_cast<long>(t_frac),
             static_cast<long>(h_whole),
             static_cast<long>(h_frac),
             static_cast<unsigned long>(flash_test_runs));
  } else if (g_aht20.valid) {
    int32_t t_whole = 0, t_frac = 0, h_whole = 0, h_frac = 0;
    formatAhtValues(g_aht20, t_whole, t_frac, h_whole, h_frac);
    snprintf(msg,
             sizeof(msg),
             "status: flash=%s aw=%s aht=%s T=%ld.%02ldC RH=%ld.%02ld%% runs=%lu",
             flash_test_passed ? "PASS" : "HOLD",
             awBootStateString(),
             "PASS",
             static_cast<long>(t_whole),
             static_cast<long>(t_frac),
             static_cast<long>(h_whole),
             static_cast<long>(h_frac),
             static_cast<unsigned long>(flash_test_runs));
  } else if (g_incoming_rail.valid) {
    char vin[16], iin[16];
    formatVoltageValue(g_incoming_rail.bus_V, vin, sizeof(vin));
    formatCurrentValue(g_incoming_rail.current_mA, iin, sizeof(iin));
    snprintf(msg,
             sizeof(msg),
             "status: flash=%s aw=%s aht=HOLD Vin=%sV Iin=%smA runs=%lu",
             flash_test_passed ? "PASS" : "HOLD",
             awBootStateString(),
             vin,
             iin,
             static_cast<unsigned long>(flash_test_runs));
  } else {
    snprintf(msg,
             sizeof(msg),
             "status: flash=%s aw=%s aht=HOLD runs=%lu",
             flash_test_passed ? "PASS" : "HOLD",
             awBootStateString(),
             static_cast<unsigned long>(flash_test_runs));
  }
  logBoth(msg);
}

void flashSelect() {
  digitalWrite(PIN_FLASH_CS, LOW);
}

void flashDeselect() {
  digitalWrite(PIN_FLASH_CS, HIGH);
}

void flashWriteEnable() {
  flashSelect();
  SPI.transfer(CMD_WREN);
  flashDeselect();
}

uint8_t flashReadStatus1() {
  flashSelect();
  SPI.transfer(CMD_RDSR1);
  const uint8_t sr1 = SPI.transfer(0x00);
  flashDeselect();
  return sr1;
}

bool flashWaitReady(uint32_t timeout_ms) {
  const uint32_t start = millis();
  while ((millis() - start) < timeout_ms) {
    if ((flashReadStatus1() & 0x01) == 0) {
      return true;
    }
    delay(2);
  }
  return false;
}

void flashReadJedec(uint8_t& manufacturer, uint8_t& mem_type, uint8_t& capacity) {
  flashSelect();
  SPI.transfer(CMD_RDID);
  manufacturer = SPI.transfer(0x00);
  mem_type = SPI.transfer(0x00);
  capacity = SPI.transfer(0x00);
  flashDeselect();
}

void flashSectorErase4K(uint32_t address) {
  flashWriteEnable();
  flashSelect();
  SPI.transfer(CMD_SECTOR_ERASE_4K);
  SPI.transfer(static_cast<uint8_t>((address >> 16) & 0xFF));
  SPI.transfer(static_cast<uint8_t>((address >> 8) & 0xFF));
  SPI.transfer(static_cast<uint8_t>(address & 0xFF));
  flashDeselect();
}

void flashPageProgram(uint32_t address, const uint8_t* data, size_t len) {
  flashWriteEnable();
  flashSelect();
  SPI.transfer(CMD_PAGE_PROGRAM);
  SPI.transfer(static_cast<uint8_t>((address >> 16) & 0xFF));
  SPI.transfer(static_cast<uint8_t>((address >> 8) & 0xFF));
  SPI.transfer(static_cast<uint8_t>(address & 0xFF));
  for (size_t i = 0; i < len; i++) {
    SPI.transfer(data[i]);
  }
  flashDeselect();
}

void flashReadData(uint32_t address, uint8_t* data, size_t len) {
  flashSelect();
  SPI.transfer(CMD_READ_DATA);
  SPI.transfer(static_cast<uint8_t>((address >> 16) & 0xFF));
  SPI.transfer(static_cast<uint8_t>((address >> 8) & 0xFF));
  SPI.transfer(static_cast<uint8_t>(address & 0xFF));
  for (size_t i = 0; i < len; i++) {
    data[i] = SPI.transfer(0x00);
  }
  flashDeselect();
}

bool flashWriteData(uint32_t address, const uint8_t* data, size_t len) {
  size_t offset = 0;
  while (offset < len) {
    const uint32_t page_offset = (address + static_cast<uint32_t>(offset)) % W25Q_PAGE_SIZE;
    size_t chunk = static_cast<size_t>(W25Q_PAGE_SIZE - page_offset);
    if (chunk > (len - offset)) {
      chunk = len - offset;
    }

    flashPageProgram(address + static_cast<uint32_t>(offset), data + offset, chunk);
    if (!flashWaitReady(1000)) {
      return false;
    }
    offset += chunk;
  }
  return true;
}

void resetPersistentConfigDefaults() {
  g_config.d9_path_enabled = 0;
  g_config.reserved0 = 0;
  g_config.reserved1 = 0;
  g_config.reserved2 = 0;

  g_config.rail_5v.voltage_gain = 1.0f;
  g_config.rail_5v.voltage_offset_mV = 0.0f;
  g_config.rail_5v.current_gain = 1.0f;
  g_config.rail_5v.current_offset_mA = 0.0f;

  g_config.rail_3v3.voltage_gain = 1.0f;
  g_config.rail_3v3.voltage_offset_mV = 0.0f;
  g_config.rail_3v3.current_gain = 1.0f;
  g_config.rail_3v3.current_offset_mA = 0.0f;
}

bool savePersistentConfig(bool verbose) {
  if (!flash_test_passed) {
    if (verbose) {
      logBoth("[CFG] Save skipped: flash bring-up test not passing");
    }
    return false;
  }

  PersistentConfigRecord record = {};
  record.magic = FLASH_CFG_MAGIC;
  record.version = FLASH_CFG_VERSION;
  record.payload_len = static_cast<uint16_t>(sizeof(record.payload));
  record.payload = g_config;
  record.crc32 = crc32(reinterpret_cast<const uint8_t*>(&record.payload), sizeof(record.payload));

  flashSectorErase4K(FLASH_CFG_ADDR);
  if (!flashWaitReady(4000)) {
    if (verbose) {
      logBoth("[CFG] Save failed: erase timeout");
    }
    return false;
  }

  if (!flashWriteData(FLASH_CFG_ADDR, reinterpret_cast<const uint8_t*>(&record), sizeof(record))) {
    if (verbose) {
      logBoth("[CFG] Save failed: write timeout");
    }
    return false;
  }

  if (verbose) {
    logBoth("[CFG] Saved");
  }
  return true;
}

bool loadPersistentConfig(bool verbose) {
  if (!flash_test_passed) {
    if (verbose) {
      logBoth("[CFG] Load skipped: flash bring-up test not passing");
    }
    return false;
  }

  PersistentConfigRecord record = {};
  flashReadData(FLASH_CFG_ADDR, reinterpret_cast<uint8_t*>(&record), sizeof(record));

  if (record.magic != FLASH_CFG_MAGIC) {
    if (verbose) {
      logBoth("[CFG] No saved config signature");
    }
    return false;
  }
  if (record.version != FLASH_CFG_VERSION) {
    if (verbose) {
      char msg[96];
      snprintf(msg,
               sizeof(msg),
               "[CFG] Version mismatch: got %u expected %u",
               static_cast<unsigned>(record.version),
               static_cast<unsigned>(FLASH_CFG_VERSION));
      logBoth(msg);
    }
    return false;
  }
  if (record.payload_len != sizeof(record.payload)) {
    if (verbose) {
      logBoth("[CFG] Size mismatch");
    }
    return false;
  }

  const uint32_t expected_crc = crc32(reinterpret_cast<const uint8_t*>(&record.payload), sizeof(record.payload));
  if (record.crc32 != expected_crc) {
    if (verbose) {
      logBoth("[CFG] CRC mismatch");
    }
    return false;
  }

  if (record.payload.rail_5v.voltage_gain <= 0.0f || record.payload.rail_3v3.voltage_gain <= 0.0f ||
      record.payload.rail_5v.current_gain <= 0.0f || record.payload.rail_3v3.current_gain <= 0.0f) {
    if (verbose) {
      logBoth("[CFG] Invalid gain values");
    }
    return false;
  }

  g_config = record.payload;
  if (verbose) {
    logBoth("[CFG] Loaded");
  }
  return true;
}

bool erasePersistentConfig(bool verbose) {
  if (!flash_test_passed) {
    if (verbose) {
      logBoth("[CFG] Erase skipped: flash bring-up test not passing");
    }
    return false;
  }
  flashSectorErase4K(FLASH_CFG_ADDR);
  if (!flashWaitReady(4000)) {
    if (verbose) {
      logBoth("[CFG] Erase timeout");
    }
    return false;
  }
  if (verbose) {
    logBoth("[CFG] Erased");
  }
  return true;
}

void printPersistentConfig() {
  char msg[192];
  snprintf(msg,
           sizeof(msg),
           "cfg: d9_default=%s 5V[vGain=%.5f vOff=%.2fmV iGain=%.5f iOff=%.2fmA] 3V3[vGain=%.5f vOff=%.2fmV iGain=%.5f iOff=%.2fmA]",
           g_config.d9_path_enabled ? "ON" : "OFF",
           g_config.rail_5v.voltage_gain,
           g_config.rail_5v.voltage_offset_mV,
           g_config.rail_5v.current_gain,
           g_config.rail_5v.current_offset_mA,
           g_config.rail_3v3.voltage_gain,
           g_config.rail_3v3.voltage_offset_mV,
           g_config.rail_3v3.current_gain,
           g_config.rail_3v3.current_offset_mA);
  logBoth(msg);
}

bool runFlashBringupTest() {
  logBoth("flash: begin bring-up test");

  uint8_t manufacturer = 0;
  uint8_t mem_type = 0;
  uint8_t capacity = 0;
  flashReadJedec(manufacturer, mem_type, capacity);

  char id_msg[96];
  snprintf(id_msg,
           sizeof(id_msg),
           "flash: JEDEC ID mfg=0x%02X type=0x%02X cap=0x%02X",
           manufacturer,
           mem_type,
           capacity);
  logBoth(id_msg);

  // Winbond W25Q128 typical ID: EF 40 18.
  if (manufacturer != 0xEF) {
    logBoth("flash: unexpected manufacturer (expected Winbond 0xEF)");
    return false;
  }

  const uint8_t sr1_before = flashReadStatus1();
  char sr_msg[64];
  snprintf(sr_msg, sizeof(sr_msg), "flash: SR1 before=0x%02X", sr1_before);
  logBoth(sr_msg);

  uint8_t tx[FLASH_TEST_LEN];
  uint8_t rx[FLASH_TEST_LEN];
  for (size_t i = 0; i < FLASH_TEST_LEN; i++) {
    tx[i] = static_cast<uint8_t>(0xA0 + i);
    rx[i] = 0;
  }

  flashSectorErase4K(FLASH_TEST_ADDR);
  if (!flashWaitReady(4000)) {
    logBoth("flash: sector erase timeout");
    return false;
  }

  flashPageProgram(FLASH_TEST_ADDR, tx, FLASH_TEST_LEN);
  if (!flashWaitReady(1000)) {
    logBoth("flash: page program timeout");
    return false;
  }

  flashReadData(FLASH_TEST_ADDR, rx, FLASH_TEST_LEN);
  if (memcmp(tx, rx, FLASH_TEST_LEN) != 0) {
    logBoth("flash: readback mismatch");
    return false;
  }

  logBoth("flash: erase/program/readback PASS");
  return true;
}

// ── CRC8 calculation (poly=0x07, init=0x00) ───────────────────────────────
uint8_t crc8(const uint8_t* data, size_t len) {
  uint8_t crc = 0x00;
  for (size_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (int j = 0; j < 8; j++) {
      if (crc & 0x80) {
        crc = (crc << 1) ^ 0x07;
      } else {
        crc <<= 1;
      }
    }
  }
  return crc;
}

// ── Build and send an extended telemetry frame on USART3 ──────────────────
// Layout after tag:
// [seq][v5_mV(2)][i5_mA(2)][v3v3_mV(2)][i3v3_mA(2)][temp_C][status][protection_flags]
void publishTelemetry(uint16_t v5_mV,
                      int16_t i5_mA,
                      uint16_t v3v3_mV,
                      int16_t i3v3_mA,
                      uint8_t temp_C,
                      uint8_t status,
                      uint8_t protection_flags) {
  uint8_t frame[FRAME_SIZE];
  frame[0] = FRAME_SOF1;
  frame[1] = FRAME_SOF2;
  frame[2] = FRAME_LEN;
  frame[3] = FRAME_TAG;
  frame[4] = frame_seq++;

  frame[5] = static_cast<uint8_t>(v5_mV & 0xFF);
  frame[6] = static_cast<uint8_t>((v5_mV >> 8) & 0xFF);

  const uint16_t i5_u = static_cast<uint16_t>(i5_mA);
  frame[7] = static_cast<uint8_t>(i5_u & 0xFF);
  frame[8] = static_cast<uint8_t>((i5_u >> 8) & 0xFF);

  frame[9] = static_cast<uint8_t>(v3v3_mV & 0xFF);
  frame[10] = static_cast<uint8_t>((v3v3_mV >> 8) & 0xFF);

  const uint16_t i3_u = static_cast<uint16_t>(i3v3_mA);
  frame[11] = static_cast<uint8_t>(i3_u & 0xFF);
  frame[12] = static_cast<uint8_t>((i3_u >> 8) & 0xFF);

  frame[13] = temp_C;
  frame[14] = status;
  frame[15] = protection_flags;

  // CRC over [2..15] (len + tag + payload)
  frame[16] = crc8(&frame[2], 14);
  
  // Send binary frame
  SerialU3.write(frame, FRAME_SIZE);
}

bool isFaultCriticalActive() {
  // Rev-C fault sum is active-low on the current comparator aggregation path.
  return digitalRead(PIN_FAULT_CRITICAL_SUM) == LOW;
}

bool is3v3PathEnabled() {
  const uint16_t mask = static_cast<uint16_t>((1u << SR_BIT_3V3_HI) | (1u << SR_BIT_3V3_LO));
  return (g_sr_state & mask) == 0u;
}

void setup() {
  // Keep control outputs inactive as early as possible.
  pinMode(PIN_ISET_5V, OUTPUT);
  pinMode(PIN_ISET_3V3, OUTPUT);
  pinMode(PIN_ISET_CH3, OUTPUT);
  pinMode(PIN_FAULT_CRITICAL_SUM, INPUT_PULLUP);
  digitalWrite(PIN_ISET_5V, LOW);
  digitalWrite(PIN_ISET_3V3, LOW);
  digitalWrite(PIN_ISET_CH3, LOW);

  pinMode(PIN_STATUS_LED, OUTPUT);
  digitalWrite(PIN_STATUS_LED, HIGH); // LED off (active-low)

  pinMode(PIN_FLASH_CS, OUTPUT);
  digitalWrite(PIN_FLASH_CS, HIGH);

  pinMode(PIN_SR_LATCH, OUTPUT);
  digitalWrite(PIN_SR_LATCH, LOW);

  SPI.begin();
  SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));

  Wire.setSCL(I2C_SCL_PIN);
  Wire.setSDA(I2C_SDA_PIN);
  Wire.begin();

  Serial.begin(115200);
  SerialDbg.begin(115200);
  delay(150);
  Serial.println("stm32-bluepill bringup: boot");
  SerialDbg.println("stm32-bluepill bringup: boot");

  aw95xxBootInit();

  // USART3 on PB10/PB11 for future HAT->CrowPanel link validation.
  SerialU3.begin(115200);
  SerialU3.println("stm32-bluepill usart3: ready");
  SerialDbg.println("stm32-bluepill usart3: ready");

  resetPersistentConfigDefaults();
  printCommandHelp();
  flash_test_passed = runFlashBringupTest();
  flash_test_runs = 1;
  if (flash_test_passed) {
    if (!loadPersistentConfig(true)) {
      savePersistentConfig(true);
    }
  } else {
    logBoth("[CFG] Flash not healthy; using volatile defaults only");
  }
  setD9PathEnabled(g_config.d9_path_enabled != 0);
  g_output_enabled = (g_config.d9_path_enabled != 0);
  printPersistentConfig();

  Aht20Sample boot_sample;
  if (readAht20Now(boot_sample)) {
    g_aht20 = boot_sample;
    logBoth("aht20: startup probe PASS");
  } else {
    logBoth("aht20: startup probe HOLD");
  }
  printInaProbeSummary();
  if (refreshIncomingRailSample()) {
    logBoth("ina: incoming rail sample PASS");
  } else {
    logBoth("ina: incoming rail sample HOLD");
  }
  logHealthSummary();
}

void loop() {
  pollCommands();

  static uint32_t lastMs = 0;
  static uint32_t lastSummaryMs = 0;
  uint32_t now = millis();

  if (now - lastMs >= 5000) {
    lastMs = now;
    digitalWrite(PIN_STATUS_LED, LOW);
    delay(40);
    digitalWrite(PIN_STATUS_LED, HIGH);

    refreshIncomingRailSample();

    if (g_hb_print_enabled) {
      Serial.println("hb");
      SerialDbg.println("hb");

      if (g_incoming_rail.valid) {
        char ina_hb_msg[96];
        char vin[16], iin[16];
        formatVoltageValue(g_incoming_rail.bus_V, vin, sizeof(vin));
        formatCurrentValue(g_incoming_rail.current_mA, iin, sizeof(iin));
        snprintf(ina_hb_msg,
                 sizeof(ina_hb_msg),
                 "ina hb: Vin=%sV Iin=%smA",
                 vin,
                 iin);
        logBoth(ina_hb_msg);
      } else {
        logBoth("ina hb: HOLD");
      }

      if (!flash_test_passed) {
        Serial.println("flash: HOLD (bring-up test failed)");
        SerialDbg.println("flash: HOLD (bring-up test failed)");
      }
    }
    
    // Publish live telemetry from INA3221 rails when available.
    Ina3221Reading ina_5v = {};
    Ina3221Reading ina_3v3 = {};
    const bool ok_5v = readIna3221(INA3221_ADDR_5V, ina_5v);
    const bool ok_3v3 = readIna3221(INA3221_ADDR_3V3, ina_3v3);

    const auto clampU16 = [](float value, uint16_t fallback) -> uint16_t {
      if (value < 0.0f) return fallback;
      if (value > 65535.0f) return 65535;
      return static_cast<uint16_t>(value + 0.5f);
    };
    const auto clampI16 = [](float value, int16_t fallback) -> int16_t {
      if (value < -32768.0f || value > 32767.0f) return fallback;
      return static_cast<int16_t>(value + (value >= 0.0f ? 0.5f : -0.5f));
    };

    float v5_bus = 5.0f;
    float i5_bus = 500.0f;
    float v3v3_bus = 3.3f;
    float i3v3_bus = 320.0f;
    if (ok_5v) {
      const RailCalibrationConfig& cal = calibrationForRail(INA3221_ADDR_5V);
      v5_bus = applyVoltageCalibration(ina_5v.channel[0].bus_V, cal);
      i5_bus = applyCurrentCalibration(ina_5v.channel[0].current_mA, cal);
    }
    if (ok_3v3) {
      const RailCalibrationConfig& cal = calibrationForRail(INA3221_ADDR_3V3);
      v3v3_bus = applyVoltageCalibration(ina_3v3.channel[0].bus_V, cal);
      i3v3_bus = applyCurrentCalibration(ina_3v3.channel[0].current_mA, cal);
    }

    const uint16_t v5_mV = clampU16(v5_bus * 1000.0f, 5000);
    const int16_t i5_mA = clampI16(i5_bus, 500);
    const uint16_t v3v3_mV = clampU16(v3v3_bus * 1000.0f, 3300);
    const int16_t i3v3_mA = clampI16(i3v3_bus, 320);
    const uint8_t temp_C = g_aht20.valid
        ? static_cast<uint8_t>(constrain(static_cast<int>(g_aht20.temp_C + 0.5f), 0, 125))
        : 31;
    const bool ch1_enabled = g_output_enabled && ok_5v && (v5_mV >= CH1_ENABLED_MIN_MV);
    const bool ch2_enabled = g_output_enabled && ok_3v3 && is3v3PathEnabled() && (v3v3_mV >= CH2_ENABLED_MIN_MV);
    const bool ch1_cc = static_cast<uint16_t>(abs(i5_mA)) >= g_current_limit_ch1_mA;
    const bool ch2_cc = static_cast<uint16_t>(abs(i3v3_mA)) >= g_current_limit_ch2_mA;
    const bool thermal_warn = temp_C >= THERMAL_WARN_THRESHOLD_C;
    const bool ch1_ovp = ok_5v && (v5_mV >= CH1_OVP_THRESHOLD_MV);
    const bool ch2_ovp = ok_3v3 && (v3v3_mV >= CH2_OVP_THRESHOLD_MV);
    const bool otp_trip = temp_C >= OTP_THRESHOLD_C;
    const bool ocp_sum_trip = isFaultCriticalActive();

    uint8_t status = 0x00;
    status |= ch1_enabled ? 0x80u : 0x00u;
    status |= ch2_enabled ? 0x40u : 0x00u;
    status |= (!ch1_cc) ? 0x20u : 0x00u;
    status |= (!ch2_cc) ? 0x10u : 0x00u;
    status |= thermal_warn ? 0x08u : 0x00u;

    uint8_t protection_flags = 0x00;
    protection_flags |= ch1_ovp ? 0x80u : 0x00u;
    protection_flags |= ocp_sum_trip ? 0x40u : 0x00u;
    protection_flags |= ch2_ovp ? 0x20u : 0x00u;
    protection_flags |= ocp_sum_trip ? 0x10u : 0x00u;
    protection_flags |= otp_trip ? 0x08u : 0x00u;
    protection_flags |= otp_trip ? 0x04u : 0x00u;
    publishTelemetry(v5_mV, i5_mA, v3v3_mV, i3v3_mA, temp_C, status, protection_flags);
  }

  if (now - lastSummaryMs >= 60000) {
    lastSummaryMs = now;
    Aht20Sample periodic_sample;
    if (readAht20Now(periodic_sample)) {
      g_aht20 = periodic_sample;
    }
    refreshIncomingRailSample();
    logHealthSummary();
  }
}
