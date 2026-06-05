#pragma once

// disp_link — HAT (ESP32-C6) → CrowPanel 4.3" display link.
//
// Transport: UART1 (wired).
//
// UART wiring (HAT J8 ↔ CrowPanel UART1-OUT HY2.0-4P):
//   HAT GPIO7 (TX)  →  CrowPanel IO19 (RX, IO19_RX_IRQ)
//   HAT GPIO0 (RX)  ←  CrowPanel IO20 (TX, IO20_TX_CE)
//   GND common.
//
// CrowPanel UART1-OUT is routed through a CH486F analog mux (U9) controlled
// by the K1 DIP switch. Switches MUST be set to S1=0, S0=1 to select UART1
// mode (default 0,0 = MIC&SPK and IO19/IO20 do not reach the connector).
// Confirmed working only with K1 in (0,1).
//
// HAT acts as a plain UART transmitter: pushes the 10-byte frame on every
// publishTelemetry() call. CrowPanel runs a byte-by-byte SOF state machine.
// INA3221 sensors are read via a small bit-bang I²C master on GPIO5/6 in
// i2c_scanner.cpp; the HP_I2C0 hardware peripheral is currently unused.
//
// Earlier I²C-slave attempts (Wire1 wrapper and raw IDF `i2c_new_slave_device`)
// were abandoned: the C6 slave peripheral never ACKed its own address.
// I2C-OUT (IO15/IO16) on the CrowPanel has a direct 0Ω trace and remains
// reserved for the planned Rev 2 STM32 HAT.
//
// Frame on the wire (10 bytes, little-endian payload):
//   [0] 0xAA  SOF1
//   [1] 0x55  SOF2
//   [2] 0x06  LEN  (TAG + seq + v(2) + i(2))
//   [3] 'T'   tag
//   [4] seq   u8, increments per publishTelemetry() call
//   [5..6]    v12_mV  uint16
//   [7..8]    i12_mA  int16
//   [9]       crc8    poly 0x07, init 0x00, over bytes [2..8]

#include <Arduino.h>

namespace disp_link {

constexpr int      kUartTxPin    = 7;       // HAT TX  → CrowPanel IO19 (RX)
constexpr int      kUartRxPin    = 0;       // HAT RX  ← CrowPanel IO20 (TX)
constexpr uint32_t kUartBaud     = 115200;

constexpr uint8_t  kFrameSof1    = 0xAA;
constexpr uint8_t  kFrameSof2    = 0x55;
constexpr uint8_t  kFrameTagTlm  = 'T';
constexpr uint8_t  kFrameLenTlm  = 6;
constexpr size_t   kFrameSizeTlm = 10;

// Initialise UART transport. Safe to call once.
void begin();

// Build a telemetry frame and send it via UART1.
bool publishTelemetry(uint16_t v12_mV, int16_t i12_mA);

}  // namespace disp_link
