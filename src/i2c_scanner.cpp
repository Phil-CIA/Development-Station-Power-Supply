#include <Arduino.h>

#include <string.h>

#include "disp_link.h"

// ----------------------------------------------------------------------------
// Bit-bang I2C master on GPIO5 (SDA) / GPIO6 (SCL).
//
// This bus currently serves bring-up reads for INA3221 devices and the U5
// AHT20 temperature/humidity sensor. Pull-ups are expected externally;
// INPUT_PULLUP is also enabled as a fallback.
// ----------------------------------------------------------------------------
constexpr int INA_SDA = 5;
constexpr int INA_SCL = 6;
constexpr uint32_t I2C_BIT_DELAY_US = 5;  // ~100 kHz
constexpr bool OTA_ENABLED = false;

static inline void sdaHigh() { pinMode(INA_SDA, INPUT_PULLUP); }
static inline void sdaLow()  { pinMode(INA_SDA, OUTPUT); digitalWrite(INA_SDA, LOW); }
static inline void sclHigh() { pinMode(INA_SCL, INPUT_PULLUP); }
static inline void sclLow()  { pinMode(INA_SCL, OUTPUT); digitalWrite(INA_SCL, LOW); }
static inline int  sdaRead() { return digitalRead(INA_SDA); }
static inline void i2cDelay() { delayMicroseconds(I2C_BIT_DELAY_US); }

static void i2cStart() {
  sdaHigh(); sclHigh(); i2cDelay();
  sdaLow();  i2cDelay();
  sclLow();  i2cDelay();
}
static void i2cStop() {
  sdaLow();  sclHigh(); i2cDelay();
  sdaHigh(); i2cDelay();
}
static void i2cRestart() {
  sdaHigh(); i2cDelay();
  sclHigh(); i2cDelay();
  sdaLow();  i2cDelay();
  sclLow();  i2cDelay();
}

// Returns 0 = ACK from slave, 1 = NACK.
static int i2cWriteByte(uint8_t b) {
  for (int i = 7; i >= 0; i--) {
    if ((b >> i) & 1) sdaHigh(); else sdaLow();
    i2cDelay();
    sclHigh(); i2cDelay();
    sclLow();  i2cDelay();
  }
  sdaHigh();          // release for slave ACK
  i2cDelay();
  sclHigh(); i2cDelay();
  const int ack = sdaRead();
  sclLow();  i2cDelay();
  return ack;
}

// ack=0 -> we ACK (more bytes to follow), ack=1 -> we NACK (last byte).
static uint8_t i2cReadByte(int ack) {
  sdaHigh();          // release SDA so slave can drive
  uint8_t b = 0;
  for (int i = 7; i >= 0; i--) {
    i2cDelay();
    sclHigh(); i2cDelay();
    b |= (sdaRead() & 1) << i;
    sclLow();
  }
  if (ack == 0) sdaLow(); else sdaHigh();
  i2cDelay();
  sclHigh(); i2cDelay();
  sclLow();  i2cDelay();
  sdaHigh();
  return b;
}

// Returns true if slave ACKs (address present), false otherwise.
static bool inaPing(uint8_t addr) {
  i2cStart();
  const int ack = i2cWriteByte(static_cast<uint8_t>(addr << 1));
  i2cStop();
  return ack == 0;
}

static bool inaWriteReg16(uint8_t addr, uint8_t reg, uint16_t value) {
  i2cStart();
  if (i2cWriteByte(static_cast<uint8_t>(addr << 1)))           { i2cStop(); return false; }
  if (i2cWriteByte(reg))                                       { i2cStop(); return false; }
  if (i2cWriteByte(static_cast<uint8_t>((value >> 8) & 0xFF))) { i2cStop(); return false; }
  if (i2cWriteByte(static_cast<uint8_t>(value & 0xFF)))        { i2cStop(); return false; }
  i2cStop();
  return true;
}

static bool inaReadReg16(uint8_t addr, uint8_t reg, uint16_t& out) {
  i2cStart();
  if (i2cWriteByte(static_cast<uint8_t>(addr << 1))) { i2cStop(); return false; }
  if (i2cWriteByte(reg))                              { i2cStop(); return false; }
  i2cRestart();
  if (i2cWriteByte(static_cast<uint8_t>((addr << 1) | 1))) { i2cStop(); return false; }
  const uint8_t hi = i2cReadByte(0);  // ACK, more to come
  const uint8_t lo = i2cReadByte(1);  // NACK, last byte
  i2cStop();
  out = (static_cast<uint16_t>(hi) << 8) | lo;
  return true;
}

