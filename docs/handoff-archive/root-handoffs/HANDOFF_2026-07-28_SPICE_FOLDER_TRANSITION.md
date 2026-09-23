# Handoff: SPICE Folder Transition (2026-07-28)

## Why this handoff exists
We need to continue SPICE validation in a different project folder where the full KiCad/netlist/model files for the PV input switch are available.

## Work completed in this repo
A conceptual ngspice simulation was created and executed to test Vgs behavior of a high-side back-to-back NMOS pass pair with pull-up/pull-down gate drive.

### Files created/updated
- hardware/sim/nmos_vgs_check.cir
- hardware/sim/nmos_vgs_check.net
- hardware/sim/nmos_vgs_check.log
- hardware/sim/tmp_vin_12.cir
- hardware/sim/tmp_vin_24.cir
- hardware/sim/tmp_vin_36.cir
- hardware/sim/tmp_vin_50.cir
- hardware/sim/tmp_vin_12.log
- hardware/sim/tmp_vin_24.log
- hardware/sim/tmp_vin_36.log
- hardware/sim/tmp_vin_50.log

### Local simulator used
- ngspice binary path:
  C:\ProgramData\chocolatey\lib\ngspice\tools\Spice64\bin\ngspice.exe

## Key conceptual results (VIN sweep)
From manual VIN sweeps at 12/24/36/50 V:

- VIN=12 V: VGS_ON_MIN=1.14291e-03 V, VGS_OFF_MAX=2.51218 V, VOUT_ON_AVG=6.22954 V, ILOAD_ON_AVG=0.259604 A
- VIN=24 V: VGS_ON_MIN=2.28614e-03 V, VGS_OFF_MAX=2.67174 V, VOUT_ON_AVG=14.0684 V, ILOAD_ON_AVG=0.586262 A
- VIN=36 V: VGS_ON_MIN=3.42969e-03 V, VGS_OFF_MAX=2.79160 V, VOUT_ON_AVG=21.9448 V, ILOAD_ON_AVG=0.914485 A
- VIN=50 V: VGS_ON_MIN=4.76424e-03 V, VGS_OFF_MAX=2.90770 V, VOUT_ON_AVG=31.1564 V, ILOAD_ON_AVG=1.29835 A

Interpretation:
- ON-state effective Vgs collapses near zero.
- OFF-state Vgs remains a few volts.
- This indicates ambiguous/partial conduction risk in this gate-drive topology.

## Important ngspice compatibility note
This ngspice build reports:
- .step param ... is unimplemented
Use separate single-run decks per VIN (as done above), or an external script loop.

## What to do next in the new folder
Goal: replace conceptual models with exact device/netlist-level simulation.

1. Import exact KiCad/netlist connectivity for:
   - U2 (BSC112N06LD dual NMOS), Q2 (BSS138), R40, R41, R42, R43, D1, D2, R35, R36
2. Add exact SPICE models:
   - BSC112N06LD vendor model
   - preferred BSS138 model
   - TVS/zener models as needed
3. Define control truth states:
   - V_In_ON and CTRL_3V3 levels for ON and OFF conditions
4. Run operating-point and transient cases at real panel inputs (12-50 V).
5. Report pass/fail margins:
   - Vgs(on) headroom across temperature/corners (if model supports)
   - OFF leakage / blocking
   - conduction loss at target current

## Command template to rerun in PowerShell
$exe='C:\ProgramData\chocolatey\lib\ngspice\tools\Spice64\bin\ngspice.exe'
& $exe -b -o "<logfile>.log" "<deck>.cir"

## Suggested startup prompt in the new folder
"Continue from HANDOFF_2026-07-28_SPICE_FOLDER_TRANSITION.md. Build an exact ngspice deck from the local KiCad netlist for the PV input NMOS switch (U2/BSC112N06LD + Q2/BSS138 network), run ON/OFF simulations at VIN=12,24,36,50 V, and report Vgs margins and whether gate drive is valid."
