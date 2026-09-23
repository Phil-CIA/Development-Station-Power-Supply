# System Development Workflow

Status: active — this is the source of truth for how work moves through this
repo from now on. It replaces the ad-hoc "dated handoff doc" pattern used
during board bring-up.

## Why this changed

Board bring-up was single-threaded: one person, one bench, one branch
(`main`), progress tracked with dated `HANDOFF_*.md` files. That worked for
debugging one board at a time, but it doesn't scale to system development,
where firmware, the two display paths, and hardware revisions move in
parallel and need to be reviewed before they land on `main`.

Evidence this repo already outgrew the old pattern: `main` has dozens of
direct commits and no pull requests, and there are already several
unmerged local branches (`phil-cia-*`, `copilot/*`) with no record of why
they exist or whether they're still needed. See "Branch cleanup" below.

## Branching model

- `main` is always buildable and reflects the current bench-validated state.
- Do all new work on a branch, never commit directly to `main`.
- One branch per unit of work, scoped to a subsystem:
  - `firmware/<topic>` — ESP32 control firmware in `src/`
  - `display/<topic>` — CrowPanel or custom front-panel work
  - `hw/<topic>` — KiCad/hardware changes under `hardware/`
  - `docs/<topic>` — documentation-only changes
- Branch names are kebab-case and describe the change, not the date
  (`firmware/rail-telemetry-map`, not `2026-08-14-work`).
- Open a pull request into `main` when the branch is ready for review, even
  if you are the only reviewer. The PR description is where "what changed
  and why" now lives — not a new dated handoff file.
- Merge only when:
  1. The firmware/hardware claim in the PR has been bench-verified (or
     explicitly marked "not yet bench-tested" if merging design-only work).
  2. Any doc that the change makes stale has been updated in the same PR
     (README.md, `docs/display-project/README.md`, `docs/GPIO_PINOUT.md`,
     tracker docs, etc.).
- Delete the branch after merge.

### Minimal git cheat-sheet

```powershell
git checkout main
git pull
git checkout -b firmware/rail-telemetry-map

# ... make changes, test on bench ...
git add -A
git commit -m "Add per-rail telemetry mapping"
git push -u origin firmware/rail-telemetry-map
gh pr create --fill
# after review/approval:
gh pr merge --squash --delete-branch
```

## Where things live going forward

| Kind of content | Location |
|---|---|
| Current, still-true project status | `README.md` (top-level) and the per-topic README under `docs/` |
| Subsystem reference (pinouts, interface standards, trackers) | `docs/*.md`, kept updated in place — not re-created per session |
| One-off historical record of a completed investigation | `docs/handoff-archive/` (see below) |
| Design source | `hardware/`, `src/`, `crowpanel-43-bringup/`, `stm32-bluepill-bringup/` |

Stop creating new root-level `HANDOFF_*.md` / `*_SUMMARY.md` / `*_HANDOFF.md`
files. If a session produces a durable finding, put it in the relevant
tracker doc (e.g. `docs/REGULATOR_BOARD_CHANGE_TRACKER.md`,
`docs/USB_HUB_CHANGE_TRACKER.md`) or the PR description. If it's genuinely
just a point-in-time snapshot with no lasting value once merged, it belongs
in `docs/handoff-archive/`, not the repo root.

### Root-level doc cleanup (follow-on task, not done yet)

The repo root currently has ~10 dated handoff/summary files left over from
bring-up (`BREAKTHROUGH_ROOT_CAUSE_FOUND.md`, `HANDOFF_2026-07-28_*.md`,
`RB-011_CLOSURE_SUMMARY_2026-08-12.md`, etc.), while `docs/handoff-archive/`
already holds the earlier round of these. Recommended next step: move the
still-current ones into the matching tracker doc's content, then archive the
files themselves under `docs/handoff-archive/root-handoffs/`. Do this as its
own `docs/cleanup-root-handoffs` PR so it doesn't get mixed with feature
work — ask if you want this done now.

## Branch cleanup (existing unmerged branches)

Before adopting this workflow, decide what to do with each pre-existing
branch instead of letting them accumulate silently:

- `phil-cia-didactic-invention`, `phil-cia-psychic-eureka`,
  `phil-cia-reorganize-board-rev-docs`, `phil-cia-rev-c-bringup-docs-update`,
  `phil-cia-rev-c-bringup-plan`
- `copilot/channel3-boot-control-voltage-edits`,
  `copilot/worktree-2026-05-24T08-48-44`, `copilot/worktree-2026-05-24T19-33-35`,
  `copilot/worktree-2026-05-24T20-07-51`, `copilot/worktree-2026-05-28T07-25-02`
- `origin/copilot/organize-kicad-files-structure`,
  `origin/copilot/search-solar-generator-inverter`

For each: open it, diff against `main`, and either open a PR (if the content
is still wanted) or delete the branch (if it was superseded). Track this as
a one-time cleanup pass, not ongoing work.

## Firmware build check (recommended, not yet added)

There is currently no CI. Recommended minimal addition once the workflow
above is in place: a GitHub Actions workflow that runs
`pio run` for each PlatformIO environment in `platformio.ini` on every PR,
so a PR that doesn't compile is caught before merge. This is scoped as a
separate task — flag if you want it added next.

## Definition of done for a merged PR

- [ ] Builds (firmware) or passes ERC/DRC (hardware), or is explicitly
      marked WIP/design-only in the PR description.
- [ ] Bench-tested claim is stated true or false in the PR description —
      don't leave it implied.
- [ ] Any doc this change makes stale is updated in the same PR.
- [ ] No new root-level dated handoff file was added.