// INA3221 register conversions (matching SDL_Arduino_INA3221 library):
//   Bus voltage register: 16-bit signed, upper 13 bits; LSB = 8 mV.
//   Shunt voltage register: 16-bit signed, upper 13 bits; LSB = 40 µV.
static float inaBusVoltageV(uint16_t raw) {
  const int16_t s = static_cast<int16_t>(raw) >> 3;
  return static_cast<float>(s) * 0.008f;
}
static float inaShuntMillivolts(uint16_t raw) {
  const int16_t s = static_cast<int16_t>(raw) >> 3;
  return static_cast<float>(s) * 0.04f;
}

// Configure INA3221 (default config 0x7127: continuous mode, 1.1 ms conv).
static bool inaInit(uint8_t addr) {
  return inaWriteReg16(addr, 0x00, 0x7127);
}

// ----------------------------------------------------------------------------
// Shift register control (74HC595 x2 chain — U4 first, U5 daisy-chained)
// SR_Data  → U4 DS      (GPIO3)
// SR_CLK   → U4/U5 SHCP (GPIO4)
// SR_Latch → U4/U5 STCP (GPIO1)
// OE# tied to GND (always output-enabled); MR# pulled HIGH (no reset).
//
// 16-bit value layout after shiftOut16(value):
//   bits [15:8] → U5 Q7..Q0
//   bits  [7:0] → U4 Q7..Q0
//
// U4 FET gate bit assignments (PMOS: gate LOW = FET ON):
//   bit 0 (U4 Q0): ISET_MPU_5V       → J4 pin 5 (not a load switch)
//   bit 1 (U4 Q1): ESP-GPIO 5V Hi    → R8 → Q2+Q8 gates (5V Low-Range)
//   bit 2 (U4 Q2): ESP-GPIO 5V Low   → R7 → Q1+Q7 gates (5V Hi-Range)
//   bit 3 (U4 Q3): ESP-GPIO 3.3V Hi  → R9 → Q3+Q9 gates (3.3V Hi-Range)
//   bit 4 (U4 Q4): ESP-GPIO 3.3V Low → R10 → Q4+Q10 gates (3.3V Low-Range)
//   bit 5 (U4 Q5): Channel 3 Lo-Rng  → R12 → Q6+Q12 gates (Adj Low-Range)
//   bit 6 (U4 Q6): Channel 3 Hi-Rng  → R11 → Q5+Q11 gates (Adj Hi-Range)
//   bit 7 (U4 Q7): ISET_MPU_3V3      → J4 pin 10 (not a load switch)
//   bits [15:8] (U5 Q0-Q7): channel select / ISET signals → keep 0
// ----------------------------------------------------------------------------
constexpr uint8_t SR_DATA_PIN  = 3;
constexpr uint8_t SR_CLK_PIN   = 4;
constexpr uint8_t SR_LATCH_PIN = 1;
constexpr bool SR_SELF_TEST_ENABLED = false;

// Bitmask for U4 bits 1-6: all six load-switch FET enables
constexpr uint16_t SR_ALL_CH_ENABLE = 0x0000;  // all bits 0 = all PMOS gates LOW = all FETs ON
constexpr uint16_t SR_ALL_CH_OFF    = 0x007E;  // bits 1-6 = 1 = all FETs OFF
constexpr uint8_t SR_BIT_3V3_LOW_RANGE = 4;    // U4 Q4

static uint16_t g_sr_state = SR_ALL_CH_OFF;

void shiftOut16(uint16_t value) {
  // Send 16 bits MSB-first: bits[15:8] go to U5, bits[7:0] go to U4.
  for (int i = 15; i >= 0; i--) {
    digitalWrite(SR_DATA_PIN, (value >> i) & 1);
    digitalWrite(SR_CLK_PIN, HIGH);
    delayMicroseconds(2);
    digitalWrite(SR_CLK_PIN, LOW);
    delayMicroseconds(2);
  }
  digitalWrite(SR_LATCH_PIN, HIGH);
  delayMicroseconds(2);
  digitalWrite(SR_LATCH_PIN, LOW);
}

void applyShiftRegisterState(const char* reason) {
  shiftOut16(g_sr_state);
  Serial.printf("SR state [%s]: 0x%04X (3V3_LOW=%s)\n",
                reason,
                static_cast<unsigned>(g_sr_state),
                ((g_sr_state >> SR_BIT_3V3_LOW_RANGE) & 0x1) ? "OFF" : "ON");
}

