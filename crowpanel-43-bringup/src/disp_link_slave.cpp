#include "disp_link_slave.h"

#include <driver/gpio.h>
#include <soc/usb_serial_jtag_reg.h>

// Compile-time transport switch:
//   0 = UART1 (IO19/IO20, requires K1 switch path)
//   1 = UART0 (IO44/IO43 on UART0-IN)
#ifndef DISP_LINK_SLAVE_USE_UART0
#define DISP_LINK_SLAVE_USE_UART0 1
#endif

// UART1 wiring to HAT (CrowPanel side of HY2.0-4P UART1-OUT, K1=0,1):
// Live probe result on this session:
//   IO19 = RX, IO20 = TX.
// The earlier reversed mapping was not receiving frames.
constexpr int      kUartRxPin = 19;
constexpr int      kUartTxPin = 20;
constexpr uint32_t kUartBaud  = 115200;

namespace disp_link_slave {

namespace {

constexpr uint8_t kSof1         = 0xAA;
constexpr uint8_t kSof2         = 0x55;
constexpr uint8_t kFrameTagTlm  = 'T';
constexpr uint8_t kFrameLenTlm  = 6;
constexpr uint8_t kFrameLenExtMin = 13;
constexpr size_t  kFrameSizeLegacy = 10;
constexpr size_t  kFrameSizeExtMin = 17;
constexpr size_t  kFrameSizeMax    = 40;

portMUX_TYPE       s_mux   = portMUX_INITIALIZER_UNLOCKED;
volatile Telemetry s_state = {};
bool               s_begun = false;
Stream*            s_rx_stream = nullptr;

constexpr TransportMode kTransportMode =
#if DISP_LINK_SLAVE_USE_UART0
  TransportMode::Uart0;
#else
  TransportMode::Uart1;
#endif

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

void parseFrame(const uint8_t* f, size_t frame_size) {
  if (frame_size < kFrameSizeLegacy) { bumpErr(); return; }
  if (f[0] != kSof1 || f[1] != kSof2) { bumpErr(); return; }

  const uint8_t len = f[2];
  if (frame_size != static_cast<size_t>(len) + 4) { bumpErr(); return; }
  if (f[3] != kFrameTagTlm) { bumpErr(); return; }
  if (crc8(&f[2], static_cast<size_t>(len) + 1) != f[frame_size - 1]) { bumpErr(); return; }

  const uint8_t seq = f[4];
  const uint16_t v5_mV = static_cast<uint16_t>(f[5]) |
                         (static_cast<uint16_t>(f[6]) << 8);
  const int16_t i5_mA = static_cast<int16_t>(
      static_cast<uint16_t>(f[7]) |
      (static_cast<uint16_t>(f[8]) << 8));

  portENTER_CRITICAL(&s_mux);
  s_state.i2c_rx_count++;   // counter doubles as UART success count
  s_state.rx_count++;
  s_state.last_seq = seq;
  s_state.last_v5_mV = v5_mV;
  s_state.last_i5_mA = i5_mA;
  s_state.last_v12_mV = v5_mV;
  s_state.last_i12_mA = i5_mA;
  s_state.has_extended = false;

  if (len >= kFrameLenExtMin && frame_size >= kFrameSizeExtMin) {
    const uint16_t v3v3_mV = static_cast<uint16_t>(f[9]) |
                             (static_cast<uint16_t>(f[10]) << 8);
    const int16_t i3v3_mA = static_cast<int16_t>(
        static_cast<uint16_t>(f[11]) |
        (static_cast<uint16_t>(f[12]) << 8));
    s_state.last_v3v3_mV = v3v3_mV;
    s_state.last_i3v3_mA = i3v3_mA;
    s_state.last_temp_C = f[13];
    s_state.status = f[14];
    s_state.protection_flags = f[15];
    s_state.has_extended = true;
  }

  s_state.last_rx_ms = millis();
  portEXIT_CRITICAL(&s_mux);
}

}  // namespace

void begin() {
  if (s_begun) return;

  if (kTransportMode == TransportMode::Uart1) {
    // Bring up UART1 on IO19(RX)/IO20(TX). IO19/IO20 are the S3's USB-Serial-JTAG
    // D-/D+ pads at boot; release them from USB-JTAG so the UART peripheral can
    // drive them. Without this, IO19/IO20 are held by the USB-JTAG block and the
    // UART RX line reads as a floating-high constant.
    REG_CLR_BIT(USB_SERIAL_JTAG_CONF0_REG, USB_SERIAL_JTAG_USB_PAD_ENABLE);
    gpio_reset_pin(static_cast<gpio_num_t>(kUartRxPin));
    gpio_reset_pin(static_cast<gpio_num_t>(kUartTxPin));
    Serial1.begin(kUartBaud, SERIAL_8N1, kUartRxPin, kUartTxPin);
    s_rx_stream = &Serial1;
    Serial.printf("disp_link_slave: UART1 listening @%lu baud RX=IO%d TX=IO%d (K1 must be 0,1)\n",
                  static_cast<unsigned long>(kUartBaud), kUartRxPin, kUartTxPin);
  } else {
    // UART0 mode listens on the shared Serial stream (UART0-IN path, IO44/IO43).
    // Keep console TX quiet in this mode to avoid mixing diagnostics with host traffic.
    s_rx_stream = &Serial;
    Serial.printf("disp_link_slave: UART0 listening @%lu baud on Serial (UART0-IN path)\n",
                  static_cast<unsigned long>(kUartBaud));
  }

  s_begun = true;
}

void poll() {
  // Byte-by-byte SOF state machine on Serial1. Supports variable-length
  // telemetry frames while preserving legacy 10-byte compatibility.
  static uint8_t  buf[kFrameSizeMax];
  static size_t   idx = 0;
  static size_t   expected_total = 0;

  if (!s_begun || s_rx_stream == nullptr) return;

  while (s_rx_stream->available() > 0) {
    const uint8_t b = static_cast<uint8_t>(s_rx_stream->read());
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
    if (idx == 2) {
      const uint8_t len = b;
      expected_total = static_cast<size_t>(len) + 4;
      if (expected_total < kFrameSizeLegacy || expected_total > kFrameSizeMax) {
        bumpErr();
        idx = 0;
        expected_total = 0;
        continue;
      }
      buf[idx++] = b;
      continue;
    }

    if (idx >= kFrameSizeMax) {
      bumpErr();
      idx = 0;
      expected_total = 0;
      continue;
    }

    buf[idx++] = b;
    if (expected_total > 0 && idx >= expected_total) {
      parseFrame(buf, expected_total);
      idx = 0;
      expected_total = 0;
    }
  }
}

TransportMode transportMode() {
  return kTransportMode;
}

bool telemetryOnConsoleSerial() {
  return kTransportMode == TransportMode::Uart0;
}

Telemetry snapshot() {
  Telemetry copy;
  portENTER_CRITICAL(&s_mux);
  copy = const_cast<const Telemetry&>(s_state);
  portEXIT_CRITICAL(&s_mux);
  return copy;
}

}  // namespace disp_link_slave
