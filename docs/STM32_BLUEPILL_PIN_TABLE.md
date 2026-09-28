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
| Fault summary input | PB7 (via AW9523 INT) | Input | GPIO interrupt | `FAULT_CRITICAL_SUM` -> AW9523 input -> `AW9523_INT` | `PIN_AW9523_INT = PB7`, `PIN_FAULT_CRITICAL_SUM = -1` | Implemented (interrupt-driven via AW9523, no direct STM32 FAULT_SUM net) |
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

## AW9523 (U5) P0.x sub-pin mapping (Rev-C, via I2C)

These are not direct STM32 pins - they sit behind the AW9523 I2C GPIO
expander (`AW95XX_ADDR_ACTIVE`, accessed over `I2C_SCL_PIN`/`I2C_SDA_PIN`
above). Confirmed against the Rev-C schematic (U5 AW9523B + Q9/Q12 gate-drive
sheet) on 2026-09-28.

| AW9523 pin | Net label | Gate path | Controllable from firmware? | Notes |
|---|---|---|---|---|
| P0.0 | `ISET_MPU_5V` | Q3 gate | Yes - `setQ3OnlyEnabled()` | CH1 current-range select, not a full output on/off |
| P0.1 | `ESP- GPIO 5V Hi` | Q2/Q8 gate | Yes - `setQ2PathEnabled()` | Was previously mislabeled "Q6/Q12 path" in code comments - corrected; not related to Q12 |
| P0.2 | `ESP- GPIO 5V Low` | Q1/Q7 gate | Yes - `setQ1PathEnabled()` | |
| P0.3 | `ESP- GPIO 3V3 High` | Q5/Q11 gate | Yes - `setQ5PathEnabled()` | Net comment previously said "Channel 3 Hi-Range" - Channel 3 was repurposed to the fixed 5V bootstrap supply in Rev B and is no longer an adjustable output, but this AW9523 net/gate path itself is unchanged |
| P0.4 | `ESP- GPIO 3V3 Low` | Q4/Q10 gate | Yes - `setQ4PathEnabled()` | |
| P0.5 | `ISET_MPU_3V3` | Q9 gate | Yes - `setQ9OnlyEnabled()` | CH2 current-range select, not a full output on/off |
| P0.6 | `FAULT_WARNING_SUM` | — (input only) | No (input) | |
| P0.7 | `FAULT_CRITICAL_SUM` | Q12 gate (via D14/R53, pulled up through R50 to `+5V_Boot`) | **No** - Q12 is a hardware fault cutoff, not firmware-commandable | Same net is read as an AW9523 input on P0.7 *and* drives Q12's gate directly in hardware; Q12 turns off automatically when `FAULT_CRITICAL_SUM` trips and is not independently switchable from the AW9523 or STM32 |

**Open item:** none of P0.0-P0.7 is a full independent CH2 output enable/disable
switch - `setD9PathEnabled()` (the only thing the UDI `OUTPUT ON`/`OUTPUT OFF`
command drives) is the single combined output-enable path, and its AW9523
branch writes P1.0, distinct from all of the P0.x nets above. If/when CH2
needs its own independent output toggle (separate from CH1), the actual EN
pin for U4 needs to be identified from the schematic before any firmware
command is added - do not assume Q9/P0.5 is that pin, since it is a
current-range select line, not an output enable.

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
