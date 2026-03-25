# Development-Station-Power-Supply
3 channel bench supply  with current and voltage plotting

## Current status (Display board)

- Standard MCU for prototyping: **ESP32-C6**
- Firmware framework: **Arduino** (not ESP-IDF)
- Interconnect to host/main board: **10-pin IDC (2×5) ribbon**
  - Alternating GND pins for signal integrity over ~12–18" cable
  - Host ↔ display-board communication is **SPI only**
- Power
  - IDC-10 provides **5V_IN only**
  - Display board generates **3.3V** locally using **AMS1117-3.3**
  - Typical current expected < ~300 mA (regulator sized for margin; pay attention to heat/copper area)
- Architecture
  - Display-board ESP32-C6 is **SPI SLAVE** to the host over IDC-10 (GPIO4–GPIO7)
  - Display-board ESP32-C6 is **SPI MASTER** to local peripherals (GPIO10–GPIO22)
  - Local SPI bus is shared across TFT (ST7796S), touch (XPT2046), and SD (separate CS lines)

## Pinout (single source of truth)
See `docs/GPIO_PINOUT.md`.# Development-Station-Power-Supply
3 channel bench supply  with current and voltage plotting
