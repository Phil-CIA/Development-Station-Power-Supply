# ESP32-C3-MINI-1 / MINI-1U — Key Notes

Source: [Espressif ESP32-C3-MINI-1 and MINI-1U Datasheet, v2.2](ESP32-C3-MINI-1.pdf)

Curated facts used by this project. Cite page/table numbers from the PDF.
Prefer adding findings here over re-reading the full datasheet.

## Key specs

| Parameter | Value | Page |
|-----------|-------|------|
| Module supply range | 3.0–3.6 V | 3 |
| Radio transmit peak current | 350 mA at 3.3 V, Wi-Fi 802.11b 1 Mbps, +20.5 dBm, 100% TX duty; other Wi-Fi TX conditions in Table 6-4 | 22 |
| Module variants | MINI-1 has PCB antenna; MINI-1U has external antenna connector. Recommended current variants include 4 MB v1.1 options (N4X, or H4X for higher temperature); check current ordering status before selection. | 2–4 |
| Integrated clock | 40 MHz crystal oscillator | 2 |
| Programming / boot straps | GPIO2, GPIO8, and GPIO9 are sampled at reset. GPIO9 low selects joint download only with GPIO8 high; default SPI boot requires GPIO9 high. GPIO2 is recommended pulled high to avoid glitches. | 12–13 |
| Boot strap setup/hold | Power rails must stabilize before CHIP_EN rises; strap values are held for 3 ms after CHIP_EN rises before pins become regular GPIO. | 13 |
| Reset/power-up timing | Allow at least 50 µs for rails to stabilize before CHIP_EN is released; see Table 4-6 for reset-low timing. | 14–15 |
| Module footprint | MINI-1: 13.2 × 16.6 × 2.4 mm; MINI-1U: 13.2 × 12.5 × 2.4 mm. | 3–4 |
| Base-board antenna interaction | Minimize base-board interference with antenna performance; follow the referenced ESP32-C3 Hardware Design Guidelines for exact placement/keep-out requirements. | 39 |

## Project usage / gotchas

- Historical reference only: the October 15 decisions in issue #126 select
  the actual Teyleten Robot ESP32-C3 SuperMini development board, not a bare
  MINI-1/MINI-1U module. The bare-module supply, pin numbering, footprint,
  reset/strap, and antenna guidance below do not establish the SuperMini
  board's circuitry or mechanical fit. The current F405 UART4 role is a
  custom-loader transport, not the earlier application-only candidate.
- A Wi-Fi rail sized only for average current is insufficient. Budget for at
  least the datasheet's 350 mA peak TX condition plus the module's transient
  response and any other 3.3 V loads; verify regulator and decoupling on the
  actual board.
- The support-sheet J4 has only one boot-control signal (`C3_BOOT_N` to
  GPIO9). That alone does not guarantee ROM download mode: GPIO8 must also be
  high when straps are sampled. Decide how GPIO2/GPIO8/GPIO9 are biased or
  controlled and make the strap/recovery sequence accessible.
- The candidate J4 routes application UART signals to GPIO0/GPIO1. The
  datasheet identifies UART0 on GPIO20/GPIO21; this candidate connector does
  not expose UART0, and it does not route the module's USB Serial/JTAG pins
  GPIO18/GPIO19. Define an independent programming/recovery path before
  selecting this module/interface.
- For a surface-mount radio module, explicitly choose the PCB-antenna
  MINI-1 versus external-antenna MINI-1U variant and place the antenna with
  the manufacturer layout guidance. Neither antenna type/placement nor the
  HAT RF keep-out is selected.
- The selected Rev-D architecture must keep the STM32F405 as the rail-control
  and safety authority. Network loss, radio reset, firmware update, or
  coprocessor boot failure must not bypass F405 safe-state behavior.
