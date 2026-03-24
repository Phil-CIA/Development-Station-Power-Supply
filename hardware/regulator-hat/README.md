# hardware/regulator-hat

KiCad project for the **Regulator Hat Board** *(work in progress)*.

## Intended Contents

| File | Description |
|---|---|
| `regulator-hat.kicad_pro` | KiCad project file |
| `regulator-hat.kicad_sch` | Top-level schematic |
| `regulator-hat.kicad_pcb` | PCB layout |

## Board Description

A "hat" board that sits on or connects to the main bench-supply chassis and provides:

- 3-channel regulated output (voltage + current control per channel)
- **ESP32-C6** (WROOM module) for monitoring, control, and host communication
- Host-link connector to the display board (UART + optional SPI)
- I²C / SPI interfaces for ADC and DAC ICs

## Firmware

The matching PlatformIO firmware project is at `../../firmware/hat-fw/`.
