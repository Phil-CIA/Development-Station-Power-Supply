#include "disp_link_slave.h"

#include <driver/gpio.h>
#include <soc/soc.h>
#include <soc/usb_serial_jtag_reg.h>

constexpr uint32_t kUartBaud  = 115200;

namespace disp_link_slave {

namespace {

constexpr uint8_t kSof1         = 0xAA;
constexpr uint8_t kSof2         = 0x55;
constexpr uint8_t kFrameTagTlm  = 'T';
constexpr int kUartRxPin = 19;
constexpr int kUartTxPin = 20;
constexpr uint8_t kFrameLenTlm  = 6;
constexpr uint8_t kFrameLenExtMin = 13;
constexpr size_t  kFrameSizeLegacy = 10;
constexpr size_t  kFrameSizeExtMin = 17;
constexpr size_t  kFrameSizeMax    = 40;

portMUX_TYPE       s_mux   = portMUX_INITIALIZER_UNLOCKED;
volatile Telemetry s_state = {};
CommandLink        s_cmd   = {};
bool               s_begun = false;
Stream*            s_rx_stream = nullptr;
Print*             s_tx_stream = nullptr;

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

void copyBounded(char* dst, size_t dst_len, const char* src) {
  if (dst_len == 0) return;
  if (src == nullptr) {
    dst[0] = '\0';
    return;
  }
  size_t i = 0;
  for (; src[i] != '\0' && i + 1 < dst_len; ++i) {
    dst[i] = src[i];
  }
  dst[i] = '\0';
}

void consumeControlLine(const char* line) {
  if (line == nullptr || line[0] == '\0') return;

  const uint32_t now_ms = millis();
  if (strncmp(line, "ACK:", 4) == 0) {
    portENTER_CRITICAL(&s_mux);
    s_cmd.ack_count++;
    s_cmd.last_rx_ms = now_ms;
    copyBounded(s_cmd.last_ack, sizeof(s_cmd.last_ack), line + 4);
    portEXIT_CRITICAL(&s_mux);
    return;
  }
  if (strncmp(line, "ERR:", 4) == 0) {
    portENTER_CRITICAL(&s_mux);
    s_cmd.err_count++;
    s_cmd.last_rx_ms = now_ms;
    copyBounded(s_cmd.last_err, sizeof(s_cmd.last_err), line + 4);
    portEXIT_CRITICAL(&s_mux);
    return;
  }
  if (strncmp(line, "EVT:", 4) == 0) {
    portENTER_CRITICAL(&s_mux);
    s_cmd.evt_count++;
    s_cmd.last_rx_ms = now_ms;
    copyBounded(s_cmd.last_evt, sizeof(s_cmd.last_evt), line + 4);
    portEXIT_CRITICAL(&s_mux);
    return;
  }
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

  // Release IO19/20 from USB-Serial-JTAG pad ownership so Serial1 can bind
  // to the dedicated CrowPanel UART1-OUT path.
  REG_CLR_BIT(USB_SERIAL_JTAG_CONF0_REG, BIT(14));
  gpio_reset_pin(static_cast<gpio_num_t>(kUartRxPin));
  gpio_reset_pin(static_cast<gpio_num_t>(kUartTxPin));
  Serial1.begin(kUartBaud, SERIAL_8N1, kUartRxPin, kUartTxPin);

  s_rx_stream = &Serial1;
  s_tx_stream = &Serial1;
  Serial.printf("disp_link_slave: UART1 listening @%lu baud on IO%d/IO%d\n",
                static_cast<unsigned long>(kUartBaud),
                kUartRxPin,
                kUartTxPin);

  s_begun = true;
}

void poll() {
  // Byte-by-byte SOF state machine on the selected UART stream. Supports variable-length
  // telemetry frames while preserving legacy 10-byte compatibility.
  static uint8_t  buf[kFrameSizeMax];
  static size_t   idx = 0;
  static size_t   expected_total = 0;
  static char     ctrl_line[128];
  static size_t   ctrl_len = 0;

  if (!s_begun || s_rx_stream == nullptr) return;

  while (s_rx_stream->available() > 0) {
    const uint8_t b = static_cast<uint8_t>(s_rx_stream->read());
    portENTER_CRITICAL(&s_mux);
    s_state.uart_bytes++;
    portEXIT_CRITICAL(&s_mux);

    if (b == '\r') {
      // Ignore CR in text control lines.
    } else if (b == '\n') {
      if (ctrl_len > 0) {
        ctrl_line[ctrl_len] = '\0';
        consumeControlLine(ctrl_line);
      }
      ctrl_len = 0;
    } else if (b >= 0x20 && b <= 0x7E) {
      if (ctrl_len + 1 < sizeof(ctrl_line)) {
        ctrl_line[ctrl_len++] = static_cast<char>(b);
      } else {
        ctrl_len = 0;
      }
    } else {
      // Binary telemetry bytes are expected; reset text parser on non-printable bytes.
      ctrl_len = 0;
    }

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
  return TransportMode::Uart1;
}

bool telemetryOnConsoleSerial() {
  return false;
}

Telemetry snapshot() {
  Telemetry copy;
  portENTER_CRITICAL(&s_mux);
  copy = const_cast<const Telemetry&>(s_state);
  portEXIT_CRITICAL(&s_mux);
  return copy;
}

CommandLink commandSnapshot() {
  CommandLink copy;
  portENTER_CRITICAL(&s_mux);
  copy = s_cmd;
  portEXIT_CRITICAL(&s_mux);
  return copy;
}

bool sendCommand(const char* payload) {
  if (!s_begun || s_tx_stream == nullptr || payload == nullptr || payload[0] == '\0') {
    return false;
  }
  s_tx_stream->print("CMD:");
  s_tx_stream->print(payload);
  s_tx_stream->print('\n');

  portENTER_CRITICAL(&s_mux);
  s_cmd.tx_count++;
  portEXIT_CRITICAL(&s_mux);
  return true;
}

}  // namespace disp_link_slave