void set3v3LowRange(bool on) {
  const uint16_t bit = static_cast<uint16_t>(1u << SR_BIT_3V3_LOW_RANGE);
  if (on) {
    g_sr_state &= static_cast<uint16_t>(~bit);  // PMOS gate LOW => ON
    applyShiftRegisterState("3V3LOW ON");
  } else {
    g_sr_state |= bit;                          // PMOS gate HIGH => OFF
    applyShiftRegisterState("3V3LOW OFF");
  }
}

void enableAllChannels() {
  g_sr_state = SR_ALL_CH_OFF;
  applyShiftRegisterState("idle");
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

  Serial.println("SR self-test: begin (interface-only pattern sweep)");
  for (size_t i = 0; i < sizeof(patterns) / sizeof(patterns[0]); i++) {
    shiftOut16(patterns[i]);
    Serial.printf("SR self-test pattern[%u] = 0x%04X\n",
                  static_cast<unsigned>(i),
                  static_cast<unsigned>(patterns[i]));
    delay(40);
  }
  shiftOut16(SR_ALL_CH_OFF);
  Serial.println("SR self-test: end (returned to idle pattern)");
}

struct I2cBusCandidate {
  const char* name;
  uint8_t sda;
  uint8_t scl;
};

// INA3221 sensor bus: bit-bang master on GPIO5/GPIO6 (see top of file).
// HP_I2C0 hardware peripheral is dedicated to disp_link's IDF slave on GPIO0/GPIO7.
constexpr I2cBusCandidate BUS_CANDIDATES[] = {
  {"bus0", INA_SDA, INA_SCL},
};

constexpr uint8_t TARGET_ADDRESSES[] = {0x40, 0x41, 0x43};
constexpr uint8_t AHT20_ADDRESS = 0x38;

constexpr uint8_t AHT20_CMD_SOFT_RESET = 0xBA;
constexpr uint8_t AHT20_CMD_INIT = 0xBE;
constexpr uint8_t AHT20_CMD_MEASURE = 0xAC;

constexpr uint8_t AHT20_STATUS_BUSY = 0x80;
constexpr uint8_t AHT20_STATUS_CALIBRATED = 0x08;

struct Aht20Reading {
  bool valid = false;
  float temperature_C = 0.0f;
  float humidity_pct = 0.0f;
  uint8_t status = 0;
};

Aht20Reading g_aht20;
uint32_t g_last_aht_print_ms = 0;
constexpr uint32_t AHT_PRINT_INTERVAL_MS = 1000;

// Last good 12V input reading captured from INA3221 0x41 CH3 — published over
// disp_link to the CrowPanel once per scan cycle.
struct Last12V {
  bool   valid = false;
  float  bus_v = 0.0f;
  float  current_mA = 0.0f;
};
Last12V g_last_12v;

static bool ahtWriteCmd1(uint8_t cmd) {
  i2cStart();
  if (i2cWriteByte(static_cast<uint8_t>(AHT20_ADDRESS << 1))) { i2cStop(); return false; }
  if (i2cWriteByte(cmd)) { i2cStop(); return false; }
  i2cStop();
  return true;
}

static bool ahtWriteCmd3(uint8_t cmd, uint8_t arg1, uint8_t arg2) {
  i2cStart();
  if (i2cWriteByte(static_cast<uint8_t>(AHT20_ADDRESS << 1))) { i2cStop(); return false; }
  if (i2cWriteByte(cmd)) { i2cStop(); return false; }
  if (i2cWriteByte(arg1)) { i2cStop(); return false; }
  if (i2cWriteByte(arg2)) { i2cStop(); return false; }
  i2cStop();
  return true;
}

static bool ahtReadBytes(uint8_t* out, size_t len) {
  i2cStart();
  if (i2cWriteByte(static_cast<uint8_t>((AHT20_ADDRESS << 1) | 1))) { i2cStop(); return false; }
  for (size_t i = 0; i < len; i++) {
    const int ack = (i + 1 < len) ? 0 : 1;
    out[i] = i2cReadByte(ack);
  }
  i2cStop();
  return true;
}

static bool ahtReadStatus(uint8_t& status) {
  i2cStart();
  if (i2cWriteByte(static_cast<uint8_t>((AHT20_ADDRESS << 1) | 1))) { i2cStop(); return false; }
  status = i2cReadByte(1);
  i2cStop();
  return true;
}

