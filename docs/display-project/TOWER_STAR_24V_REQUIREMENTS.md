# Tower Star 24V Zoned Lighting Requirements

Status: draft requirements baseline for the recommended architecture  
Architecture: **24V constant-voltage zoned LED system + independent minimum-light backup circuit**

This document converts the feasibility recommendation into implementation requirements.

Related:
- Feasibility and decision rationale: `docs/display-project/TOWER_STAR_LIGHTING_FEASIBILITY.md`

---

## 1) System intent

The tower star shall provide reliable nighttime visibility with minimal maintenance events at ~125 ft elevation, with graceful degradation under fault conditions.

Primary requirement:
- A single failure (branch, controller output, driver channel, or connector fault) shall **not** darken the full star.

Fallback requirement:
- On primary-path failure, the system shall automatically provide **minimum-light** operation that preserves recognizable star shape.

Seasonal duty profile requirement:
- System operation is expected to be approximately **6 weeks per year**, predominantly **night-only**; design priorities shall reflect low duty-cycle operation with high environmental exposure while idle.

---

## 2) Functional requirements

1. **Normal mode**
   - Illuminate full star outline at configured nominal brightness.
   - Support dimming (global level control) but not per-pixel animation as a hard requirement.

2. **Fallback mode**
   - Auto-engage backup lighting path on primary fault/loss.
   - Maintain minimum visible star outline at **20–35%** nominal brightness target.

3. **Degraded operation**
   - Loss of one primary zone shall leave all remaining zones operational.
   - Backup path shall be electrically independent from primary control electronics.

4. **Recovery**
   - After fault clears and is acknowledged, controlled return to normal mode without inrush-induced trips.

5. **Seasonal mode control**
   - System shall support a defined OFF-SEASON state with AC-side isolation/disconnect procedure.
   - OFF-SEASON state shall be documented to minimize unnecessary energized exposure while preserving protection readiness for next season.

---

## 3) Electrical architecture requirements

### 3.1 Distribution and zoning

1. Use **24VDC** lighting distribution for primary path.
2. Segment star into independent zones (recommended: one zone per point, or paired points if geometry/wiring requires).
3. Each zone shall have:
   - Independent branch fuse or resettable protection
   - Branch disconnect point for service
   - Polarity-protected connectoring strategy

### 3.2 Voltage-drop and conductor sizing

1. Design target at end of each branch: keep LED supply within manufacturer operating window under full-load and winter startup conditions.
2. Document worst-case branch current and voltage drop with actual installed cable gauge/length before BOM freeze.
3. Provide power injection strategy at intervals required by selected strip/module product.

### 3.3 Backup path

1. Backup illumination shall be fed from a **separate protected 24V branch set** (or separate backup PSU output with independent protection).
2. Backup path shall not depend on primary PWM/dimming controller health.
3. Transfer mechanism shall default to backup ON under detected primary-fail state.

### 3.4 Protection and surge

1. Include branch-level overcurrent protection for every zone.
2. Include supply-side surge protection appropriate for outdoor elevated metallic structure installations.
3. Include grounding/bonding scheme consistent with local electrical code and tower grounding practice.

### 3.5 Transient-voltage suppression and clamping

1. Provide a layered transient suppression design, including:
   - AC-side SPD at feed panel or service point
   - DC-side TVS clamping at 24V PSU output
   - Additional TVS clamping at long branch entry points
2. Clamp selection shall be coordinated for:
   - Nominal 24V operation (no nuisance conduction in normal conditions)
   - Fast overvoltage clamping during surge/transient events
   - Adequate energy/pulse handling margin for expected exposure
3. Controller supply and zone-control interfaces shall include local clamp/protection networks.
4. Backup minimum-light circuit shall have equivalent transient suppression coverage as the primary path.
5. Protective elements shall be placed physically close to entry points they protect, with low-impedance return paths to the designated bonding reference.

### 3.6 Transient design parameter baseline (to be finalized at BOM freeze)

1. AC-side surge protection:
   - Install a Type 2 SPD at the AC feed for the lighting PSU enclosure.
   - Minimum discharge class target: 20 kA per mode (or higher if site assessment requires).

2. 24VDC bus clamping:
   - Primary bus TVS at PSU output: 33V stand-off surge-rated TVS (SMCJ class/5kW or stronger).
   - Per-zone entry TVS: 33V stand-off local clamp at each long branch ingress.

