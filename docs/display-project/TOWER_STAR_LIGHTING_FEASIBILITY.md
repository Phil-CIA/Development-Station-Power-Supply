# Tower Star Lighting Feasibility (WS28xx vs modern alternatives)

## Scope and priority

This study evaluates replacing an existing tower-top star lighting system:

- Existing system: **120VAC**, **72 medium-base lamps**, **11W each** (nameplate total **~792W**)
- Installation height: **~125 ft**
- Condition: wiring/sockets degraded by weather and UV exposure

Primary optimization priority (confirmed): **reliability + low maintenance at height**, with explicit **minimum-light fallback** if the primary path is damaged.

Operational profile update:
- Active season: approximately **6 weeks/year**, **night-only operation**
- Inactive season: system is installed but mostly unpowered/idle
- Dominant hazard concern: **lightning/surge exposure during long off-season idle periods**

---

## Constraint summary

1. **Serviceability risk dominates**: truck/lift access and weather windows make tower trips expensive.
2. **Outdoor exposure is severe**: UV, moisture ingress, thermal cycling, and wind motion.
3. **Long cable run reality**: data integrity and voltage drop become first-order design constraints.
4. **Failure behavior matters more than effects**: a partial fault should not black out the full star.
5. **Seasonal utilization is low**: protection design is driven more by environmental survival than by annual runtime hours.

---

## Candidate architectures

| Option | Description | Key Pros | Key Risks / Penalties |
|---|---|---|---|
| A. WS28xx addressable strips/pixels | Individually addressable RGB (5V WS2812-class or 12V WS2811-class) | Maximum visual flexibility and animation | High current at low voltage, frequent power injection, long-run data integrity challenges, many failure points |
| B. Constant-voltage zoned LED strips/modules | 24V white (or RGBW) strips/modules in independently fused branches | Simpler wiring/control, lower current than 5V/12V pixels, easier isolation and fault containment | Less per-pixel animation capability |
| C. Pro pixel nodes with DMX/sACN controller | Commercial outdoor pixel ecosystem (nodes/rope + industrial controller) | Better tooling, diagnostics, and weather-rated parts than hobby strips | Cost and complexity increase; still pixel-network failure modes |
| D. LED medium-base lamp retrofit | Keep socket architecture, replace lamps/wiring with modern weather-rated equivalents | Fastest path, lowest change risk, simplest troubleshooting | Least modernization benefit; fewer advanced visual effects |

---

## First-pass electrical feasibility checks

Because exact star perimeter and conductor gauge were not provided, use two planning lengths:

- **Case L1:** ~120 ft luminous path (~36.6 m)
- **Case L2:** ~180 ft luminous path (~54.9 m)

### A) WS28xx strip-class estimates

Assumed typical strip powers (worst-case full-white):

- 5V WS2812-class: ~18 W/m
- 12V WS2811-class: ~14.4 W/m

| Technology | Case L1 Power | Case L1 Current | Case L2 Power | Case L2 Current | Practical impact |
|---|---:|---:|---:|---:|---|
| 5V WS2812-class | ~659W | **~132A @5V** | ~988W | **~198A @5V** | Not practical for this geometry/reliability target |
| 12V WS2811-class | ~527W | **~44A @12V** | ~790W | **~66A @12V** | Feasible only with many injection points + robust segmentation |

**Implication:** WS28xx is technically possible, but maintenance burden and fault count are high for a 125 ft tower-top install.

### B) 24V constant-voltage estimates

Assumed strip/module baseline: ~9.6 W/m

| Technology | Case L1 Power | Case L1 Current | Case L2 Power | Case L2 Current | Practical impact |
|---|---:|---:|---:|---:|---|
| 24V constant-voltage | ~351W | ~14.6A @24V | ~527W | ~22.0A @24V | Much easier conductor sizing and segmentation than WS28xx |

**Implication:** 24V zoned architecture is substantially more robust for long outdoor runs.

---

## Reliability and fallback design requirements (minimum-light guarantee)

Any selected architecture should include:

1. **Electrical segmentation:** at least one independent branch per star section (for example, one branch per point or per paired points), each branch fused.
2. **Fault containment:** no single branch fault can darken the entire star.
3. **Backup illumination path:** independent low-power static-white circuit that can keep a minimum visible outline if the primary system fails.
4. **Controller fail-safe state:** primary controller failure defaults to safe static pattern (or automatic transfer to backup driver).

### Recommended fallback pattern

- **Primary:** normal operating system (zoned CV or pixel, depending final decision).
- **Backup:** separate 24V static-white “life-safety visibility” branches sized for ~20–35% nominal brightness.
- **Transfer logic:** normally-healthy primary enable; on primary fault/loss, backup auto-enables (relay or solid-state supervised transfer).

This delivers “still visibly lit” operation under partial or full primary-path failure.

---

## Transient-voltage protection and clamping strategy (added requirement)

For a 125 ft outdoor metallic structure, surge/transient events are not edge cases.  
The selected architecture should include a layered protection stack:

