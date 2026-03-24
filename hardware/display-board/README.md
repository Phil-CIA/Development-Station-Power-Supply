# hardware/display-board

KiCad project for the **Front Display Board**.

## Contents

| File | Description |
|---|---|
| `Front display board.kicad_pro` | KiCad project file – open this in KiCad |
| `Front display board.kicad_sch` | Top-level schematic |
| `Front display board.kicad_pcb` | PCB layout (created when layout work begins) |

## Board Description

A 4.0″ 480 × 320 SPI TFT display board based on the **ST7796S** controller with:

- **XPT2046** resistive touch controller (SPI)
- **SD card** slot (SPI)
- **ESP32-C6** (WROOM module) for local UI processing and OTA updates
- Single host-link connector carrying UART + optional SPI for smart/bridge mode selection

## Opening in KiCad (Windows)

1. Launch KiCad.
2. **File → Open Project…** → navigate here → select `Front display board.kicad_pro`.

## Firmware

The matching PlatformIO firmware project is at `../../firmware/display-fw/`.