3. Branch overcurrent coordination:
   - Each zone branch fuse rating shall be set to 125% of measured steady-state current at max configured brightness.
   - Fuse type shall tolerate startup/inrush without nuisance opening.

4. Controller power protection:
   - Controller 24V feed shall include hot-swap/eFuse behavior with soft-start.
   - Over-voltage cutoff target: 30-32V at controller input.
   - Under-voltage lockout shall prevent brownout chatter/restart oscillation.

5. Backup path parity:
   - Backup minimum-light branch network shall match primary-path surge/clamp coverage class.

6. Documentation requirement:
   - Final design package shall include a clamp coordination sheet showing: nominal rail, max normal rail, TVS stand-off, clamp voltage, and protected-load absolute max.

7. Idle-season lightning emphasis:
   - Surge/lightning robustness shall be treated as a primary reliability driver even though active run-hours are low.
   - Protection placement and bonding integrity shall be preferred over marginal cost/efficiency reductions.

---

## 4) Environmental and mechanical requirements

1. All exterior connectors shall be outdoor-rated, UV-resistant, and strain-relieved.
2. Harness routing shall prevent water pooling at connector entries (drip loops where applicable).
3. Enclosures shall be selected for outdoor exposure and maintenance access.
4. Fastening and cable support shall tolerate wind-induced vibration and thermal cycling.

---

## 5) Controls and monitoring requirements

1. Primary controller shall support:
   - Global dimming schedule/setpoint
   - Per-zone enable/disable for troubleshooting
   - Fault indication for zone open/short/overcurrent (where supported)

2. Minimum monitoring telemetry (local or remote):
   - PSU output voltage/current
   - Zone trip/fuse state (or inferred branch fault state)
   - Active mode state: NORMAL vs FALLBACK

3. Controller failure behavior:
   - Primary controller loss shall not prevent backup minimum-light operation.

---

## 6) Safety and service requirements

1. Service disconnect procedure shall isolate primary and backup circuits independently.
2. Labeling shall identify:
   - Zone IDs
   - Primary vs backup circuits
   - Fuse/protection ratings

3. Replaceable field units (zone module/branch segment/driver channel) shall be serviceable without rewiring unaffected zones.

---

## 7) Acceptance criteria (must pass before tower reinstall signoff)

1. **Normal-mode burn-in**
   - 72-hour continuous operation with no uncontrolled blackout.

2. **Single-zone fault drill**
   - Induced fault in one zone does not extinguish other zones.

3. **Primary failure drill**
   - Simulated primary controller/output failure triggers automatic fallback mode.
   - Fallback maintains recognizable star outline at target minimum-light level.

4. **Recovery drill**
   - Restored primary path returns to normal mode without nuisance trip or unsafe inrush behavior.

5. **Post-install verification**
   - Measured branch currents and voltage drops within design limits.
   - Ground/insulation checks pass per installation procedure.
   - Transient protection devices are installed per design intent and pass commissioning checks.

6. **Transient protection validation drill**
   - Verify clamp network installation against as-built checklist (SPD model, TVS part numbers, fuse ratings, grounding bonds).
   - Capture startup and shutdown bus waveform at PSU output and far branch entry; no destructive over-voltage excursion at protected loads.
   - Verify controller survives induced branch switching events without reset/latch-up.

7. **Seasonal readiness / recommissioning drill**
   - Define and execute OFF-SEASON -> IN-SEASON transition checklist before first annual operation.
   - Verify AC disconnect/isolation function, SPD health indication status, grounding/bond continuity, and backup transfer function.

---

## 8) Data required to finalize design package

1. Actual star geometry (point count interpretation, perimeter, branch routing lengths)
2. Existing feed cable inventory (conductor count, gauge, insulation condition)
3. Preferred output appearance (warm/cool white target, allowable nonuniformity)
4. Service method constraints (lift access window, max visit duration)
5. Site electrical and grounding constraints from local installer/electrician review

---

## 9) Recommended next deliverables

1. Zone map drawing (zone IDs, branch lengths, injection points)
2. Preliminary single-line diagram (primary + backup + transfer logic)
3. Branch current and fuse coordination table
4. Environmental connector/enclosure selection matrix
5. FAT/SAT checklist derived from Section 7 acceptance criteria
