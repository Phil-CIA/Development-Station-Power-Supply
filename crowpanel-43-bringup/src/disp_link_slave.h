#pragma once

// disp_link_slave — CrowPanel (ESP32-S3) UART receiver for HAT telemetry.
//
// See WorkStation/src/disp_link.h for the transport rationale and the
// 10-byte frame layout. The transport is UART1 only.
//
// The recv callback runs on the WiFi task. Access to the latest Telemetry
// snapshot is guarded by a portMUX critical section.

#include <Arduino.h>

namespace disp_link_slave {

struct Telemetry {
  uint32_t rx_count;       // total frames parsed (UART)
  uint32_t err_count;
  uint8_t  last_seq;
  uint16_t last_v12_mV;
  int16_t  last_i12_mA;
  uint32_t last_rx_ms;
  uint32_t i2c_rx_count;   // legacy field, now: frames received via UART1
  uint32_t uart_bytes;     // raw bytes seen on Serial1 (debug)
};

// Bring up UART1 receiver.
void begin();

// Drain Serial1 RX and feed bytes into the SOF state machine. Call from loop().
void poll();

Telemetry snapshot();

}  // namespace disp_link_slave