1. **AC service entrance protection**
   - SPD at the AC feed panel serving the lighting PSU(s).

2. **24V distribution protection**
   - TVS clamping at PSU outputs and at long branch ingress points.
   - Branch fusing coordinated with expected startup/inrush behavior.

3. **Controller and I/O hardening**
   - Dedicated clamp/protection on controller power and zone-control outputs.
   - Galvanic isolation (or isolated drivers) where control wiring exits protected enclosures.

4. **Grounding and bonding discipline**
   - Single documented bonding strategy to avoid floating/shifting references during surge events.
   - No undefined shield/drain termination practices.

5. **Fallback-path immunity**
   - Backup minimum-light path receives the same transient protection class as the primary path.

This is one of the strongest arguments for 24V zoned CV over hobby-style low-voltage pixel strip wiring.

### Initial concrete protection baseline (rev A targets)

Use these as starting design targets, then finalize against the selected PSU datasheet and installer code requirements:

| Location | Protection element | Initial target |
|---|---|---|
| AC feed to lighting PSU enclosure | Type 2 SPD | 120V split-phase compatible SPD, nominal discharge class at least 20 kA per mode |
| AC line-to-neutral clamp | MOV network (inside SPD/enclosure) | 275 VAC MOV class, thermally protected |
| 24V PSU output bus | High-power TVS | 33V stand-off class TVS, surge-rated (SMCJ/5kW class or stronger) |
| Zone branch entry (each long run) | Local TVS | 33V stand-off TVS per branch near cable entry |
| Zone branch output protection | Fuse | Time-delay DC branch fusing sized at 125% of measured steady-state zone current |
| Controller 24V input | eFuse/hot-swap + TVS | UVLO, soft-start/inrush control, over-voltage cutoff around 30-32V plus local TVS |
| Control I/O leaving enclosure | Clamp/isolation | TVS + series impedance, or isolated transceiver where cable exits enclosure |
| Backup lighting feed | Same suppression class as primary | AC SPD + 24V bus TVS + branch TVS + branch fuse |

Notes:
- 33V TVS class is the normal starting point for a regulated 24VDC rail where nuisance conduction at normal max bus voltage must be avoided.
- If final PSU trim range or cold-start overshoot exceeds this window, raise stand-off class accordingly and re-check downstream survivability.
- Because annual energized hours are low, prioritize **surge robustness and isolation discipline** over efficiency micro-optimization.

### Seasonal operation implications (6-week night-only profile)

1. Add a documented **off-season state**:
   - AC feed disconnected/isolated at service disconnect when not in holiday operation.
   - Controller and LED loads de-energized outside active season unless needed for periodic checks.

2. Keep protection always “first in line” when energized:
   - SPD and bonding path remain permanently installed and connected even though the lighting load is seasonal.

3. Add pre-season recommissioning checklist:
   - Visual inspection of SPDs, TVS modules, enclosure seals, and grounding/bond jumpers.
   - Verification that backup fallback path still transfers correctly before tower season start.

---

## Recommendation (reliability-first)

### Primary recommendation

Use **Option B: 24V constant-voltage zoned LED modules/strips** as the primary architecture, with independent branch protection and a dedicated backup minimum-light circuit.

Why:

- Lowest electrical stress and current density for long runs
- Simplest troubleshooting and field replacement
- Best alignment with low-maintenance tower operation
- Straightforward redundancy implementation

### Secondary recommendation (if advanced effects are mandatory)

Use **Option C (pro outdoor pixel ecosystem)** rather than hobby WS28xx strips. If WS28xx-style effects are required, prefer commercial outdoor nodes + industrial controller + differential signaling path, not raw hobby strip topology as the sole lighting system.

### Not recommended as sole primary at this height

Use of **raw WS28xx strip architecture (Option A)** as the only illumination path is **not recommended** for this installation priority profile (too many injection/data/failure points).

---

## Implementation staging

1. **Survey and measurement pass**  
   Confirm exact perimeter length, branchable geometry, existing conductor gauge/count, enclosure space, and grounding/bonding condition.

2. **Ground-level prototype**  
   Build one representative branch with full weatherproof connector stack and run thermal/water ingress checks.

3. **Burn-in and fault drills**  
   72-hour burn-in with induced branch-open/short tests and primary-failover verification.

4. **Tower reinstall acceptance**  
   Verify branch currents, fallback auto-transfer, and post-install insulation/ground continuity.

---

## Open data needed before final BOM

- Exact star mechanical dimensions and true luminous path length
- Existing cable routes and conductor sizes
- Reuse vs replacement decision for sockets/harnesses
- Preferred white CCT/CRI and minimum fallback brightness target
- Environmental sealing standard to adopt (connector/enclosure family)

---

## Requirements handoff

Detailed implementation requirements for the selected architecture are captured in:

- `docs/display-project/TOWER_STAR_24V_REQUIREMENTS.md`