static bool ahtEnsureInitialized() {
  uint8_t status = 0;
  if (!ahtReadStatus(status)) return false;
  if ((status & AHT20_STATUS_CALIBRATED) != 0) return true;

  if (!ahtWriteCmd3(AHT20_CMD_INIT, 0x08, 0x00)) return false;
  delay(12);
  if (!ahtReadStatus(status)) return false;
  return (status & AHT20_STATUS_CALIBRATED) != 0;
}

static bool ahtReadMeasurement(Aht20Reading& out) {
  out.valid = false;

  if (!ahtWriteCmd3(AHT20_CMD_MEASURE, 0x33, 0x00)) return false;
  delay(80);

  uint8_t status = 0;
  const uint32_t deadline = millis() + 120;
  while (millis() < deadline) {
    if (!ahtReadStatus(status)) return false;
    if ((status & AHT20_STATUS_BUSY) == 0) break;
    delay(10);
  }
  if ((status & AHT20_STATUS_BUSY) != 0) return false;

  // AHT20 readout frame: [status][hum20:3 bytes][temp20:3 bytes][crc]
  uint8_t frame[7] = {0};
  if (!ahtReadBytes(frame, sizeof(frame))) return false;

  const uint32_t raw_h =
      (static_cast<uint32_t>(frame[1]) << 12) |
      (static_cast<uint32_t>(frame[2]) << 4) |
      ((static_cast<uint32_t>(frame[3]) & 0xF0) >> 4);
  const uint32_t raw_t =
      ((static_cast<uint32_t>(frame[3]) & 0x0F) << 16) |
      (static_cast<uint32_t>(frame[4]) << 8) |
      static_cast<uint32_t>(frame[5]);

  out.humidity_pct = (static_cast<float>(raw_h) * 100.0f) / 1048576.0f;
  out.temperature_C = (static_cast<float>(raw_t) * 200.0f) / 1048576.0f - 50.0f;
  out.status = frame[0];
  out.valid = true;
  return true;
}

void pollAht20() {
  if (!inaPing(AHT20_ADDRESS)) {
    if (millis() - g_last_aht_print_ms >= AHT_PRINT_INTERVAL_MS) {
      Serial.println("AHT20 0x38: no response");
      g_last_aht_print_ms = millis();
    }
    return;
  }

  bool init_ok = ahtEnsureInitialized();
  if (!init_ok) {
    // Recovery path for startup edge cases.
    ahtWriteCmd1(AHT20_CMD_SOFT_RESET);
    delay(25);
    init_ok = ahtEnsureInitialized();
  }

  if (!init_ok) {
    if (millis() - g_last_aht_print_ms >= AHT_PRINT_INTERVAL_MS) {
      Serial.println("AHT20 0x38: init/calibration failed");
      g_last_aht_print_ms = millis();
    }
    return;
  }

  Aht20Reading sample;
  if (!ahtReadMeasurement(sample)) {
    if (millis() - g_last_aht_print_ms >= AHT_PRINT_INTERVAL_MS) {
      Serial.println("AHT20 0x38: measurement read failed");
      g_last_aht_print_ms = millis();
    }
    return;
  }

  g_aht20 = sample;
  if (millis() - g_last_aht_print_ms >= AHT_PRINT_INTERVAL_MS) {
    Serial.printf("AHT20 0x38: T=%.2f C RH=%.2f %% status=0x%02X\n",
                  g_aht20.temperature_C,
                  g_aht20.humidity_pct,
                  static_cast<unsigned>(g_aht20.status));
    g_last_aht_print_ms = millis();
  }
}

void printCommandHelp() {
  Serial.println("Commands: HELP | SRTEST | AHTNOW | AHTRESET | SRSTATE | 3V3LOWON | 3V3LOWOFF");
}

