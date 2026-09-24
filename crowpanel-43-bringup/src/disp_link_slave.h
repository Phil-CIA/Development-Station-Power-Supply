#pragma once

// disp_link_slave — CrowPanel (ESP32-S3) UART receiver for HAT telemetry.
//
// See WorkStation/src/disp_link.h for the transport rationale and the
// 10-byte frame layout.
//
// Transport mode is compile-time selectable:
// - UART1 (CrowPanel UART1-OUT path, IO19/IO20)
// - UART0 (CrowPanel UART0-IN path, IO44/IO43)
//
// The recv callback runs on the WiFi task. Access to the latest Telemetry
// snapshot is guarded by a portMUX critical section.

#include <Arduino.h>

namespace disp_link_slave {

enum class TransportMode : uint8_t {
  Uart1 = 0,
  Uart0 = 1,
};

struct Telemetry {
  uint32_t rx_count;       // total frames parsed (UART)
  uint32_t err_count;
  uint8_t  last_seq;
  uint16_t last_v12_mV;
  int16_t  last_i12_mA;
  uint16_t last_v5_mV;
  int16_t  last_i5_mA;
  uint16_t last_v3v3_mV;
  int16_t  last_i3v3_mA;
  uint8_t  last_temp_C;
  uint8_t  status;
  uint8_t  protection_flags;
  bool     has_extended;
  uint32_t last_rx_ms;
  uint32_t i2c_rx_count;   // legacy field, now: frames received via active UART transport
  uint32_t uart_bytes;     // raw bytes seen on active UART transport (debug)
};

struct CommandLink {
  uint32_t tx_count;       // display -> host CMD: lines sent
  uint32_t ack_count;      // host -> display ACK: lines received
  uint32_t err_count;      // host -> display ERR: lines received
  uint32_t evt_count;      // host -> display EVT: lines received
  uint32_t last_rx_ms;     // timestamp of last ACK/ERR/EVT line
  char     last_ack[64];
  char     last_err[96];
  char     last_evt[96];
};

// Bring up telemetry receiver on the selected UART transport.
void begin();

// Drain selected UART RX and feed bytes into the SOF state machine. Call from loop().
void poll();

// Compile-time selected transport mode.
TransportMode transportMode();

// True when telemetry is bound to Serial (UART0) and shares the console stream.
bool telemetryOnConsoleSerial();

Telemetry snapshot();
CommandLink commandSnapshot();

// Send one UDI command line over UART using CMD:<payload>\n framing.
// Returns false when the UART link has not been initialized.
bool sendCommand(const char* payload);

}  // namespace disp_link_slave
