#include "disp_link.h"

namespace disp_link {

namespace {

constexpr bool kOtaEnabled = false;

uint8_t s_seq    = 0;
bool    s_begun  = false;

uint8_t crc8(const uint8_t* data, size_t len) {
  uint8_t crc = 0x00;
  for (size_t i = 0; i < len; ++i) {
    crc ^= data[i];
    for (int b = 0; b < 8; ++b) {
      crc = (crc & 0x80) ? static_cast<uint8_t>((crc << 1) ^ 0x07)
                         : static_cast<uint8_t>(crc << 1);
    }
  }
  return crc;
}

}  // namespace

void begin() {
  if (s_begun) return;

  Serial.printf("disp_link: ota policy=%s\n", kOtaEnabled ? "enabled" : "disabled");

  // UART1 on the C6's UART1 peripheral, RX=GPIO0, TX=GPIO7. Wired to the
  // CrowPanel UART1-OUT HY2.0-4P connector through the CH486F mux (K1=0,1).
  Serial1.begin(kUartBaud, SERIAL_8N1, kUartRxPin, kUartTxPin);
  Serial.printf("disp_link: UART1 up @%lu baud RX=GPIO%d TX=GPIO%d\n",
                static_cast<unsigned long>(kUartBaud), kUartRxPin, kUartTxPin);

  s_begun = true;
}

bool publishTelemetry(uint16_t v12_mV, int16_t i12_mA) {
  if (!s_begun) return false;

  uint8_t frame[kFrameSizeTlm];
  frame[0] = kFrameSof1;
  frame[1] = kFrameSof2;
  frame[2] = kFrameLenTlm;
  frame[3] = kFrameTagTlm;
  frame[4] = s_seq++;
  frame[5] = static_cast<uint8_t>(v12_mV & 0xFF);
  frame[6] = static_cast<uint8_t>((v12_mV >> 8) & 0xFF);
  const uint16_t i_u = static_cast<uint16_t>(i12_mA);
  frame[7] = static_cast<uint8_t>(i_u & 0xFF);
  frame[8] = static_cast<uint8_t>((i_u >> 8) & 0xFF);
  frame[9] = crc8(&frame[2], 7);

  // Primary wired path: push the frame down UART1 (via CrowPanel CH486F mux
  // to S3 IO19/IO20; see repo memory entry "UART HAT↔CrowPanel — SOLVED").
  const size_t written = Serial1.write(frame, kFrameSizeTlm);
  return written == kFrameSizeTlm;
}

}  // namespace disp_link