void handleSerialCommands() {
  if (!Serial.available()) return;

  const String cmd = Serial.readStringUntil('\n');
  String normalized = cmd;
  normalized.trim();
  normalized.toUpperCase();
  if (normalized.length() == 0) return;

  if (normalized == "HELP") {
    printCommandHelp();
    return;
  }
  if (normalized == "SRTEST") {
    runShiftRegisterSelfTest();
    return;
  }
  if (normalized == "AHTNOW") {
    pollAht20();
    if (g_aht20.valid) {
      Serial.printf("AHT20 sample: T=%.2f C RH=%.2f %% status=0x%02X\n",
                    g_aht20.temperature_C,
                    g_aht20.humidity_pct,
                    static_cast<unsigned>(g_aht20.status));
    } else {
      Serial.println("AHT20 sample unavailable");
    }
    return;
  }
  if (normalized == "AHTRESET") {
    if (!ahtWriteCmd1(AHT20_CMD_SOFT_RESET)) {
      Serial.println("AHT20 reset command failed");
      return;
    }
    delay(25);
    const bool init_ok = ahtEnsureInitialized();
    Serial.printf("AHT20 reset done, init=%s\n", init_ok ? "OK" : "FAIL");
    return;
  }
  if (normalized == "SRSTATE") {
    applyShiftRegisterState("query");
    return;
  }
  if (normalized == "3V3LOWON") {
    set3v3LowRange(true);
    return;
  }
  if (normalized == "3V3LOWOFF") {
    set3v3LowRange(false);
    return;
  }

  Serial.print("Unknown command: ");
  Serial.println(normalized);
  printCommandHelp();
}

float shuntOhmsForTap(uint8_t address, int ch) {
  // Shunt resistor map — confirmed from DSP Regulator_hat.net netlist + bench measurements.
  //
  // SHUNT COMPONENTS (2512 package, in load-switch FET path):
  //   R13 = U1 (0x40) CH1, R14 = U1 CH2
  //   R15 = U2 (0x41) CH1, R16 = U2 CH2
  //   R17 = U3 (0x43) CH1, R18 = U3 CH2
  //   All use LCSC C7420022 = 200mOhm 2W (confirmed from netlist BOM fields).
  //   NOTE: Schematic value fields for R13/R15/R17 say "20mOhm" — this is a
  //   schematic error. The linked LCSC part C7420022 is 200mOhm. Both
  //   CH1 (Hi-Range) and CH2 (Low-Range) are the same 200mOhm physical part.
  //
  // CURRENT PATH CAVEAT:
  //   CH1/CH2 for all rails are in series with output load-switch MOSFETs
  //   (Q1/Q7 for 5V, Q3/Q9 for 3.3V, Q5/Q11 for Adj Hi, etc.), driven by
  //   the GPIO shift registers (U4/U5). Until those MOSFETs are enabled by
  //   firmware, CH1/CH2 shunt readings will show near-zero even under load.
  //
  // 0x41 CH3 (12V input monitor):
  //   No dedicated shunt on the HAT board. R34/R35 are 10-Ohm EMI filters
  //   connecting "Incoming(+)" / "Incoming(-)" at J6 pins 9/8 to U2 IN+3/IN-3.
  //   The ~18 mOhm measured is from trace/connector resistance on the regulator
  //   board side. Value derived empirically: delta 3.000 mV / 178 mA = 16.85 mOhm,
  //   absolute at idle 0.960 mV / 50 mA = 19.2 mOhm; using 18 mOhm as best fit.
  //
  // 0x40 CH3, 0x43 CH3: unconnected on the HAT (confirmed zero readings).
  if (address == 0x40) {
    if (ch == 1) return 0.200f;  // R13, 200mOhm (C7420022) — needs load switch enabled
    if (ch == 2) return 0.200f;  // R14, 200mOhm (C7420022) — needs load switch enabled
    return 1.000f;               // CH3 unconnected
  }
  if (address == 0x41) {
    if (ch == 1) return 0.200f;  // R15, 200mOhm (C7420022) — needs load switch enabled
    if (ch == 2) return 0.200f;  // R16, 200mOhm (C7420022) — needs load switch enabled
    if (ch == 3) return 0.018f;  // 12V input, no HAT shunt; ~18mOhm trace/connector (empirical)
    return 1.000f;
  }
  if (address == 0x43) {
    if (ch == 1) return 0.200f;  // R17, 200mOhm (C7420022) — needs load switch enabled
    if (ch == 2) return 0.200f;  // R18, 200mOhm (C7420022) — needs load switch enabled
    return 1.000f;               // CH3 unconnected
  }
  return 1.000f;
}

