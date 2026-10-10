---
mode: agent
description: "Rev-D session 5 - integrate the selected HAT source with F405 support"
---
Work on issue #102, session 5: partial full-HAT integration in
`hardware/kicad/dsp-regulator-hat-rev-d-full/`.

The HAT sheet uses the user's exact selected source:
`C:\Users\forch\OneDrive\JLCPCB files\Development station supply\Regulator Hat\REV C\KiCad Files\DSP-Regulator-HAT-RevC.kicad_sch`
(SHA-256
`34EB41BA12237268CF8E8C640CFA2B76A4710CFF8DE99C9E988C9717D22872B3`).
Its fresh KiCad 10.0.5 export is 57 components / 83 nets. This explicit
selection supersedes PR112's earlier repository-source import and the old
prompt instruction not to use the supplied schematic. It is a design-source
choice only; it does not prove a match to the 2026-08-28 production PCB or a
physical HAT. See the full-HAT README and
`docs/HAT_REVD_PIN_AND_SUPPORT_CONTRACT.md` for current checked results.

The current HAT sheet retains 55 source components after removing source U11
(Blue Pill) and U2 (ESP32-C3 development board). Do not add a bare C3 module
or infer source-absent endpoints. The existing full-project root and MCU
support sheet are byte-identical to their reviewed copies. Preserve them and
the entire support-only project, repository Rev-C projects, and external
production folder byte-identically. Resolve only actual reference collisions;
source HAT R1/R2/R3 map to R20/R21/R22 on the HAT sheet without changing
values or footprints.

Existing F405 global labels were inspected. The supported paths are SPI flash
(PA5/6/7/8), CH340 UART1 TX/RX (PB6/PB7), UDI (PB10/PB11), I2C (PB8/PB9),
fan control (PB5 via `FAN_PWM`), and SWDIO/SWCLK (PA13/PA14). Preserve the
selected source's J12 pin contract and fan circuit exactly. Its fan is a
3-pin J7 with source JP1 supply selection, R64=100k, and no MCU tach input;
do not replace these values or connector/polarity with the repository-source
fan circuit to match the frozen pin targets.

The selected source has no ISET destinations, shift-register, AW9523, or
complete measurement/protection blocks. Leave those F405 functions
unintegrated and document them; do not import old repo-only endpoints.
Ground legacy WS2812B D12 DIN only as documented, intentional exception;
unused CH340 CTS/RTS are NC because the F405 interface has TX/RX only. J4 is
still a logical C3 interface, not the bare C3 module, power, straps, UART0
recovery or antenna implementation.

Keep all Rev-C files unchanged. Run full-severity KiCad ERC and export the
three-sheet netlist. Record source-pair connectivity, value/footprint and
reference checks; state remaining warnings, fabrication gates and explicit
absence of production-board identity and bench validation. Do not suppress
ERC findings or claim as-built identity, full-HAT completion, fabrication
readiness or hardware validation.
