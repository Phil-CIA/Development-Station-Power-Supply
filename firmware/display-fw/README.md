# firmware/display-fw

PlatformIO project for the **Display Board** firmware (ESP32-C6).

## Getting Started (Windows)

1. Install **VS Code** from <https://code.visualstudio.com/>.
2. Install the **PlatformIO IDE** extension (search "PlatformIO" in the Extensions panel).
3. Open VS Code, then **File → Open Folder…** → select this folder (`firmware/display-fw/`).
4. PlatformIO will initialise automatically; first build downloads the toolchain.
5. Press **`Ctrl+Alt+B`** to build, **`Ctrl+Alt+U`** to upload over USB.

## Intended Structure

```
display-fw/
├── platformio.ini       ← Build environments (smart-mode, bridge-mode, OTA)
├── src/
│   └── main.cpp         ← Application entry point
├── include/             ← Shared headers
└── lib/                 ← Local libraries (LVGL port, display driver, protocol)
```

## Key Features (planned)

- **Smart mode** – LVGL runs on this board; host sends high-level UI commands over UART.
- **Bridge mode** – Host sends pixel rectangles over SPI slave; board blits to ST7796S.
- Mode selected by GPIO strap (jumper) at boot; overridable by host command.
- OTA firmware update support via Wi-Fi.

## Related Hardware

Schematic and PCB are in `../../hardware/display-board/`.
