#include "disp_link_slave.h"

#include <driver/gpio.h>
#include <soc/usb_serial_jtag_reg.h>

// UART1 wiring to HAT (CrowPanel side of HY2.0-4P UART1-OUT, K1=0,1):
//   IO20 = RX (HAT TX = GPIO7)
//   IO19 = TX (HAT RX = GPIO0)  — unused for now but reserved
// Pin assignments per Elecrow CrowPanel Advance 4.3 reference (lesson-09
// zigbee_7.0): from the S3's perspective, IO19 is UART1 RX and IO20 is
// UART1 TX.  HAT TX (GPIO7) -> CrowPanel IO19 (S3 RX); HAT RX (GPIO0) <-
// CrowPanel IO20 (S3 TX).  K1 DIP must be (0,1) to engage UART1_OUT.
constexpr int      kUartRxPin = 19;
constexpr int      kUartTxPin = 20;
constexpr uint32_t kUartBaud  = 115200;

namespace disp_link_slave {

namespace {

constexpr uint8_t kSof1         = 0xAA;
constexpr uint8_t kSof2         = 0x55;
constexpr uint8_t kFrameTagTlm  = 'T';
constexpr uint8_t kFrameLenTlm  = 6;
constexpr size_t  kFrameSize    = 10;

portMUX_TYPE       s_mux   = portMUX_INITIALIZER_UNLOCKED;
volatile Telemetry s_state = {};
bool               s_begun = false;

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

void bumpErr() {
  portENTER_CRITICAL(&s_mux);
  s_state.err_count++;
  portEXIT_CRITICAL(&s_mux);
}

void parseFrame(const uint8_t* f) {
  if (f[0] != kSof1 || f[1] != kSof2) { bumpErr(); return; }
  if (f[2] != kFrameLenTlm)            { bumpErr(); return; }
  if (f[3] != kFrameTagTlm)            { bumpErr(); return; }
  if (crc8(&f[2], 7) != f[9])          { bumpErr(); return; }

  const uint8_t  seq    = f[4];
  const uint16_t v12_mV = static_cast<uint16_t>(f[5]) |
                          (static_cast<uint16_t>(f[6]) << 8);
  const int16_t  i12_mA = static_cast<int16_t>(
      static_cast<uint16_t>(f[7]) |
      (static_cast<uint16_t>(f[8]) << 8));

  portENTER_CRITICAL(&s_mux);
  s_state.i2c_rx_count++;   // counter doubles as UART success count
  s_state.rx_count++;
  s_state.last_seq    = seq;
  s_state.last_v12_mV = v12_mV;
  s_state.last_i12_mA = i12_mA;
  s_state.last_rx_ms  = millis();
  portEXIT_CRITICAL(&s_mux);
}

}  // namespace

void begin() {
  if (s_begun) return;

  // Bring up UART1 on IO20(RX)/IO19(TX). IO19/IO20 are the S3's USB-Serial-JTAG
  // D-/D+ pads at boot; release them from USB-JTAG so the UART peripheral can
  // drive them. Without this, IO19/IO20 are held by the USB-JTAG block and the
  // UART RX line reads as a floating-high constant.
  REG_CLR_BIT(USB_SERIAL_JTAG_CONF0_REG, USB_SERIAL_JTAG_USB_PAD_ENABLE);
  gpio_reset_pin(static_cast<gpio_num_t>(kUartRxPin));
  gpio_reset_pin(static_cast<gpio_num_t>(kUartTxPin));
  Serial1.begin(kUartBaud, SERIAL_8N1, kUartRxPin, kUartTxPin);

  s_begun = true;
  Serial.printf("disp_link_slave: UART1 listening @%lu baud RX=IO%d TX=IO%d (K1 must be 0,1)\n",
                static_cast<unsigned long>(kUartBaud), kUartRxPin, kUartTxPin);
}

void poll() {
  // Byte-by-byte SOF state machine on Serial1. Frames are 10 bytes; we resync
  // on any byte that doesn't match the expected SOF sequence.
  static uint8_t  buf[kFrameSize];
  static uint8_t  idx = 0;

  while (Serial1.available()) {
    const uint8_t b = static_cast<uint8_t>(Serial1.read());
    portENTER_CRITICAL(&s_mux);
    s_state.uart_bytes++;
    portEXIT_CRITICAL(&s_mux);
    if (idx == 0) {
      if (b == kSof1) { buf[0] = b; idx = 1; }
      continue;
    }
    if (idx == 1) {
      if (b == kSof2)      { buf[1] = b; idx = 2; }
      else if (b == kSof1) { buf[0] = b; idx = 1; }   // resync on stray SOF1
      else                 { idx = 0; }
      continue;
    }
    buf[idx++] = b;
    if (idx >= kFrameSize) {
      parseFrame(buf);
      idx = 0;
    }
  }
}

Telemetry snapshot() {
  Telemetry copy;
  portENTER_CRITICAL(&s_mux);
  copy = const_cast<const Telemetry&>(s_state);
  portEXIT_CRITICAL(&s_mux);
  return copy;
}

}  // namespace disp_link_slave
