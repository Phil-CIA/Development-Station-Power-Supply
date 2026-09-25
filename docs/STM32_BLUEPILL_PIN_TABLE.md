# STM32 Rev-C Pin Contract (Authoritative)

Status: active. This is the controller-of-record pin contract for the STM32
path (`stm32-bluepill-bringup`) against current Rev-C hardware files.

## Scope and sources

- **Hardware source of truth:** `hardware/kicad/dsp-regulator-hat-rev-c/DSP-Regulator-HAT-RevC.net`
- **Firmware source of truth:** `stm32-bluepill-bringup/src/main.cpp`
- **System/docs entry point:** `docs/GPIO_PINOUT.md`

This table intentionally tracks both aligned and mismatched signals so drift
is visible and actionable.

## Canonical pin matrix

| Logical function | MCU pin | Direction | Electrical role | Owning hardware net label | Firmware symbol/constant | Status |
|---|---|---|---|---|---|---|
| Rail 5V control | PA0 | Output | GPIO | `ISET_MPU_5V` | `PIN_ISET_5V` | Implemented (aligned) |
| Rail 3V3 control | PA1 | Output | GPIO | `ISET_MPU_3V3` | `PIN_ISET_3V3` | Implemented (aligned) |
| Rail CH3 control | PA2 | Output | GPIO | `ISET_MPU_Channel_3` | `PIN_ISET_CH3` | Implemented (aligned) |
| Fault summary input | N/C (Rev-C) | Input (unavailable) | GPIO | `FAULT_CRITICAL_SUM` (not routed to STM32 GPIO in current netlist) | `PIN_FAULT_CRITICAL_SUM = -1` | Implemented (disabled for current Rev-C routing) |
| Shift-register latch | PA4 | Output | GPIO | `SR_Latch` | `PIN_SR_LATCH` | Implemented (aligned) |
| External flash CS | PA8 | Output | SPI CS (GPIO) | `Flash_CS` | `PIN_FLASH_CS` | Implemented (aligned) |
| CH340 debug TX | PA9 | Output | UART1 TX | `UART1_TX` | `SerialDbg` TX (`Uart SerialDbg(PA10, PA9)`) | Implemented (aligned) |
| CH340 debug RX | PA10 | Input | UART1 RX | `UART1_RX` | `SerialDbg` RX (`Uart SerialDbg(PA10, PA9)`) | Implemented (aligned) |
| Display-link TX | PB10 | Output | USART3 TX | `DISP_UART_TX` | `SerialU3` TX (`Uart SerialU3(PB11, PB10)`) | Implemented (aligned) |
| Display-link RX | PB11 | Input | USART3 RX | `DISP_UART_RX` | `SerialU3` RX (`Uart SerialU3(PB11, PB10)`) | Implemented (aligned) |
| Telemetry I2C clock | PB8 | Bidirectional | I2C SCL | `I²C SCL_0` | `I2C_SCL_PIN` | Implemented (aligned) |
| Telemetry I2C data | PB9 | Bidirectional | I2C SDA | `I²C SDA_0` | `I2C_SDA_PIN` | Implemented (aligned) |
| Fan gate control path | PB5 | Output (intended) | GPIO | `PB5` -> `R63` -> `Net-(Q9-G)` | (not yet defined in firmware) | Planned/board-wired |
| Fan tach feedback | (none) | Input (N/A) | Tach input | (none on J7 in current Rev-C netlist) | (none) | Deprecated/not present on current Rev-C |

## Fan contract (Issue #29)

Current Rev-C netlist contract:
- `J7` is a 2-pin **Fan Control** connector.
- `J7` pin 2 is on `+5V_Boot`.
- `J7` pin 1 is on `Net-(D11-A)` (switched path).
- No dedicated fan tach net is present on J7 in this revision.

Policy for this revision:
- Fan control is currently documented as **open-loop** (no tach feedback).
- Any tach/RPM feature work requires a hardware-net addition and a follow-up
  pin-contract update in this file before firmware work starts.

## Drift-check workflow (for PRs touching STM32 pins)

1. Update this table first when a pin/net assignment changes.
2. Verify firmware constants:
   - `rg -n "Uart Serial|PIN_|I2C_" stm32-bluepill-bringup/src/main.cpp`
3. Verify netlist labels:
   - `rg -n "\\(name \\"ISET_MPU_5V\\"\\)|\\(name \\"ISET_MPU_3V3\\"\\)|\\(name \\"ISET_MPU_Channel_3\\"\\)|\\(name \\"FAULT_CRITICAL_SUM\\"\\)|\\(name \\"UART1_TX\\"\\)|\\(name \\"UART1_RX\\"\\)|\\(name \\"DISP_UART_TX\\"\\)|\\(name \\"DISP_UART_RX\\"\\)|\\(name \\"I²C SCL_0\\"\\)|\\(name \\"I²C SDA_0\\"\\)|\\(name \\"PB5\\"\\)" hardware/kicad/dsp-regulator-hat-rev-c/DSP-Regulator-HAT-RevC.net`
4. Ensure any mismatch is explicitly marked in **Status** as either:
   - `Implemented (aligned)`,
   - `Mismatch` (with reason), or
   - `Planned/board-wired` / `Deprecated`.
5. If this table changes, update related references in:
   - `docs/GPIO_PINOUT.md`
   - `docs/FIRMWARE_DEVELOPMENT_PLAN.md` (if behavior/scope changed)
