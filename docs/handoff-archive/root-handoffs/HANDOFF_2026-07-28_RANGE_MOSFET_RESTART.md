# Handoff: Main Project Restart for Range MOSFET Fix (2026-07-28)

## Session closeout
This session validated the likely root issue class: insufficient and ambiguous Vgs control in the high-side back-to-back NMOS range-switch concept.

A conceptual SPICE run was completed and showed:
- ON-state effective Vgs collapsing near zero
- OFF-state still showing non-trivial Vgs
- behavior consistent with partial conduction / unstable range-switch behavior

Related handoff with full simulation details:
- HANDOFF_2026-07-28_SPICE_FOLDER_TRANSITION.md

## Next session objective (main project)
Return to the main Development-Station-Power-Supply design and produce a concrete fix for the range MOSFET topology used in measurement range selection.

## Priority tasks
1. Map the exact range-switch MOSFET network in the main project schematic/netlist.
2. Confirm gate-drive levels versus source potential for both ON and OFF states.
3. Decide final corrective topology:
   - PMOS high-side back-to-back with proper gate pull network, or
   - NMOS high-side with dedicated boosted gate driver, or
   - low-side switching if architecture allows.
4. Simulate fixed topology with realistic rail/load values.
5. Produce implementation-ready change list (parts, net changes, resistor values, and expected behavior).

## Known working local simulator path
C:\ProgramData\chocolatey\lib\ngspice\tools\Spice64\bin\ngspice.exe

## Suggested new-chat startup prompt
Continue from HANDOFF_2026-07-28_RANGE_MOSFET_RESTART.md and HANDOFF_2026-07-28_SPICE_FOLDER_TRANSITION.md. We are back on the main Development-Station-Power-Supply project and need a concrete fix for the range MOSFET switching topology. Locate the exact range-switch netlist path, validate Vgs margins for ON/OFF, propose the best topology change, and produce a simulation-backed implementation plan.
