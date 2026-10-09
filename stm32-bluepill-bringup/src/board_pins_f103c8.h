#pragma once

#include <Arduino.h>

// Rev-C F103 assignments from docs/STM32_BLUEPILL_PIN_TABLE.md.
static constexpr uint8_t PIN_DEBUG_RX = PA10;
static constexpr uint8_t PIN_DEBUG_TX = PA9;
static constexpr uint8_t PIN_UDI_RX = PB11;
static constexpr uint8_t PIN_UDI_TX = PB10;

static constexpr int8_t PIN_ISET_5V = PA0;
static constexpr int8_t PIN_ISET_3V3 = PA1;
// Rev-C has no CH3 MCU control; PA2/PA3 are the C3 link pair.
static constexpr int8_t PIN_ISET_CH3 = -1;
static constexpr int8_t PIN_FAULT_CRITICAL_SUM = -1;
static constexpr uint8_t PIN_AW9523_INT = PB7;
static constexpr uint8_t PIN_STATUS_LED = PC13;
static constexpr uint8_t PIN_FLASH_CS = PA8;
static constexpr uint8_t PIN_SR_LATCH = PA4;
static constexpr uint8_t I2C_SCL_PIN = PB8;
static constexpr uint8_t I2C_SDA_PIN = PB9;
