# Strip-Recorder Graph Screen — Integration Plan

Supersedes the D5 scope line in `crowpanel-screens-tracker.md` (issue #26).
Source: user-supplied `LVGL_Strip_Recorder_Spec.docx` (Yokogawa GX-style
paperless recorder) plus a Yokogawa GX reference screenshot. This doc adapts
that spec to our actual CrowPanel 4.3" hardware and telemetry, and records
where we intentionally fall short of the spec in this revision.

## 1. Source spec summary

- Continuous right-to-left scrolling trend, multi-channel pens, engineering
  units, colored traces, alarm markers, timestamp axis, recorder-bezel look.
- Feed rate adjustable: 10 s/div, 30 s/div, 1 min/div, 5 min/div, 15 min/div,
  1 hr/div — changes scroll rate live, preserves history.
- Touch controls: zoom, pause, history review (pan), return-to-live, pen
  select, channel visibility, feed-rate adjust.
- Side panel of current values; vertical bar gauges per channel (per the
  reference screenshot).
- Up to 6+ pens.

## 2. Current implementation (as of this branch)

`crowpanel-43-bringup/src/main.cpp`:
- Graph screen is two separate `lv_chart` line widgets: CH1 voltage (mV) and
  CH1 current (mA). **CH2 is not charted at all** — `TrendSample` only stores
  `v12_mV`/`i12_mA` (CH1).
- Fixed window: `kChartPoints = 120` points, sampled every `kTrendSampleMs =
  200 ms` with `kChartDecimation = 2`, i.e. a fixed ~2 minute view. No
  feed-rate control.
- Backing ring buffer `trend_buf[kTrendCapacity]` = 1800 samples (~7.5 min at
  4 Hz) in internal SRAM (not PSRAM), holding CH1 only.
- No pause/resume, no history pan, no alarm markers, no pen legend/visibility
  toggles, no recorder bezel styling.
- D5 (issue #26) previously scoped a lighter version of this: window selector
  (30 s/5 min/30 min), pause/resume, clear, per-channel visibility,
  autoscale. This doc **replaces** that D5 scope line.

## 3. Hardware feasibility constraints

- MCU: ESP32-S3-WROOM-1-N16R8 — 16 MB flash, 8 MB PSRAM. PSRAM is already
  committed to two 800×480×16bpp LVGL framebuffers (~1.5 MB) plus LVGL/UI
  overhead. Several MB of PSRAM headroom remain for a sample ring buffer.
- Telemetry arrives over UDI UART from the STM32 HAT; sample cadence is
  whatever the host pushes (currently read into `trend_buf` at up to 4 Hz).
- CH2 (`v3`/`i3` in the live labels) is read live but **never recorded** into
  history today — this is new plumbing, not just a UI change.
- `lv_chart` (LVGL 8.3) is a fixed-point-count circular buffer renderer. It is
  good at "scrolling live window" but has no native concept of panning into
  older history beyond its configured point count — true pan/zoom into an
  hour of history requires our own decimated backing store that we re-sample
  into the chart's point array, not something `lv_chart` gives us for free.

### What "full spec" would require vs. what we can do now

| Spec item | Full-spec cost | What we can realistically do this rev |
|---|---|---|
| 1 hr/div feed rate with full-resolution pan | ~1 hr × desired sample rate in PSRAM (e.g. 1 Hz × 3600 × (4 channels × 4 bytes + timestamp) ≈ 60–80 KB — actually cheap) *but* also needs a resampling/pan engine, not just a bigger array | Extend ring buffer to PSRAM, size it for ~1 hr at 1 Hz (coarse), but ship **pan/zoom into history as a later sub-bucket**, not in the first PR |
| Up to 6+ pens | New sensor channels we don't have (temp, consumption) | 4 real pens only: CH1 V, CH1 I, CH2 V, CH2 I (per prior decision) |
| Vertical bar gauges beside the trend (reference photo) | Extra LVGL bar widgets + layout | Defer; keep the existing numeric side-panel readouts, no bar gauges in this rev |
| Alarm markers tied to real alarm events | Needs `EVT:` fault frames wired to the chart (D7 fault-modal work is separate and not yet merged) | Add the rendering hook (vertical marker overlay) now, wire it to real fault events once D7 lands; stub/demo marker acceptable for first PR |
| Full recorder bezel chrome (paper-style ticks, group header, battery/clock icons) | Cosmetic LVGL styling pass | In scope — this is cheap relative to the above and matches "visually resembles a strip recorder" acceptance criterion |

**Decision (per user):** we will not chase full 1-hour/full-resolution
history or the 6-pen framework in this revision. The buffer will be sized
for what PSRAM comfortably allows, and feed rates slower than the live
buffer's real depth will show correctly-decimated-but-shorter real history
rather than a full synthetic hour. This gap is documented here, not hidden.

## 4. Revised scope — splits D5 into sub-buckets

Replace the single D5 tracker row with:

| ID | Branch (off `main`) | Scope |
|----|---|---|
| D5a | `phil-cia-crowpanel-strip-recorder-core` | Rebuild Graph screen as a strip-recorder-style scrolling chart: recorder bezel (header bar, group label, clock), 4-pen trend (CH1 V/I, CH2 V/I) sharing one chart area, pen legend with live value + color swatch, grid/time-axis styling. Extend `TrendSample` + ring buffer to carry CH2, move buffer to PSRAM. No feed-rate control yet (keep current fixed window) — this bucket is the re-skin + data-model fix. |
| D5b | `phil-cia-crowpanel-strip-recorder-feedrate` | Feed-rate selector (10 s/30 s/1 min/5 min/15 min/1 hr per div, capped to real buffer depth), pause/resume, return-to-live, per-pen visibility toggle. Resamples the PSRAM ring buffer into the chart rather than relying on `lv_chart`'s own circular mode alone. |
| D5c | `phil-cia-crowpanel-strip-recorder-history` | History review: pan backward through the buffered window while paused, touch/drag or prev/next-page buttons, explicit "LIVE" badge when not panned. Bounded by buffer depth from D5a (not the full spec's unbounded 1 hr target). |
| D5d | `phil-cia-crowpanel-strip-recorder-alarms` | Alarm marker rendering hook (vertical line + icon on the chart) wired to whatever fault/event source exists at the time (stub source if D7 hasn't merged yet; real `EVT:` frames once it has). |

Each bucket is its own branch + PR per `SYSTEM_DEVELOPMENT_WORKFLOW.md` /
tracker rules of engagement, bench-tested on the physical CrowPanel before
merge. D5a must merge before D5b/c/d start (they depend on the PSRAM buffer
and CH2 data model it introduces).

## 5. D5a data-model changes (first PR)

- Extend `TrendSample` to add `v2_mV` (CH2 voltage) and `i2_mA` (CH2 current)
  alongside the existing CH1 fields.
- Move `trend_buf` from a static internal-SRAM array to a
  `heap_caps_malloc(..., MALLOC_CAP_SPIRAM)` allocation sized for the agreed
  buffer depth (to be picked once exact per-sample byte size and target
  duration are finalized — see open question below).
- Keep `trendPushSample()`/`trendAt()`/ring-index helpers; they're
  structurally reusable, just need the new fields threaded through and the
  CH2 live read (`t.last_v3_mV`/`last_i3_mA` or equivalent) captured into the
  ring on each tick instead of only updating the live labels.
- Add a second pair of `lv_chart_series_t*` (CH2 V, CH2 I) to the existing
  two chart objects (or to one combined chart — see layout question below).

## 6. Open questions before D5a starts

1. **One combined chart vs. two stacked charts for 4 pens?** Current screen
   has separate V-chart and I-chart. Spec's reference photo shows one shared
   plot area with all pens overlaid and a shared vertical gauge strip. Given
   V and I use very different scales, recommend keeping **two stacked chart
   areas (V pair, I pair)** rather than overlaying all 4 traces on one axis —
   confirm this is acceptable before D5a locks in layout.
2. **PSRAM buffer depth target.** Needs a concrete number (e.g. "10 minutes
   at 4 Hz" vs. "30 minutes at 1 Hz") to size the allocation in D5a. Pick
   this once D5b's feed-rate list is finalized against buffer depth.
3. **Alarm source for D5d.** Confirm whether to stub synthetic alarm markers
   for bench demo purposes before D7 (fault modal / `EVT:` plumbing) merges,
   or to block D5d until D7 lands.

## 7. Acceptance criteria (this revision's scope)

- Graph screen visually resembles a strip chart recorder (bezel, grid, pens,
  scrolling behavior) per the spec's acceptance criteria.
- 4 real channels (CH1 V/I, CH2 V/I) plotted with distinct colors and a
  legend showing live values.
- Feed rate adjustable across the spec's list, capped to real buffer depth,
  with pause/resume and return-to-live.
- Bounded history review within buffer depth — not unbounded 1-hour pan.
- Documented, not silently dropped: 6-pen framework, full 1-hour
  full-resolution pan, and bar gauges are explicitly deferred (section 3).
