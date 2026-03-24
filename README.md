# Development-Station-Power-Supply

3-channel bench supply with current and voltage plotting, a smart display board, and ESP32-based firmware.

## Repository Layout

```
Development-Station-Power-Supply/
├── hardware/
│   ├── display-board/      ← KiCad project for the 4" TFT display board
│   └── regulator-hat/      ← KiCad project for the regulator hat board (future)
├── firmware/
│   ├── display-fw/         ← PlatformIO project for the display-board ESP32-C6
│   └── hat-fw/             ← PlatformIO project for the regulator-hat ESP32-C6
├── docs/                   ← Additional documentation, datasheets, design notes
└── README.md               ← This file
```

## Opening the KiCad Projects (Windows)

> Tested with **KiCad 8.x**. Download from <https://www.kicad.org/download/>.

1. Open **KiCad** from the Start menu.
2. Click **File → Open Project…** (or press `Ctrl+O`).
3. Navigate to the project subfolder (e.g., `hardware\display-board\`).
4. Select **`Front display board.kicad_pro`** and click **Open**.
5. The KiCad project manager will load. Double-click the `.kicad_sch` entry to open the schematic, or the `.kicad_pcb` entry (once created) to open the PCB layout.

## Opening the PlatformIO Firmware Projects (Windows)

> Requires **VS Code** + the **PlatformIO IDE** extension.
> Install VS Code from <https://code.visualstudio.com/>, then install the *PlatformIO IDE* extension from the Extensions panel (`Ctrl+Shift+X`).

1. Open **VS Code**.
2. Click **File → Open Folder…** and choose one of the firmware folders:
   - `firmware\display-fw\` – firmware for the display board
   - `firmware\hat-fw\` – firmware for the regulator hat board
3. PlatformIO will detect `platformio.ini` automatically and download the required toolchain/libraries on first build.
4. Press **`Ctrl+Alt+B`** (or click the ✓ button in the bottom toolbar) to build.
5. Connect the ESP32-C6 board via USB and press **`Ctrl+Alt+U`** to upload.

## Board Overview

| Board | MCU | Role |
|---|---|---|
| Regulator Hat | ESP32-C6 | Power supply control, current/voltage monitoring |
| Display Board | ESP32-C6 | 4" TFT UI, touch input, SD card, OTA updates |

The two boards communicate over a **UART link** (smart-display mode) or an optional **SPI slave link** (bridge/pixel-streaming mode), selectable by a mode jumper on the display board.

## Contributing / Workflow

- All hardware lives under `hardware/` as KiCad projects (one subfolder per board).
- All firmware lives under `firmware/` as PlatformIO projects (one subfolder per board).
- Schematic symbols and footprints that are shared between boards go in `hardware/library/` (create when needed).
- Design notes and datasheets go in `docs/`.
