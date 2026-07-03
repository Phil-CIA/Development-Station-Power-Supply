#include <Arduino.h>

HardwareSerial SerialU3(PB11, PB10); // RX, TX (USART3)

// Project bring-up signals from docs/STM32_BLUEPILL_PIN_TABLE.md (Draft A)
static const uint8_t PIN_ISET_5V = PA0;
static const uint8_t PIN_ISET_3V3 = PA1;
static const uint8_t PIN_ISET_CH3 = PA2;
static const uint8_t PIN_STATUS_LED = PC13; // Blue Pill onboard LED (active-low on most boards)

// ── Telemetry frame constants ──────────────────────────────────────────────
static const uint8_t FRAME_SOF1 = 0xAA;
static const uint8_t FRAME_SOF2 = 0x55;
static const uint8_t FRAME_TAG = 'T';
static const uint8_t FRAME_LEN = 13;
static const size_t FRAME_SIZE = 17;
static uint8_t frame_seq = 0;

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

void setup() {
  // Keep control outputs inactive as early as possible.
  pinMode(PIN_ISET_5V, OUTPUT);
  pinMode(PIN_ISET_3V3, OUTPUT);
  pinMode(PIN_ISET_CH3, OUTPUT);
  digitalWrite(PIN_ISET_5V, LOW);
  digitalWrite(PIN_ISET_3V3, LOW);
  digitalWrite(PIN_ISET_CH3, LOW);

  pinMode(PIN_STATUS_LED, OUTPUT);
  digitalWrite(PIN_STATUS_LED, HIGH); // LED off (active-low)

  Serial.begin(115200);
  delay(150);
  Serial.println("stm32-bluepill bringup: boot");

  // USART3 on PB10/PB11 for future HAT->CrowPanel link validation.
  SerialU3.begin(115200);
  SerialU3.println("stm32-bluepill usart3: ready");
}

void loop() {
  static uint32_t lastMs = 0;
  uint32_t now = millis();

  if (now - lastMs >= 1000) {
    lastMs = now;
    digitalWrite(PIN_STATUS_LED, LOW);
    delay(40);
    digitalWrite(PIN_STATUS_LED, HIGH);

    Serial.println("hb");
    
    // Extended placeholder telemetry for Phase 5 parser/UI bring-up.
    const uint16_t v5_mV = 5000;
    const int16_t i5_mA = 500;
    const uint16_t v3v3_mV = 3300;
    const int16_t i3v3_mA = 320;
    const uint8_t temp_C = 31;
    const uint8_t status = 0xF0;           // CH1_EN CH2_EN CH1_CV CH2_CV
    const uint8_t protection_flags = 0x00; // no active faults
    publishTelemetry(v5_mV, i5_mA, v3v3_mV, i3v3_mA, temp_C, status, protection_flags);
  }
}
