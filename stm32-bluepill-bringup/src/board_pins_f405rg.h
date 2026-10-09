#pragma once

#include <Arduino.h>

// Rev-D assignments from docs/HAT_REVD_PIN_AND_SUPPORT_CONTRACT.md.
static constexpr uint8_t PIN_DEBUG_RX = PB7;
static constexpr uint8_t PIN_DEBUG_TX = PB6;
static constexpr uint8_t PIN_UDI_RX = PB11;
static constexpr uint8_t PIN_UDI_TX = PB10;
static constexpr uint8_t PIN_C3_UART_RX = PC11;
static constexpr uint8_t PIN_C3_UART_TX = PC10;
static constexpr uint8_t PIN_C3_EN_N = PC0;
static constexpr uint8_t PIN_C3_BOOT_N = PC1;
static constexpr uint8_t PIN_USB_VBUS_SENSE = PA9;
static constexpr uint8_t PIN_USB_DM = PA11;
static constexpr uint8_t PIN_USB_DP = PA12;

static constexpr int8_t PIN_ISET_5V = PA0;
static constexpr int8_t PIN_ISET_3V3 = PA1;
static constexpr int8_t PIN_ISET_CH3 = -1;
static constexpr int8_t PIN_FAULT_CRITICAL_SUM = -1;
static constexpr uint8_t PIN_AW9523_INT = PC4;
static constexpr uint8_t PIN_STATUS_LED = PC13;
static constexpr uint8_t PIN_FLASH_CS = PA8;
static constexpr uint8_t PIN_SR_LATCH = PA4;
static constexpr uint8_t I2C_SCL_PIN = PB8;
static constexpr uint8_t I2C_SDA_PIN = PB9;
