# firmware/hat-fw

PlatformIO project for the **Regulator Hat Board** firmware (ESP32-C6).

## Getting Started (Windows)

1. Install **VS Code** from <https://code.visualstudio.com/>.
2. Install the **PlatformIO IDE** extension (search "PlatformIO" in the Extensions panel).
3. Open VS Code, then **File → Open Folder…** → select this folder (`firmware/hat-fw/`).
4. PlatformIO will initialise automatically; first build downloads the toolchain.
5. Press **`Ctrl+Alt+B`** to build, **`Ctrl+Alt+U`** to upload over USB.

## Intended Structure

```
hat-fw/
├── platformio.ini       ← Build environments (debug, release, OTA)
├── src/
│   └── main.cpp         ← Application entry point
├── include/             ← Shared headers
└── lib/                 ← Local libraries (ADC/DAC drivers, comms protocol)
```

## Key Features (planned)

- Per-channel voltage and current monitoring via ADC.
- Per-channel output control via DAC.
- UART host link to the display board (send UI state, receive touch events).
- Optional SPI master link to display board for bridge/pixel-streaming mode.
- OTA firmware update support via Wi-Fi.

## Related Hardware

Schematic and PCB are in `../../hardware/regulator-hat/`.
