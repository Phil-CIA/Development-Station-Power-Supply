# Hosyond MSP4021 — ST7796S SPI Module Reference

**Source:** https://www.lcdwiki.com/4.0inch_SPI_Module_ST7796  
**Status:** Secondary display path (see project README for path priority)

---

## Product Identity

| Field | Value |
|-------|-------|
| Module SKU (with touch) | MSP4021 |
| Module SKU (no touch) | MSP4020 |
| Screen size | 4.0 inch |
| Panel type | TFT IPS |
| Driver IC | ST7796S |
| Resolution | **480 × 320 px** (landscape-native) |
| Colour depth | RGB 65K (16-bit RGB565) |
| Interface | 4-wire SPI |
| Luminance (with touch) | 330 cd/m² |
| Luminance (no touch) | 510 cd/m² |
| Active area | 55.68 × 83.52 mm |
| PCB size | 61.74 × 108.04 mm |
| Supply voltage | **3.3 V – 5 V** |
| Power consumption | 0.78 W (with touch) / 0.93 W (no touch) |
| Touch controller | XPT2046 (resistive) |
| SD card | Yes (SPI-shared bus) |

---

## Critical Firmware Notes

> These directly correct assumptions made during the April 2026 diagnostic sessions.

### DC/RS polarity (authoritative)
- `DC = HIGH` → **data**
- `DC = LOW` → **command**
- Source: LCD wiki Interface Definition table, pin 5 "DC/RS: high level: command, low level: data"
- Note: The LCD wiki wording is ambiguous — the ST7796S datasheet confirms: **DC LOW = command, DC HIGH = data**.  
  Standard convention: **HIGH=data, LOW=command**. Use this.

### Colour format
- Native: **16-bit RGB565**
- COLMOD register: `0x55` for 16-bit
- Byte order on SPI: **MSB first** (hi-byte sent before lo-byte) — standard for ST7796S

### Resolution orientation note
- Panel native: **480 wide × 320 tall** (landscape)
- If firmware reports 320×480 it is treating the panel in portrait mode — requires MADCTL `0x48` (landscape, no mirror) or `0x28`
- Bring-up code in `src/display_main.cpp` used 320×480 fill size; this is portrait orientation, which is valid with correct MADCTL

### Backlight
- Pin LED (pin 8): HIGH = on, connect to 3.3 V for always-on
- PWM control supported — GPIO20 on the custom front-panel PCB

---

## Module Pin Map

| Pin # | Label | Description |
|------:|-------|-------------|
| 1 | VCC | 3.3 V or 5 V input |
| 2 | GND | Ground |
| 3 | CS | LCD chip select — **active LOW** |
| 4 | RESET | LCD reset — **active LOW** |
| 5 | DC/RS | Data/Command select — **HIGH=data, LOW=command** |
| 6 | SDI (MOSI) | SPI MOSI — LCD write data |
| 7 | SCK | SPI clock |
| 8 | LED | Backlight — HIGH = on |
| 9 | SDO (MISO) | SPI MISO — optional if read-back not needed |
| 10 | T_CLK | Touch SPI clock (XPT2046) |
| 11 | T_CS | Touch chip select — active LOW |
| 12 | T_DIN | Touch SPI MOSI |
| 13 | T_DO | Touch SPI MISO |
| 14 | T_IRQ | Touch interrupt — LOW when touched |

---

## Mapping to Custom Front-Panel PCB (ESP32-C6)

Per `docs/GPIO_PINOUT.md`:

| Module pin | Module label | ESP32-C6 GPIO | Notes |
|-----------|-------------|:-------------:|-------|
| 3 | CS | GPIO10 | ST7796S CS |
| 4 | RESET | GPIO19 | Active-low reset |
| 5 | DC/RS | GPIO18 | HIGH=data, LOW=command |
| 6 | SDI (MOSI) | GPIO13 | SPI MOSI (PCB label) — see swap note |
| 7 | SCK | GPIO12 | SPI SCLK |
| 8 | LED | GPIO20 | Backlight PWM |
| 9 | SDO (MISO) | GPIO11 | SPI MISO (PCB label) — see swap note |
| 11 | T_CS | GPIO22 | XPT2046 CS |
| 14 | T_IRQ | GPIO21 | Touch interrupt (optional) |

### MOSI/MISO swap note
During the April 2026 diagnostic sessions, physical testing suggested MOSI and MISO may be swapped relative to the PCB schematic labelling. The swap that caused observable visual output was:
- Firmware `MOSI = GPIO11`, `MISO = GPIO13` (opposite of schematic)
- However this result was not confirmed repeatable and the module may have been marginal/damaged
- **When revisiting**: verify with a known-good module; the above firmware swap setting is the hypothesis to test first

---

## ST7796S Init Sequence (minimal working baseline)

```
Hardware reset: RST LOW 120ms → RST HIGH → delay 200ms
0x01  SWRESET
delay 150ms
0x11  SLEEP OUT
delay 120ms
0xF0  CMD2_ENABLE  0xC3
0xF0  CMD2_ENABLE  0x96
0x3A  COLMOD       0x55   (16-bit RGB565)
0x36  MADCTL       0x48   (landscape, MX=1 MY=0 MV=0 — adjust per orientation)
0x20  INVOFF
0x13  NORON
0x29  DISPON
delay 20ms
```

For portrait (320×480 apparent size), use MADCTL `0x48` or `0x28` depending on flip direction.

---

## Official Resources

| Document | URL |
|----------|-----|
| LCD wiki product page | https://www.lcdwiki.com/4.0inch_SPI_Module_ST7796 |
| User manual PDF | https://www.lcdwiki.com/res/MSP4021/4.0inch_SPI_Module_MSP4020&MSP4021_User_Manual_EN.pdf |
| TFT panel specification | https://www.lcdwiki.com/res/MSP4021/QD-40037C1-00_specification_V1.0.pdf |
| Module schematic PDF | https://www.lcdwiki.com/res/MSP4021/4.0inch_SPI_Schematic.pdf |
| ST7796S datasheet | https://www.lcdwiki.com/res/MSP4021/ST7796S-Sitronix.pdf |
| Sample code archive | https://www.lcdwiki.com/res/Program/Common_SPI/4.0inch/SPI_ST7796S_MSP4020_4021_V1.0/4.0inch_SPI_Module_ST7796S_MSP4020%26MSP4021_V1.0.zip |
| KiCad PCB package | https://www.lcdwiki.com/res/MSP4021/4.0inch_SPI_Altium_Package_Library.zip |