void printInaReadings(uint8_t address) {
  inaInit(address);

  Serial.printf("INA3221 0x%02X readings:\n", address);
  for (int ch = 1; ch <= 3; ch++) {
    // INA3221 register map: shunt voltage = 0x01,0x03,0x05; bus voltage = 0x02,0x04,0x06
    uint16_t bus_raw = 0, shunt_raw = 0;
    const uint8_t bus_reg   = static_cast<uint8_t>(0x02 + (ch - 1) * 2);
    const uint8_t shunt_reg = static_cast<uint8_t>(0x01 + (ch - 1) * 2);
    inaReadReg16(address, bus_reg,   bus_raw);
    inaReadReg16(address, shunt_reg, shunt_raw);

    const float busVoltageV = inaBusVoltageV(bus_raw);
    const float shuntmV     = inaShuntMillivolts(shunt_raw);
    const float shuntOhms   = shuntOhmsForTap(address, ch);
    const float currentmA   = shuntmV / shuntOhms;
    Serial.printf("  CH%d: %.3f V, %.3f mA (shunt %.3f mV, R=%.3f ohm)\n",
                  ch,
                  busVoltageV,
                  currentmA,
                  shuntmV,
                  shuntOhms);

    // Capture the 12V input monitor (0x41 CH3) — the only working channel on
    // this board — for publication to the CrowPanel display.
    if (address == 0x41 && ch == 3) {
      g_last_12v.bus_v = busVoltageV;
      g_last_12v.current_mA = currentmA;
      g_last_12v.valid = true;
    }
  }
}

void scanBus(const I2cBusCandidate& bus) {
  uint8_t foundCount = 0;

  Serial.printf("Scanning %s on SDA=%u SCL=%u (bit-bang)...\n", bus.name, bus.sda, bus.scl);
  // Idle the bus: both lines released high.
  sdaHigh();
  sclHigh();
  delay(2);

  for (uint8_t i = 0; i < sizeof(TARGET_ADDRESSES); i++) {
    const uint8_t address = TARGET_ADDRESSES[i];
    if (inaPing(address)) {
      Serial.print("I2C device found at address 0x");
      if (address < 16) Serial.print("0");
      Serial.print(address, HEX);
      Serial.println(" !");
      printInaReadings(address);
      foundCount++;
    } else {
      Serial.print("No response at 0x");
      if (address < 16) Serial.print("0");
      Serial.println(address, HEX);
    }
  }

  if (foundCount == 3) {
    Serial.printf("PASS on %s: all INA3221 devices detected\n\n", bus.name);
  } else {
    Serial.printf("%s detection: %u/3 devices found\n\n", bus.name, foundCount);
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.printf("policy: OTA %s\n", OTA_ENABLED ? "enabled" : "disabled");

  // Initialise shift register GPIOs and enable all output load switches.
  // Without this, the 74HC595 power-on state is random and FETs may be OFF.
  pinMode(SR_DATA_PIN,  OUTPUT);
  pinMode(SR_CLK_PIN,   OUTPUT);
  pinMode(SR_LATCH_PIN, OUTPUT);
  digitalWrite(SR_DATA_PIN,  LOW);
  digitalWrite(SR_CLK_PIN,   LOW);
  digitalWrite(SR_LATCH_PIN, LOW);
  enableAllChannels();
  if (SR_SELF_TEST_ENABLED) {
    runShiftRegisterSelfTest();
  }

  // Bring up the HAT -> CrowPanel display link using UART transport.
  disp_link::begin();

  Serial.println("\nI2C scanner starting (bit-bang on GPIO5/GPIO6 for INA3221 + AHT20)...");
  printCommandHelp();
}

void loop() {
  handleSerialCommands();

  Serial.println("Scanning INA3221 target addresses...");
  for (uint8_t i = 0; i < sizeof(BUS_CANDIDATES) / sizeof(BUS_CANDIDATES[0]); i++) {
    scanBus(BUS_CANDIDATES[i]);
  }
  pollAht20();

  // Publish latest 12V input reading to the CrowPanel over the UART link.
  if (g_last_12v.valid) {
    const float v_clamped = constrain(g_last_12v.bus_v, 0.0f, 65.0f);
    const float i_clamped = constrain(g_last_12v.current_mA, -32000.0f, 32000.0f);
    const uint16_t v12_mV = static_cast<uint16_t>(v_clamped * 1000.0f + 0.5f);
    const int16_t  i12_mA = static_cast<int16_t>(i_clamped >= 0.0f
                                                     ? i_clamped + 0.5f
                                                     : i_clamped - 0.5f);
    const bool ok = disp_link::publishTelemetry(v12_mV, i12_mA);
    Serial.printf("disp_link tx %s: V12=%u mV I12=%d mA\n",
                  ok ? "OK" : "FAIL",
                  static_cast<unsigned>(v12_mV),
                  static_cast<int>(i12_mA));
  } else {
    Serial.println("disp_link tx skipped: no 12V reading captured yet");
  }

  // Run the sampling/publish loop at ~10 Hz for lower end-to-end latency.
  delay(100);
}
