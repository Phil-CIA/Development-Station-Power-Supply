# Strip-Recorder Graph Screen — Integration Plan (Rev 2)

Supersedes the D5 scope line in `crowpanel-screens-tracker.md` (issue #26).
Source: user-supplied `LVGL_Strip_Recorder_Spec.docx` (Yokogawa GX-style
paperless recorder) plus a Yokogawa GX reference screenshot.

**Rev 2 changes vs. Rev 1:** the source spec was a wishlist, not a hard
requirement set. After a closer look at our actual hardware/telemetry
constraints, several items turned out cheaper than first assumed (see
section 3), while others were cut by explicit user direction: no alarm/relay
functions, no history-scroll-back, trimmed pen controls. CSV/PC export was
split out to a separate tracked issue
([#84](https://github.com/Phil-CIA/Development-Station-Power-Supply/issues/84)).

## 1. Source spec summary

- Continuous right-to-left scrolling trend, multi-channel pens, engineering
  units, colored traces, alarm markers, timestamp axis, recorder-bezel look.
- Feed rate adjustable: 10 s/div, 30 s/div, 1 min/div, 5 min/div, 15 min/div,
  1 hr/div — changes scroll rate live.
- Touch controls: zoom, pause, history review (pan), return-to-live, pen
  select, channel visibility, feed-rate adjust.
- Side panel of current values; vertical bar gauges per channel.
- Up to 6+ pens.

## 2. Current implementation (baseline)

`crowpanel-43-bringup/src/main.cpp`:
- Graph screen is two separate `lv_chart` line widgets: CH1 voltage (mV) and
  CH1 current (mA) only.
- Fixed window: `kChartPoints = 120` points, sampled every `kTrendSampleMs =
  200 ms` with `kChartDecimation = 2` — a fixed ~2 minute view, no feed-rate
  control.
- Backing ring buffer `trend_buf[kTrendCapacity]` = 1800 samples (~7.5 min at
  4 Hz) in internal SRAM, CH1 only.
- No pause/resume, no history pan, no alarm markers, no pen legend/visibility
  toggles, no recorder bezel styling.
- Existing USB-serial debug command `LOG_DUMP_CSV [N]` already dumps this
  buffer as CSV over the console — see issue #84.

## 3. Hardware reality check (corrected from Rev 1)

Rev 1 treated history depth and feed rate as primarily memory-constrained.
That was wrong. The real numbers:

- **CH2 is already on the wire.** The UDI telemetry frame from the STM32 HAT
  includes CH2 (`last_v3v3_mV`/`last_i3v3_mA`) in *every* frame at the same
  cadence as CH1 — it's already read and shown live on the Main screen. Only
  charting/history was ever CH1-only. Adding CH2 to the trend buffer and
  chart is a small, self-contained change, not a bandwidth or hardware fix.
- **UART is not the bottleneck.** 115200 baud, ~5 Hz sample cadence today.
  Plenty of headroom; sample rate is a firmware choice, not a wire limit.
- **PSRAM is not the bottleneck.** ESP32-S3 has 8 MB PSRAM; ~1.5 MB is used
  by the two LVGL framebuffers. Even a naive full hour of all 4 channels at
  full ~5 Hz resolution is well under 1 MB — trivial.
- **The actual limits are:**
  1. `lv_chart`'s useful point count tops out around the chart's pixel width
     (~650 px) — no benefit storing more points than that for a given view.
  2. Per user direction, we are **not** building history scroll-back, so the
     buffer only needs to cover *the currently selected feed rate's visible
     window*, not an archive.
  3. Slower feed rates (15 min/div, 1 hr/div) necessarily show a *coarser*
     view of whatever's in the buffer — they are not meant to replay a full
     literal hour of untouched raw samples.

### Feed-rate data strategy — options considered

| Option | Approach | Cost | Outcome |
|---|---|---|---|
| A — live-only bucket averaging | One small fixed chart buffer (~700 pts); each incoming sample bucket-averages into the next chart point; changing feed rate re-arms bucket duration going forward only | Near-zero memory | Switching feed rate restarts the visible trace from "now"; no instant backlog |
| **B — small raw buffer + instant resample (chosen)** | Keep ~10–15 min of raw ~5 Hz samples in a small PSRAM ring; feed-rate change immediately resamples/redraws from this buffer | Tens of KB, still trivial | Instant redraw on feed-rate switch; slow settings (15 min/1 hr per div) show a decimated view of the ~10–15 min buffer, not a true literal hour |
| C — host-side pre-aggregation | STM32 HAT keeps its own longer rolling summary and ships a second slow-rate stream over a new UDI message type | New protocol + host firmware work | Deferred — no current need now that B is proven cheap enough |
| D — host-rendered bitmap transfer | Host renders the chart as an image, ships it over UART instead of scalars | Slower than scalars at 115200 baud, loses interactivity, duplicates rendering on two MCUs | Rejected — not worth the effort this rev |

**Decision:** Option B. Full Yokogawa feed-rate list (10 s / 30 s / 1 min /
5 min / 15 min / 1 hr per div) is kept selectable, with the explicit
understanding that 15 min/div and 1 hr/div only have ~10–15 real minutes of
backing data — older portions of those slow views show as empty/no-data
rather than synthetic history. This is documented in the UI (see section 6).

## 4. Explicitly cut from this revision (per user direction)

- **No alarm markers / relay functions.** Entire alarm overlay and any
  relay-driven behavior from the original spec is dropped, not just
  deferred.
- **No history scroll-back / pan.** The chart only ever shows the live
  window at the selected feed rate; no "rewind" interaction, no "LIVE"
  badge needed since there's no non-live state to leave.
- **Pen (channel) controls trimmed, not fully removed.** Per user choice:
  keep a simple per-channel show/hide toggle for the 4 real channels
  (CH1 V/I, CH2 V/I). No 6-pen framework, no pen "selection" beyond
  show/hide, no per-pen color/scale editing.
- **No bar gauges beside the trend.** Keep the existing numeric side-panel
  readouts; the Yokogawa reference photo's vertical bar gauges are not
  being added.
- **CSV/PC export is a separate, already-mostly-built feature.** Tracked in
  issue [#84](https://github.com/Phil-CIA/Development-Station-Power-Supply/issues/84),
  not part of the D5x buckets below. `LOG_DUMP_CSV` already exists; it just
  needs CH2 columns once D5a lands.

## 5. Revised scope — D5 sub-buckets

| ID | Branch (off `main`) | Scope |
|----|---|---|
| D5a | `phil-cia-crowpanel-strip-recorder-core` | Recorder-bezel re-skin of the Graph screen (header bar, group label, grid/time-axis styling). Extend `TrendSample` to carry CH2 (`v2_mV`/`i2_mA`), move the ring buffer to a PSRAM allocation sized for ~10–15 min at current sample rate. Two stacked chart areas (V pair, I pair — see rationale below), each with both channels' traces, pen legend with live value + color swatch per channel. No feed-rate control yet — this bucket is the re-skin + CH2 data-model fix. |
| D5b | `phil-cia-crowpanel-strip-recorder-feedrate` | Feed-rate selector (10 s/30 s/1 min/5 min/15 min/1 hr per div per Option B), pause/resume, return-to-live, simple per-channel show/hide toggle. Resamples the PSRAM ring buffer into the chart on every feed-rate change. |
| D5c | `phil-cia-crowpanel-strip-recorder-microview` | Per-channel micro-view (CH1, CH2): one taller V+I graph from that channel's data only, auto-scaled for display, with V and I on separate bands and axes. See section 5a. Time-of-day axis replaces uptime labels once #82 lands. |

The Rev 1 D5c (history pan) and D5d (alarm markers) are **removed**, not
deferred — they are out of scope per section 4. The D5c above is a new,
unrelated proposal.

## 5a. D5c micro-view (as implemented)

One `Micro` screen serves both channels (`micro_channel`); there is no second channel page.

- **Navigation.** Detail has a `MICRO / V / A zoom` button; the Graph header keeps CH1/CH2 buttons. Micro has a
  Back button that returns to the screen that opened it (Detail or Graph; Detail is re-selected to the
  channel last viewed in Micro), a Main button, and CH1/CH2 switches that keep the origin. All header
  buttons are at least 44 px tall. Serial `SCREEN MICRO1|MICRO2` also opens it.
- **Data.** Only the selected channel's samples from the existing PSRAM ring, resampled by the same
  time buckets as Graph. Live and demo samples are never mixed. Empty buckets stay blank. The raw ring is
  never modified; all scaling is display-only.
- **Trace positioning.** V occupies the upper band (55-92 % of plot height, left axis, `V`), I the lower
  band (8-45 %, right axis, `A`), so the traces cannot overlap. Each axis is labelled with its unit, the
  headers say which trace is which, and a caption states that V and A are scaled independently and are
  not comparable.
- **Autoscale.** Span covers the window's raw min and max, snapped outward to 100 mV (V) / 10 mA (I).
  Minimum span is 2 steps (200 mV / 20 mA) grown alternately below and above, so flat data sits near the
  middle of its band. Signed current is handled (labels carry the sign). No data: nominal V (5.0 V / 3.3 V)
  or 0 A, blank traces, `NO DATA` note. Peaks and outliers are never clipped; a spike expands the scale
  immediately. Hysteresis: the scale grows at once and shrinks only when the held span exceeds twice the
  needed span; held spans reset on channel, feed-rate or live/demo change.
- **Shared with Graph.** Feed rate, pause/resume, time axis (uptime; time of day waits for #82) and the
  fault row. **Not applicable to Micro:** per-pen show/hide (both traces are always drawn).
- **State treatment.** Header link chip (LIVE / STALE n s / DEMO DATA / UNKNOWN), traces and live readouts
  dimmed when STALE, and a note in the plot gap for NO DATA, DEMO, legacy no-CH2 frame, STALE, or an empty
  window.
- **Pause and rollover.** While paused the window is frozen and not recomputed. If the plot is rebuilt
  (re-entering Micro, changing channel) after the ring has overwritten samples that belonged to the
  frozen window, those buckets stay blank and the note reads `PAUSED - older samples overwritten`.
- **Not asserted.** No CV/CC claim (#77).

Each bucket is its own branch + PR per `SYSTEM_DEVELOPMENT_WORKFLOW.md` /
tracker rules of engagement, bench-tested on the physical CrowPanel before
merge. D5a must merge before D5b starts (it depends on the PSRAM buffer and
CH2 data model it introduces).

## 6. D5a data-model changes (first PR)

- Extend `TrendSample` to add `v2_mV` (CH2 voltage) and `i2_mA` (CH2 current)
  alongside the existing CH1 fields, captured from
  `t.last_v3v3_mV`/`t.last_i3v3_mA` on each tick (same source already used
  for the live Main-screen CH2 labels).
- Move `trend_buf` from a static internal-SRAM array to a
  `heap_caps_malloc(..., MALLOC_CAP_SPIRAM)` allocation sized for ~10–15 min
  at the current sample rate (concrete capacity picked during D5a
  implementation once per-sample byte size is finalized with the new
  fields).
- Keep `trendPushSample()`/`trendAt()`/ring-index helpers; thread the new
  fields through.
- Add a second pair of `lv_chart_series_t*` (CH2 V, CH2 I) to the existing
  two chart objects.
- Chart layout: keep **two stacked chart areas** (V pair sharing the voltage
  chart, I pair sharing the current chart) rather than overlaying all 4
  traces on one axis, since V and I use very different scales/units. This
  matches Rev 1's recommendation and is treated as decided unless raised
  again during D5a review.
- For feed rates where the selected window exceeds real buffer depth (15
  min/div, 1 hr/div with an empty or partially-filled buffer), render the
  unfilled portion of the chart as blank rather than fabricating data —
  acceptance criteria below requires this be visually unambiguous (e.g. the
  trace simply doesn't extend into that region yet).

## 7. Acceptance criteria (this revision's scope)

- Graph screen visually resembles a strip chart recorder (bezel, grid, pens,
  scrolling behavior).
- 4 real channels (CH1 V/I, CH2 V/I) plotted with distinct colors and a
  legend showing live values, each independently shown/hidden.
- Feed rate adjustable across the full spec list (10 s/div through 1 hr/div);
  slow settings honestly show only as much real history as the ~10–15 min
  buffer holds, with no synthetic/fabricated data.
- No alarm markers, no relay behavior, no history scroll-back/pan, no bar
  gauges — these are out of scope, not bugs.
- CSV export tracked and extended separately in issue #84, not blocking this
  screen's PRs.
- D5c micro-views: CH1 and CH2 each show only their own channel, with
  separately scaled and unit-labelled V and A axes, readable flat data, no
  clipped peaks, truthful LIVE/STALE/DEMO/UNKNOWN/no-data treatment, visible
  Back navigation, and touch targets of at least 44x44 px. Bench evidence is
  recorded in the screens tracker.
