#pragma once

// disp_link_slave — CrowPanel (ESP32-S3) ESP-NOW receiver for HAT telemetry.
//
// See WorkStation/src/disp_link.h for the transport rationale and the
// 10-byte frame layout. Both ends use WiFi STA + ESP-NOW broadcast on a
// fixed channel; no association, no MAC pinning.
//
// The recv callback runs on the WiFi task. Access to the latest Telemetry
// snapshot is guarded by a portMUX critical section.

#include <Arduino.h>

namespace disp_link_slave {

constexpr uint8_t kEspNowChannel = 1;  // must match HAT disp_link.h

struct Telemetry {
  uint32_t rx_count;       // total frames parsed (UART + ESP-NOW)
  uint32_t err_count;
  uint8_t  last_seq;
  uint16_t last_v12_mV;
  int16_t  last_i12_mA;
  uint32_t last_rx_ms;
  uint32_t i2c_rx_count;   // legacy field, now: frames received via UART1
  uint32_t uart_bytes;     // raw bytes seen on Serial1 (debug)
};

// Bring up WiFi STA + ESP-NOW and register the recv callback.
void begin();

// Drain Serial1 RX and feed bytes into the SOF state machine. Call from loop().
void poll();

Telemetry snapshot();

}  // namespace disp_link_slave
